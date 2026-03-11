#ifndef DALLAS_SENSOR_H
#define DALLAS_SENSOR_H

#include "SensorDriver.h"
#include <OneWire.h>
#include <DallasTemperature.h>

// --- HARDWARE DEFAULTS -------------------------------------------------------
#define DEFAULT_ONEWIRE_PIN 4

// --- SOFTWARE DEFAULTS -------------------------------------------------------
#define DEFAULT_DALLAS_OFFSET 0.0
#define DEFAULT_DALLAS_ALPHA 0.2
#define MAX_TEMP_JUMP 5.0
#define MAX_OUTLIERS 3

class DallasSensor : public SensorDriver {
public:
    DallasSensor(int pin = DEFAULT_ONEWIRE_PIN);
    ~DallasSensor() override;

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
    
    float getTelemetryValue() const override;
    
    void populateMeta(JsonObject& meta) const override;
    void applyMeta(JsonObjectConst meta) override;
    void populateDiagnostics(JsonObject& metrics, JsonObject& tags) override;

    // SensorDriver Interface
    float getTemperature() const override;
    float getHumidity() const override;
    unsigned long getWarmupTimeMs() const override;
    unsigned int getErrorCount() const override;
    void clearErrorCount() override;

    // Calibration (Specific to DallasSensor, no override)
    void setOffset(float offset);
    float getOffset();

private:
    int _pin;
    OneWire* _oneWire;
    DallasTemperature* _sensors;
    DeviceAddress _deviceAddress;
    
    char _id[24]; 
    char _globalId[64]; 
    
    unsigned long _lastRequestMillis;
    bool _requestPending;
    bool _isActive;
    bool _isConnected;
    unsigned long _reconnectMillis; 

    // Software Configuration (Specific to Dallas)
    float _offset;
    float _alpha; 
    
    // Internal State
    unsigned int _errorCount;
    float _filteredTemp;
    float _lastRawTemp;
    int _consecutiveOutliers;

    void applyFilter(float rawTemp);
};

#endif // DALLAS_SENSOR_H