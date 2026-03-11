#include "RelayDriver.h"
#include "../managers/ConfigManager.h" 


// --- CONSTRUCTOR -------------------------------------------------------------

RelayDriver::RelayDriver(int pin, bool activeHigh) {
    _pin = pin;
    _activeHigh = activeHigh;
    _state = false;
    _lastCommandMillis = 0;
    _isActive = true; // Actuators are usually active by default
    _globalId[0] = '\0';
    
    // Generate a local ID based on the pin (e.g., "relay_5")
    sprintf(_localId, "relay_%d", _pin);

    // Load Defaults
    _modulationThreshold = DEFAULT_MODULATION_THRESHOLD;
    _safetyTimeoutMs = DEFAULT_SAFETY_TIMEOUT_MS;
    strlcpy(_sensorType, "relay_state", sizeof(_sensorType)); // Default API type
    
    _reportDelta = 0.0; // Relays are event-driven, delta doesn't apply
}


// --- BEGIN -------------------------------------------------------------------

void RelayDriver::begin() {
    pinMode(_pin, OUTPUT);
    turnOff(); // Default safe state
    DEBUG_PRINTLN("[RELAY] Initialized on pin ", _pin);
}


// --- UPDATE ------------------------------------------------------------------

void RelayDriver::update() {
    if (!_isActive) return;

    if (_state && _safetyTimeoutMs > 0) {
        if (millis() - _lastCommandMillis > _safetyTimeoutMs) {
            DEBUG_PRINTLN("[RELAY] SAFETY TIMEOUT REACHED! Turning OFF.");
            turnOff();
        }
    }
}


// --- SET MODULATION ----------------------------------------------------------

void RelayDriver::setModulation(float level) {
    if (!_isActive) return;

    _lastCommandMillis = millis(); // Reset safety timer

    if (level >= _modulationThreshold) {
        if (!_state) {
            DEBUG_PRINTLN("[RELAY] Turning ON (Modulation: ", level, ")");
            turnOn();
        }
    } else {
        if (_state) {
            DEBUG_PRINTLN("[RELAY] Turning OFF (Modulation: ", level, ")");
            turnOff();
        }
    }
}


// --- GET CURRENT STATE -------------------------------------------------------

float RelayDriver::getCurrentState() const {
    return _state ? 1.0 : 0.0;
}


// --- TURN ON -----------------------------------------------------------------

void RelayDriver::turnOn() {
    _state = true;
    digitalWrite(_pin, _activeHigh ? HIGH : LOW);
    emitTelemetry(1.0);
}


// --- TURN OFF ----------------------------------------------------------------

void RelayDriver::turnOff() {
    _state = false;
    digitalWrite(_pin, _activeHigh ? LOW : HIGH);
    emitTelemetry(0.0);
}


// --- UNIFIED TELEMETRY INTERFACE ---------------------------------------------

float RelayDriver::getTelemetryValue() const {
    return getCurrentState(); // 1.0 for ON, 0.0 for OFF
}


// --- POPULATE META -----------------------------------------------------------

void RelayDriver::populateMeta(JsonObject& meta) const {
    OasisDevice::populateMeta(meta);
    
    meta["pin"] = _pin;
    meta["active_high"] = _activeHigh;
    meta["modulation_threshold"] = _modulationThreshold;
    meta["safety_timeout_ms"] = _safetyTimeoutMs;
}


// --- APPLY META --------------------------------------------------------------

void RelayDriver::applyMeta(JsonObjectConst meta) {
    OasisDevice::applyMeta(meta);
    
    if (meta["modulation_threshold"].is<float>()) {
        _modulationThreshold = meta["modulation_threshold"].as<float>();
    }
    if (meta["safety_timeout_ms"].is<unsigned long>()) {
        _safetyTimeoutMs = meta["safety_timeout_ms"].as<unsigned long>();
    }
    DEBUG_PRINTLN("[RELAY] Meta Applied. Threshold: ", _modulationThreshold);
}


// --- GETTERS & SETTERS -------------------------------------------------------

OasisDeviceType RelayDriver::getType() const { return DEVICE_TYPE_ACTUATOR_RELAY; }
const char* RelayDriver::getLocalId() const { return _localId; }
const char* RelayDriver::getGlobalId() const { return _globalId; }
void RelayDriver::setGlobalId(const char* globalId) { strlcpy(_globalId, globalId, sizeof(_globalId)); }
bool RelayDriver::isActive() const { return _isActive; }
void RelayDriver::setActive(bool state) { 
    _isActive = state; 
    if (!state) turnOff(); // Force off if deactivated
}
bool RelayDriver::isConnected() const { return true; } // Relays are assumed always connected