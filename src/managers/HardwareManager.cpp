#include "HardwareManager.h"
#include "../sensors/DallasSensor.h"


// --- CONSTRUCTOR -------------------------------------------------------------

HardwareManager::HardwareManager() {
    _relay = nullptr;
    _config = nullptr;
}


// --- BEGIN -------------------------------------------------------------------

void HardwareManager::begin(ConfigManager* config) {
    _config = config;
    
    // 1. Initialize Relay
    int relayPin = config->getPin("relay", 5); // Default GPIO 5
    _relay = new RelayDriver(relayPin);
    _relay->begin();
    _relay->setSafetyTimeout(30 * 60 * 1000); // 30 min default safety timeout
    
    DEBUG_PRINTLN("HardwareManager: Relay initialized on pin ", relayPin);

    // 2. Initialize Sensors (Dallas DS18B20)
    int oneWirePin = config->getPin("onewire_bus", 4); // Default GPIO 4
    DallasSensor* dallas = new DallasSensor(oneWirePin);
    dallas->begin();
    
    // Load saved state (Global ID, Active status, Offset) from NVS
    if (strlen(dallas->getId()) > 0) {
        char savedGlobalId[64] = "";
        bool savedIsActive = false;
        float savedOffset = 0.0;
        
        if (config->loadSensorState(dallas->getId(), savedGlobalId, sizeof(savedGlobalId), &savedIsActive, &savedOffset)) {
            dallas->setGlobalId(savedGlobalId);
            dallas->setActive(savedIsActive);
            dallas->setOffset(savedOffset);
            DEBUG_PRINTLN("HardwareManager: Restored sensor state. GlobalID: ", savedGlobalId, " Active: ", savedIsActive, " Offset: ", savedOffset);
        }
    }
    
    _sensors.push_back(dallas);
    
    DEBUG_PRINTLN("HardwareManager: OneWire bus initialized on pin ", oneWirePin);

    // Trigger the first async read immediately so data is ready sooner
    update(); 
}


// --- UPDATE ------------------------------------------------------------------

void HardwareManager::update() {
    // Update Relay Safety Logic
    if (_relay) _relay->update();

    // Update Sensors (Async Read)
    for (auto sensor : _sensors) {
        sensor->update();
    }
}


// --- GET TEMPERATURE ---------------------------------------------------------

float HardwareManager::getTemperature() {
    // Simple strategy: return the first valid reading found
    for (auto sensor : _sensors) {
        float t = sensor->getTemperature();
        if (!isnan(t)) return t;
    }
    return NAN;
}


// --- GET RELAY STATE ---------------------------------------------------------

bool HardwareManager::getRelayState() {
    if (_relay) return _relay->isOn();
    return false;
}


// --- SET RELAY STATE ---------------------------------------------------------

void HardwareManager::setRelayState(bool state) {
    if (_relay) {
        if (state) _relay->turnOn();
        else _relay->turnOff();
    }
}


// --- GET SENSORS -------------------------------------------------------------

const std::vector<SensorDriver*>& HardwareManager::getSensors() const {
    return _sensors;
}