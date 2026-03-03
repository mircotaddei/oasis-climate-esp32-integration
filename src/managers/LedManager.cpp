#include "LedManager.h"


// --- CONSTRUCTOR -------------------------------------------------------------

LedManager::LedManager(int pin, bool activeHigh) {
    _pin = pin;
    _activeHigh = activeHigh;
    _currentState = LED_OFF;
    _lastUpdateMillis = 0;
    _blinkCount = 0;
    _isOn = false;
}


// --- BEGIN -------------------------------------------------------------------

void LedManager::begin() {
    pinMode(_pin, OUTPUT);
    turnOff();
}


// --- SET STATE ---------------------------------------------------------------

void LedManager::setState(LedState newState) {
    if (_currentState == newState) return; // No change
    
    _currentState = newState;
    _blinkCount = 0; // Reset sequence
    _lastUpdateMillis = millis();
    
    // Immediate action for static states
    if (_currentState == LED_ON) {
        turnOn();
    } else if (_currentState == LED_OFF) {
        turnOff();
    } else {
        // Start a blink sequence immediately
        turnOn();
    }
}


// --- UPDATE (NON-BLOCKING) ---------------------------------------------------

void LedManager::update() {
    if (_currentState == LED_ON || _currentState == LED_OFF) {
        return; // Nothing to animate
    }

    unsigned long currentMillis = millis();
    unsigned long elapsed = currentMillis - _lastUpdateMillis;

    if (_currentState == LED_FAST_BLINK) {
        if (elapsed >= FAST_BLINK_TIME) {
            _lastUpdateMillis = currentMillis;
            if (_isOn) turnOff();
            else turnOn();
        }
        return;
    }

    // Logic for numbered blinks (1, 2, 3, 4)
    int targetBlinks = 0;
    switch (_currentState) {
        case LED_BLINK_1: targetBlinks = 1; break;
        case LED_BLINK_2: targetBlinks = 2; break;
        case LED_BLINK_3: targetBlinks = 3; break;
        case LED_BLINK_4: targetBlinks = 4; break;
        default: break;
    }

    if (_isOn) {
        // Currently ON, waiting to turn OFF
        if (elapsed >= BLINK_ON_TIME) {
            _lastUpdateMillis = currentMillis;
            turnOff();
            _blinkCount++;
        }
    } else {
        // Currently OFF, waiting to turn ON or PAUSE
        if (_blinkCount >= targetBlinks) {
            // Sequence finished, wait for long pause
            if (elapsed >= PAUSE_TIME) {
                _lastUpdateMillis = currentMillis;
                _blinkCount = 0; // Restart sequence
                turnOn();
            }
        } else {
            // Short pause between blinks
            if (elapsed >= BLINK_OFF_TIME) {
                _lastUpdateMillis = currentMillis;
                turnOn();
            }
        }
    }
}


// --- HARDWARE CONTROL --------------------------------------------------------

void LedManager::turnOn() {
    _isOn = true;
    digitalWrite(_pin, _activeHigh ? HIGH : LOW);
}

void LedManager::turnOff() {
    _isOn = false;
    digitalWrite(_pin, _activeHigh ? LOW : HIGH);
}