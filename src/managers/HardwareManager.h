#ifndef HARDWARE_MANAGER_H
#define HARDWARE_MANAGER_H

#include <Arduino.h>
#include <vector>
#include "../build_config.h"
#include "ConfigManager.h"
#include "../devices/OasisDevice.h"
#include "../sensors/SensorDriver.h"
#include "../actuators/ActuatorDriver.h"

// --- HARDWARE MANAGER --------------------------------------------------------

class HardwareManager {
public:
    HardwareManager();
    ~HardwareManager();
    
    void begin(ConfigManager* config);
    void update();
    
    // Unified Device Access
    const std::vector<OasisDevice*>& getAllDevices() const;
    OasisDevice* getDeviceByLocalId(const char* localId) const;
    OasisDevice* getDeviceByGlobalId(const char* globalId) const;

    // Specific Accessors (Convenience methods for main loop)
    float getTemperature(); 
    void setRelayModulation(float level);
    float getRelayModulation() const;

    // Telemetry Callback
    void setTelemetryCallback(TelemetryCallback cb);

private:
    std::vector<OasisDevice*> _devices;
    ConfigManager* _config;
    
    // Pointers to specific primary devices for quick access
    ActuatorDriver* _primaryRelay;
};

#endif // HARDWARE_MANAGER_H