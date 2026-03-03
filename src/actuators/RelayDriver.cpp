#include "RelayDriver.h"
#include "../managers/ConfigManager.h"


// --- CONSTRUCTOR -------------------------------------------------------------

RelayDriver::RelayDriver(int pin, bool activeHigh) {
    _pin = pin;
    _activeHigh = activeHigh;
    _state = false;
    _lastCommandMillis = 0;
    _safetyTimeoutMs = 0; // 0 = disabled
}


// --- BEGIN -------------------------------------------------------------------

void RelayDriver::begin() {
    pinMode(_pin, OUTPUT);
    turnOff(); // Default safe state
}


// --- UPDATE ------------------------------------------------------------------

void RelayDriver::update() {
    if (_state && _safetyTimeoutMs > 0) {
        if (millis() - _lastCommandMillis > _safetyTimeoutMs) {
            DEBUG_PRINTLN("SAFETY: Relay timeout reached. Turning OFF.");
            turnOff();
        }
    }
}


// --- TURN ON -----------------------------------------------------------------

void RelayDriver::turnOn() {
    _state = true;
    _lastCommandMillis = millis();
    digitalWrite(_pin, _activeHigh ? HIGH : LOW);
    DEBUG_PRINTLN("Relay turned ON");
}


// --- TURN OFF ----------------------------------------------------------------

void RelayDriver::turnOff() {
    _state = false;
    digitalWrite(_pin, _activeHigh ? LOW : HIGH);
    DEBUG_PRINTLN("Relay turned OFF");
}


// --- IS ON -------------------------------------------------------------------

bool RelayDriver::isOn() {
    DEBUG_PRINTLN("Relay is ", _state ? "ON" : "OFF");
    return _state;
}


// --- SET SAFETY TIMEOUT ------------------------------------------------------

void RelayDriver::setSafetyTimeout(unsigned long timeoutMs) {
    _safetyTimeoutMs = timeoutMs;
}