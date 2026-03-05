#include "HardwareManager.h"

// Conditionally include drivers based on build_config.h
#ifdef ENABLE_BOILER_RELAY
    #include "../actuators/RelayDriver.h"
#endif
#ifdef ENABLE_DALLAS_SENSOR
    #include "../sensors/DallasSensor.h"
#endif


// --- CONSTRUCTOR & DESTRUCTOR ------------------------------------------------

HardwareManager::HardwareManager() {
    _config = nullptr;
    _primaryRelay = nullptr;
}

HardwareManager::~HardwareManager() {
    for (auto device : _devices) {
        delete device;
    }
    _devices.clear();
}


// --- BEGIN -------------------------------------------------------------------

void HardwareManager::begin(ConfigManager* config) {
    _config = config;
    DEBUG_PRINTLN("--- [HW] Initializing Hardware Modules ---");

    #ifdef ENABLE_BOILER_RELAY
        RelayDriver* relay = new RelayDriver(); 
        _devices.push_back(relay);
        _primaryRelay = relay;
        DEBUG_PRINTLN("[HW] Boiler Relay module enabled.");
    #endif

    #ifdef ENABLE_DALLAS_SENSOR
        DallasSensor* dallas = new DallasSensor(); 
        _devices.push_back(dallas);
        DEBUG_PRINTLN("[HW] Dallas 1-Wire module enabled.");
    #endif

    for (auto device : _devices) {
        device->begin();
        
        if (strlen(device->getLocalId()) > 0) {
            char savedGlobalId[64] = "";
            bool savedIsActive = false;
            
            // Modern syntax for temporary JsonDocument
            JsonDocument tempMetaDoc;
            JsonObject metaObj = tempMetaDoc.to<JsonObject>();
            
            if (config->loadDeviceState(device->getLocalId(), savedGlobalId, sizeof(savedGlobalId), &savedIsActive, metaObj)) {
                device->setGlobalId(savedGlobalId);
                device->setActive(savedIsActive);
                device->applyMeta(metaObj); 
                DEBUG_PRINTLN("[HW] Restored state for: ", device->getLocalId());
            }
        }
    }

    update(); 
    DEBUG_PRINTLN("--- [HW] Initialization Complete ---");
}


// --- UPDATE ------------------------------------------------------------------

void HardwareManager::update() {
    for (auto device : _devices) {
        device->update();
    }
}


// --- GET ALL DEVICES ---------------------------------------------------------

const std::vector<OasisDevice*>& HardwareManager::getAllDevices() const {
    return _devices;
}


// --- GET DEVICE BY LOCAL ID --------------------------------------------------

OasisDevice* HardwareManager::getDeviceByLocalId(const char* localId) const {
    for (auto device : _devices) {
        if (strcmp(device->getLocalId(), localId) == 0) {
            return device;
        }
    }
    return nullptr;
}


// --- GET DEVICE BY GLOBAL ID -------------------------------------------------

OasisDevice* HardwareManager::getDeviceByGlobalId(const char* globalId) const {
    for (auto device : _devices) {
        if (strcmp(device->getGlobalId(), globalId) == 0) {
            return device;
        }
    }
    return nullptr;
}


// --- GET TEMPERATURE ---------------------------------------------------------

float HardwareManager::getTemperature() {
    // Iterate through all devices, find the first active Dallas sensor
    for (auto device : _devices) {
        if (device->getType() == DEVICE_TYPE_SENSOR_DALLAS && device->isActive()) {
            SensorDriver* sensor = static_cast<SensorDriver*>(device);
            float t = sensor->getTemperature();
            if (!isnan(t)) return t;
        }
    }
    return NAN;
}


// --- SET RELAY MODULATION ----------------------------------------------------

void HardwareManager::setRelayModulation(float level) {
    if (_primaryRelay) {
        _primaryRelay->setModulation(level);
    }
}


// --- GET RELAY MODULATION ----------------------------------------------------
float HardwareManager::getRelayModulation() const {
    if (_primaryRelay) {
        return _primaryRelay->getCurrentState();
    }
    return 0.0;
}