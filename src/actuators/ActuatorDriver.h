#ifndef ACTUATOR_DRIVER_H
#define ACTUATOR_DRIVER_H

#include "../devices/OasisDevice.h"

// --- ACTUATOR DRIVER INTERFACE -----------------------------------------------

class ActuatorDriver : public OasisDevice {
public:
    virtual ~ActuatorDriver() {}

    // Actuator specific methods
    
    // Set the target modulation level (0.0 to 1.0)
    // For a simple relay: > threshold = ON, else OFF
    virtual void setModulation(float level) = 0;
    
    // Returns the current physical state (e.g., 1.0 if relay is closed)
    virtual float getCurrentState() const = 0;
};

#endif // ACTUATOR_DRIVER_H