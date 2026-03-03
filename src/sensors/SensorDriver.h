#ifndef SENSOR_DRIVER_H
#define SENSOR_DRIVER_H

#include <Arduino.h>


// --- SENSOR TYPE -------------------------------------------------------------

enum SensorType {
    SENSOR_TYPE_UNKNOWN,
    SENSOR_TYPE_DALLAS,
    SENSOR_TYPE_DHT
};


// --- SENSOR DRIVER -----------------------------------------------------------

class SensorDriver {
public:
    virtual void begin() = 0;
    virtual void update() = 0;
    virtual float getTemperature() = 0;
    virtual float getHumidity() = 0;
    
    // Hardware ID (e.g., ROM code)
    virtual const char* getId() = 0; 
    
    // Global ID assigned by Backend
    virtual const char* getGlobalId() = 0;
    virtual void setGlobalId(const char* globalId) = 0;

    virtual SensorType getType() = 0;
    
    // Active state management
    virtual bool isActive() = 0;
    virtual void setActive(bool state) = 0;

    // Physical connection state
    virtual bool isConnected() = 0;

    // Time required to stabilize after power-on or reconnection
    virtual unsigned long getWarmupTimeMs() = 0;

    // Calibration Offset
    virtual void setOffset(float offset) = 0;
    virtual float getOffset() = 0;

    // Diagnostics
    virtual unsigned int getErrorCount() = 0;
    virtual void clearErrorCount() = 0;
};

#endif