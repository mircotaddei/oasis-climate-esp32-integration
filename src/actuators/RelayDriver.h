#ifndef RELAY_DRIVER_H
#define RELAY_DRIVER_H

#include "ActuatorDriver.h"

// --- HARDWARE DEFAULTS -------------------------------------------------------

#define DEFAULT_RELAY_PIN 5
#define DEFAULT_RELAY_ACTIVE_HIGH true

// --- SOFTWARE DEFAULTS -------------------------------------------------------

#define DEFAULT_MODULATION_THRESHOLD 0.1 // 10%
#define DEFAULT_SAFETY_TIMEOUT_MS (30 * 60 * 1000) // 30 minutes

class RelayDriver : public ActuatorDriver {
public:
    RelayDriver(int pin = DEFAULT_RELAY_PIN, bool activeHigh = DEFAULT_RELAY_ACTIVE_HIGH);
    ~RelayDriver() override {}

    // OasisDevice Interface
    void begin() override;
    void update() override;
    OasisDeviceType getType() const override;
    const char* getLocalId() const override;
    const char* getGlobalId() const override;
    void setGlobalId(const char* globalId) override;
    bool isActive() const override;
    void setActive(bool state) override;
    bool isConnected() const override;
    void populateMeta(JsonObject& meta) const override;
    void applyMeta(JsonObjectConst meta) override;

    // ActuatorDriver Interface
    void setModulation(float level) override;
    float getCurrentState() const override;

private:
    int _pin;
    bool _activeHigh;
    char _localId[16];
    char _globalId[64];
    bool _isActive;
    
    // Internal State
    bool _state;
    unsigned long _lastCommandMillis;
    
    // OasisDevice Interface
    const char* getSensorType() const override;
    float getTelemetryValue() const override;

    // Software Configuration (Meta)
    float _modulationThreshold;
    unsigned long _safetyTimeoutMs;
    char _sensorType[32];

    void turnOn();
    void turnOff();
};

#endif // RELAY_DRIVER_H