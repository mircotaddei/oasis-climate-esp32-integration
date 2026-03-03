#ifndef LED_MANAGER_H
#define LED_MANAGER_H

#include <Arduino.h>


// --- LED STATES --------------------------------------------------------------

enum LedState {
    LED_OFF,
    LED_ON,
    LED_BLINK_1, // 1 blink, pause (Critical Error)
    LED_BLINK_2, // 2 blinks, pause (Claiming)
    LED_BLINK_3, // 3 blinks, pause (Failsafe/Offline)
    LED_BLINK_4, // 4 blinks, pause (Captive Portal)
    LED_FAST_BLINK // Continuous fast blink (Provisioning)
};


// --- LED MANAGER -------------------------------------------------------------

class LedManager {
public:
    // Constructor defaults to the standard ESP32 built-in LED (GPIO 2)
    LedManager(int pin = 2, bool activeHigh = true);
    
    void begin();
    void update(); // Must be called in the main loop
    void setState(LedState newState);

private:
    int _pin;
    bool _activeHigh;
    LedState _currentState;
    
    // Timing variables
    unsigned long _lastUpdateMillis;
    int _blinkCount;
    bool _isOn;
    
    // Constants for blink timing (milliseconds)
    const unsigned long BLINK_ON_TIME = 200;
    const unsigned long BLINK_OFF_TIME = 200;
    const unsigned long PAUSE_TIME = 1500;
    const unsigned long FAST_BLINK_TIME = 100;

    void turnOn();
    void turnOff();
};

#endif