#ifndef RELAY_DRIVER_H
#define RELAY_DRIVER_H

#include <Arduino.h>


// --- RELAY DRIVER ------------------------------------------------------------

class RelayDriver {
public:
    RelayDriver(int pin, bool activeHigh = true);
    void begin();
    void update(); // Check for safety timeout
    
    void turnOn();
    void turnOff();
    bool isOn();
    
    void setSafetyTimeout(unsigned long timeoutMs); // Auto-off if no command received

private:
    int _pin;
    bool _activeHigh;
    bool _state;
    unsigned long _lastCommandMillis;
    unsigned long _safetyTimeoutMs;
};

#endif
