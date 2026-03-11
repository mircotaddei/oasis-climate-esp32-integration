#include "DallasSensor.h"
#include "../managers/ConfigManager.h" 


// --- CONSTRUCTOR & DESTRUCTOR ------------------------------------------------

DallasSensor::DallasSensor(int pin) {
    _pin = pin;
    _oneWire = nullptr;
    _sensors = nullptr;
    _lastRequestMillis = 0;
    _requestPending = false;
    _id[0] = '\0';
    _globalId[0] = '\0'; 
    _isActive = false; 
    _isConnected = false; 
    _reconnectMillis = 0; 
    
    // Load Defaults
    _offset = DEFAULT_DALLAS_OFFSET;
    _alpha = DEFAULT_DALLAS_ALPHA;
    
    // Set Base Class Defaults
    strlcpy(_sensorType, "temp_in", sizeof(_sensorType));
    _reportDelta = 0.1; // Default for temperature
    
    _errorCount = 0;
    _filteredTemp = NAN;
    _lastRawTemp = NAN;
    _consecutiveOutliers = 0;
}

DallasSensor::~DallasSensor() {
    if (_oneWire) delete _oneWire;
    if (_sensors) delete _sensors;
}


// --- BEGIN -------------------------------------------------------------------

void DallasSensor::begin() {
    if (_oneWire) delete _oneWire;
    if (_sensors) delete _sensors;

    _oneWire = new OneWire(_pin);
    _sensors = new DallasTemperature(_oneWire);
    
    DEBUG_PRINTLN("[SENSOR] Initializing Dallas on pin ", _pin);
    _sensors->begin();
    
    int deviceCount = _sensors->getDeviceCount();
    DEBUG_PRINTLN("[SENSOR] Found ", deviceCount, " devices on bus.");

    if (deviceCount > 0 && _sensors->getAddress(_deviceAddress, 0)) {
        sprintf(_id, "%02X%02X%02X%02X%02X%02X%02X%02X",
                _deviceAddress[0], _deviceAddress[1], _deviceAddress[2], _deviceAddress[3],
                _deviceAddress[4], _deviceAddress[5], _deviceAddress[6], _deviceAddress[7]);
        
        _sensors->setWaitForConversion(false); 
        DEBUG_PRINTLN("[SENSOR] Primary sensor ID: ", _id);
        _isConnected = true; 
        _reconnectMillis = millis(); 
    } else {
        DEBUG_PRINTLN("[SENSOR] ERROR: No DS18B20 found on pin ", _pin);
        _isConnected = false;
    }
}


// --- UPDATE ------------------------------------------------------------------

void DallasSensor::update() {
    if (_id[0] == '\0' || !_isActive) return; 

    unsigned long currentMillis = millis();
    unsigned long pollInterval = isnan(_filteredTemp) ? 2000 : 10000;

    if (!_requestPending && (currentMillis - _lastRequestMillis > pollInterval || _lastRequestMillis == 0)) {
        _sensors->requestTemperatures();
        _lastRequestMillis = currentMillis;
        _requestPending = true;
    }

    if (_requestPending && (currentMillis - _lastRequestMillis > 1000)) {
        float tempC = _sensors->getTempC(_deviceAddress);
        
        if (tempC != DEVICE_DISCONNECTED_C && tempC != 85.0) { 
            if (!_isConnected) {
                DEBUG_PRINTLN("[SENSOR] ", _id, " RECONNECTED! Starting warmup...");
                _isConnected = true;
                _reconnectMillis = currentMillis; 
                _filteredTemp = NAN; 
            }
            
            if (millis() - _reconnectMillis >= getWarmupTimeMs()) {
                applyFilter(tempC);
            }
            
        } else {
            _errorCount++; 
            if (_isConnected) {
                DEBUG_PRINTLN("[SENSOR] ERROR: ", _id, " DISCONNECTED or CRC FAIL!");
                _isConnected = false;
            }
            _filteredTemp = NAN; 
        }
        _requestPending = false;
    }
}


// --- FILTER LOGIC (ANTI-SPIKE & EMA) ----------------------------------------

void DallasSensor::applyFilter(float rawTemp) {
    if (isnan(_filteredTemp)) {
        _filteredTemp = rawTemp;
        _lastRawTemp = rawTemp;
        _consecutiveOutliers = 0;
        return;
    }

    float diff = abs(rawTemp - _lastRawTemp);
    if (diff > MAX_TEMP_JUMP) {
        _consecutiveOutliers++;
        if (_consecutiveOutliers >= MAX_OUTLIERS) {
            _filteredTemp = rawTemp; 
            _lastRawTemp = rawTemp;
            _consecutiveOutliers = 0;
        }
        return; 
    }

    _consecutiveOutliers = 0;
    _lastRawTemp = rawTemp;
    _filteredTemp = (rawTemp * _alpha) + (_filteredTemp * (1.0 - _alpha));
}


// --- UNIFIED TELEMETRY INTERFACE ---------------------------------------------

float DallasSensor::getTelemetryValue() const {
    return getTemperature();
}


// --- META CONFIGURATION ------------------------------------------------------


// --- POPULATE META -----------------------------------------------------------

void DallasSensor::populateMeta(JsonObject& meta) const {
    OasisDevice::populateMeta(meta);
    
    meta["pin"] = _pin;
    meta["offset"] = _offset;
    meta["alpha"] = _alpha;
}


// --- APPLY META --------------------------------------------------------------

void DallasSensor::applyMeta(JsonObjectConst meta) {
    OasisDevice::applyMeta(meta);
    
    if (meta["offset"].is<float>()) {
        _offset = meta["offset"].as<float>();
    }
    if (meta["alpha"].is<float>()) {
        _alpha = meta["alpha"].as<float>();
        if (_alpha < 0.01) _alpha = 0.01;
        if (_alpha > 1.0) _alpha = 1.0;
    }
    DEBUG_PRINTLN("[SENSOR] ", _id, " Meta Applied. Offset: ", _offset);
}


// --- DIAGNOSTICS -------------------------------------------------------------

void DallasSensor::populateDiagnostics(JsonObject& metrics, JsonObject& tags) {
    metrics["error_count"] = _errorCount;
    tags["status"] = _isConnected ? "connected" : "disconnected";
    
    // Reset error count after reporting
    _errorCount = 0; 
}


// --- GETTERS & SETTERS -------------------------------------------------------

float DallasSensor::getTemperature() const {
    if (_isConnected && !isnan(_filteredTemp)) {
        return _filteredTemp + _offset; 
    }
    return NAN;
}

float DallasSensor::getHumidity() const { return NAN; }
const char* DallasSensor::getLocalId() const { return _id; }
const char* DallasSensor::getGlobalId() const { return _globalId; }
void DallasSensor::setGlobalId(const char* globalId) { strlcpy(_globalId, globalId, sizeof(_globalId)); }
OasisDeviceType DallasSensor::getType() const { return DEVICE_TYPE_SENSOR_DALLAS; }
bool DallasSensor::isActive() const { return _isActive; }
void DallasSensor::setActive(bool state) { _isActive = state; }
bool DallasSensor::isConnected() const { return _isConnected; }
unsigned long DallasSensor::getWarmupTimeMs() const { return 2000; }
unsigned int DallasSensor::getErrorCount() const { return _errorCount; }
void DallasSensor::clearErrorCount() { _errorCount = 0; }