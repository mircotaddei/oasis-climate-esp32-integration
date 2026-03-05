#ifndef SENSOR_DRIVER_H
#define SENSOR_DRIVER_H

#include "../devices/OasisDevice.h"


// --- SENSOR DRIVER INTERFACE -------------------------------------------------

class SensorDriver : public OasisDevice {
public:
    virtual ~SensorDriver() {}

    // Sensor specific methods
    virtual float getTemperature() = 0;
    virtual float getHumidity() = 0;
    
    // Time required to stabilize after power-on or reconnection
    virtual unsigned long getWarmupTimeMs() const = 0;

    // Diagnostics
    virtual unsigned int getErrorCount() const = 0;
    virtual void clearErrorCount() = 0;
};

#endif // SENSOR_DRIVER_H