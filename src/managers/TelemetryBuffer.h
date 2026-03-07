#ifndef TELEMETRY_BUFFER_H
#define TELEMETRY_BUFFER_H

#include <Arduino.h>
#include <vector>
#include <ArduinoJson.h>
#include "HardwareManager.h" // To resolve sensor IDs


// --- TELEMETRY RECORD --------------------------------------------------------

struct TelemetryRecord {
    unsigned long timestamp; // Epoch time
    OasisDevice* device;     // Pointer to the device
    float value;
};


// --- TELEMETRY BUFFER --------------------------------------------------------

class TelemetryBuffer {
public:
    TelemetryBuffer(size_t capacity = 60); // Default 60 records (~1 hour at 1 min interval for 1 sensor)
    
    int init(int requestedSize);

    void add(unsigned long timestamp, OasisDevice* device, float value);
    bool isEmpty();
    void clear();
    
    // Serializes the buffer to a JSON array string
    String getPayload(const char* deviceId);

private:
    std::vector<TelemetryRecord> _buffer;
    size_t _capacity;
};

#endif