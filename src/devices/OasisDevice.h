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

class OasisDevice; // Forward declaration

// --- TELEMETRY CALLBACK ------------------------------------------------------

typedef void (*TelemetryCallback)(OasisDevice* device, float value);


// --- OASIS DEVICE ------------------------------------------------------------

class OasisDevice {
public:
    OasisDevice() : _telemetryCallback(nullptr) {}
    virtual ~OasisDevice() {}

    // Lifecycle
    virtual void begin() = 0;
    virtual void update() = 0; 

    // Identity
    virtual OasisDeviceType getType() const = 0;
    virtual const char* getLocalId() const = 0; 
    virtual const char* getGlobalId() const = 0; 
    virtual void setGlobalId(const char* globalId) = 0;

    // State
    virtual bool isActive() const = 0;
    virtual void setActive(bool state) = 0;
    virtual bool isConnected() const = 0;

    // Unified Telemetry Interface
    virtual const char* getSensorType() const = 0; 
    virtual float getTelemetryValue() const = 0; 

    // Configuration (Meta)
    virtual void populateMeta(JsonObject& meta) const = 0;
    virtual void applyMeta(JsonObjectConst meta) = 0;

    // Event-Driven Telemetry
    void setTelemetryCallback(TelemetryCallback cb) {
        _telemetryCallback = cb;
    }

protected:
    // Helper for child classes to emit events when their state changes
    void emitTelemetry(float value) {
        if (_telemetryCallback && isActive() && isConnected()) {
            _telemetryCallback(this, value);
        }
    }

private:
    TelemetryCallback _telemetryCallback;
};

#endif // OASIS_DEVICE_H