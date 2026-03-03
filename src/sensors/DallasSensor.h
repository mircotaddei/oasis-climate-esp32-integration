#ifndef DALLAS_SENSOR_H
#define DALLAS_SENSOR_H

#include "SensorDriver.h"
#include <OneWire.h>
#include <DallasTemperature.h>


// --- DALLAS SENSOR -----------------------------------------------------------

class DallasSensor : public SensorDriver {
public:
    DallasSensor(int pin);

    void begin() override;
    void update() override;

    float getTemperature() override;
    float getHumidity() override;
    
    const char* getId() override;
    const char* getGlobalId() override;
    void setGlobalId(const char* globalId) override;
    
    SensorType getType() override;

    bool isActive() override;
    void setActive(bool state) override;

    bool isConnected() override;

    unsigned long getWarmupTimeMs() override;

    void setOffset(float offset) override;
    float getOffset() override;

    unsigned int getErrorCount() override;
    void clearErrorCount() override;

private:
    int _pin;
    OneWire* _oneWire;
    DallasTemperature* _sensors;
    DeviceAddress _deviceAddress;
    char _id[24]; 
    char _globalId[64]; 
    float _lastTemp;
    unsigned long _lastRequestMillis;
    bool _requestPending;
    bool _isActive;
    bool _isConnected;
    unsigned long _reconnectMillis;

    // Filtering and Calibration variables
    float _offset;
    unsigned int _errorCount;
    
    float _filteredTemp;
    float _lastRawTemp;
    int _consecutiveOutliers;
    
    // Filter constants
    const float EMA_ALPHA = 0.2; // Smoothing factor (0.0 - 1.0). Lower is smoother.
    const float MAX_TEMP_JUMP = 5.0; // Max allowed jump in °C between readings
    const int MAX_OUTLIERS = 3; // Accept jump after this many consecutive outliers

    void applyFilter(float rawTemp);
};

#endif