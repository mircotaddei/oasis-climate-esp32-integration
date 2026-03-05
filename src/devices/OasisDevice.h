#ifndef OASIS_DEVICE_H
#define OASIS_DEVICE_H

#include <Arduino.h>
#include <ArduinoJson.h>

// --- DEVICE TYPES ------------------------------------------------------------

enum OasisDeviceType {
    DEVICE_TYPE_UNKNOWN,
    DEVICE_TYPE_SENSOR_DALLAS,
    DEVICE_TYPE_SENSOR_DHT,
    DEVICE_TYPE_ACTUATOR_RELAY,
    DEVICE_TYPE_ACTUATOR_OPENTHERM
};

// --- BASE INTERFACE ----------------------------------------------------------

class OasisDevice {
public:
    virtual ~OasisDevice() {}

    // Lifecycle
    virtual void begin() = 0;
    virtual void update() = 0; // Non-blocking loop

    // Identity
    virtual OasisDeviceType getType() const = 0;
    virtual const char* getLocalId() const = 0; // Hardware ID (e.g., ROM code or Pin)
    
    virtual const char* getGlobalId() const = 0; // Backend ID
    virtual void setGlobalId(const char* globalId) = 0;

    // State
    virtual bool isActive() const = 0;
    virtual void setActive(bool state) = 0;
    virtual bool isConnected() const = 0;

    // Configuration (Meta)
    // Populates the JSON object with the device's current configuration
    virtual void populateMeta(JsonObject& meta) const = 0;
    
    // Applies configuration received from the backend or NVS
    virtual void applyMeta(JsonObjectConst meta) = 0;
};

#endif // OASIS_DEVICE_H