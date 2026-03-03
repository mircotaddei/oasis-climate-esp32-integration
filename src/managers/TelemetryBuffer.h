#ifndef TELEMETRY_BUFFER_H
#define TELEMETRY_BUFFER_H

#include <Arduino.h>
#include <vector>
#include <ArduinoJson.h>
#include "HardwareManager.h" // To resolve sensor IDs


// --- TELEMETRY RECORD --------------------------------------------------------

struct TelemetryRecord {
    unsigned long timestamp; // Epoch time
    int sensorIndex;         // Index in HardwareManager's sensor vector
    float value;
};


// --- TELEMETRY BUFFER --------------------------------------------------------

class TelemetryBuffer {
public:
    TelemetryBuffer(size_t capacity = 60); // Default 60 records (~1 hour at 1 min interval for 1 sensor)
    
    void add(unsigned long timestamp, int sensorIndex, float value);
    bool isEmpty();
    void clear();
    
    // Serializes the buffer to a JSON array string
    // Requires HardwareManager to resolve sensorIndex to Global ID
    String getPayload(const char* deviceId, const HardwareManager* hwManager);

private:
    std::vector<TelemetryRecord> _buffer;
    size_t _capacity;
};

#endif