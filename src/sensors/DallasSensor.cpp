#include "DallasSensor.h"
#include "../managers/ConfigManager.h" 


// --- CONSTRUCTOR ------------------------------------------------------------

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
    
    _offset = 0.0;
    _errorCount = 0;
    _filteredTemp = NAN;
    _lastRawTemp = NAN;
    _consecutiveOutliers = 0;
}


// --- BEGIN ------------------------------------------------------------------

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


// --- FILTER LOGIC (ANTI-SPIKE & EMA) ----------------------------------------

void DallasSensor::applyFilter(float rawTemp) {
    // 1. First valid reading initialization
    if (isnan(_filteredTemp)) {
        _filteredTemp = rawTemp;
        _lastRawTemp = rawTemp;
        _consecutiveOutliers = 0;
        return;
    }

    // 2. Outlier Rejection (Spike Filter)
    float diff = abs(rawTemp - _lastRawTemp);
    if (diff > MAX_TEMP_JUMP) {
        _consecutiveOutliers++;
        DEBUG_PRINTLN("[SENSOR] WARNING: Temp spike detected (", rawTemp, " C). Ignoring.");
        
        if (_consecutiveOutliers >= MAX_OUTLIERS) {
            DEBUG_PRINTLN("[SENSOR] Spike persisted. Accepting new baseline.");
            _filteredTemp = rawTemp; // Reset filter to new reality
            _lastRawTemp = rawTemp;
            _consecutiveOutliers = 0;
        }
        return; // Ignore this reading for the filter
    }

    // 3. Exponential Moving Average (Smoothing)
    _consecutiveOutliers = 0;
    _lastRawTemp = rawTemp;
    _filteredTemp = (rawTemp * EMA_ALPHA) + (_filteredTemp * (1.0 - EMA_ALPHA));
}


// --- UPDATE -----------------------------------------------------------------

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
        
        if (tempC != DEVICE_DISCONNECTED_C && tempC != 85.0) { // 85.0 is a common power-on error value
            if (!_isConnected) {
                DEBUG_PRINTLN("[SENSOR] ", _id, " RECONNECTED! Starting warmup...");
                _isConnected = true;
                _reconnectMillis = currentMillis; 
                _filteredTemp = NAN; // Reset filter on reconnect
            }
            
            // Only apply filter if warmup is complete
            if (millis() - _reconnectMillis >= getWarmupTimeMs()) {
                applyFilter(tempC);
            }
            
        } else {
            _errorCount++; // Increment diagnostic counter
            if (_isConnected) {
                DEBUG_PRINTLN("[SENSOR] ERROR: ", _id, " DISCONNECTED or CRC FAIL!");
                _isConnected = false;
            }
            _filteredTemp = NAN; 
        }
        _requestPending = false;
    }
}


// --- GET WARMUP TIME --------------------------------------------------------

unsigned long DallasSensor::getWarmupTimeMs() {
    return 2000; 
}


// --- GET TEMPERATURE --------------------------------------------------------

float DallasSensor::getTemperature() {
    if (_isConnected && !isnan(_filteredTemp)) {
        return _filteredTemp + _offset; // Apply calibration offset
    }
    return NAN;
}


// --- GET HUMIDITY -----------------------------------------------------------

float DallasSensor::getHumidity() {
    return NAN;
}


// --- GET ID -----------------------------------------------------------------

const char* DallasSensor::getId() {
    return _id;
}


// --- GET GLOBAL ID ----------------------------------------------------------

const char* DallasSensor::getGlobalId() {
    return _globalId;
}


// --- SET GLOBAL ID -----------------------------------------------------------

void DallasSensor::setGlobalId(const char* globalId) {
    strlcpy(_globalId, globalId, sizeof(_globalId));
}


// --- GET TYPE ----------------------------------------------------------------

SensorType DallasSensor::getType() {
    return SENSOR_TYPE_DALLAS;
}


// --- IS ACTIVE ---------------------------------------------------------------

bool DallasSensor::isActive() {
    return _isActive;
}


// --- SET ACTIVE --------------------------------------------------------------

void DallasSensor::setActive(bool state) {
    _isActive = state;
}


// --- IS CONNECTED ------------------------------------------------------------

bool DallasSensor::isConnected() {
    return _isConnected;
}


// --- CALIBRATION & DIAGNOSTICS -----------------------------------------------

void DallasSensor::setOffset(float offset) {
    _offset = offset;
}


// --- GET OFFSET --------------------------------------------------------------

float DallasSensor::getOffset() {
    return _offset;
}


// --- GET ERROR COUNT ---------------------------------------------------------

unsigned int DallasSensor::getErrorCount() {
    return _errorCount;
}


// --- CLEAR ERROR COUNT -------------------------------------------------------

void DallasSensor::clearErrorCount() {
    _errorCount = 0;
}
