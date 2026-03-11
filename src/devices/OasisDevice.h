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

// --- TELEMETRY CALLBACK TYPE -------------------------------------------------
typedef void (*TelemetryCallback)(OasisDevice* device, float value);

// --- BASE CLASS --------------------------------------------------------------
class OasisDevice {
public:
    OasisDevice();
    virtual ~OasisDevice();

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
    const char* getSensorType() const; 
    virtual float getTelemetryValue() const = 0; 

    // Configuration (Meta) - Base implementation handles common fields
    virtual void populateMeta(JsonObject& meta) const;
    virtual void applyMeta(JsonObjectConst meta);

    // Event Driven Telemetry
    void setTelemetryCallback(TelemetryCallback cb);
    bool shouldReport(float newValue, bool globalEnable);

    // Diagnostics
    virtual void populateDiagnostics(JsonObject& metrics, JsonObject& tags);

protected:
    void emitTelemetry(float value);

    // Common Configuration Variables
    char _sensorType[32];
    float _reportDelta;
    int _reportHeartbeat;

    // Internal State for Send on Delta
    float _lastReportedValue;
    int _samplesSinceLastReport;

private:
    TelemetryCallback _telemetryCallback;
};

#endif // OASIS_DEVICE_H