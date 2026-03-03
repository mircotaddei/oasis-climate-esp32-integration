#ifndef HARDWARE_MANAGER_H
#define HARDWARE_MANAGER_H

#include <Arduino.h>
#include <vector>
#include "ConfigManager.h"
#include "../sensors/SensorDriver.h"
#include "../actuators/RelayDriver.h"


// --- HARDWARE MANAGER --------------------------------------------------------

class HardwareManager {
public:
    HardwareManager();
    void begin(ConfigManager* config);
    void update();
    
    // Accessors
    float getTemperature(); // Returns average or primary sensor temp
    bool getRelayState();
    void setRelayState(bool state);
    const std::vector<SensorDriver*>& getSensors() const;

private:
    std::vector<SensorDriver*> _sensors;
    RelayDriver* _relay;
    ConfigManager* _config;
};

#endif
