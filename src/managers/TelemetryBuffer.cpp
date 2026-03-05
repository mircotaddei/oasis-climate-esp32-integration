#include "TelemetryBuffer.h"
#include "TimeManager.h" // Include TimeManager for conversion
#include "ConfigManager.h" 


// --- CONSTRUCTOR -------------------------------------------------------------

TelemetryBuffer::TelemetryBuffer(size_t capacity) {
    _capacity = capacity;
    _buffer.reserve(capacity);
}


// --- ADD RECORD --------------------------------------------------------------

void TelemetryBuffer::add(unsigned long timestamp, int sensorIndex, float value) {
    if (_buffer.size() >= _capacity) {
        // Buffer full: remove oldest element (FIFO)
        _buffer.erase(_buffer.begin());
        DEBUG_PRINTLN("[BUFFER] Full! Dropping oldest record.");
    }
    
    TelemetryRecord record;
    record.timestamp = timestamp;
    record.sensorIndex = sensorIndex;
    record.value = value;
    
    _buffer.push_back(record);
    DEBUG_PRINTLN("[BUFFER] Added record. Count: ", _buffer.size());
}


// --- IS EMPTY ----------------------------------------------------------------

bool TelemetryBuffer::isEmpty() {
    return _buffer.empty();
}


// --- CLEAR -------------------------------------------------------------------

void TelemetryBuffer::clear() {
    _buffer.clear();
}


// --- GET PAYLOAD -------------------------------------------------------------

String TelemetryBuffer::getPayload(const char* deviceId, const HardwareManager* hwManager) {
    JsonDocument doc;
    doc["device_id"] = deviceId;
    JsonArray readings = doc["readings"].to<JsonArray>();

    // FIX: Use getAllDevices() instead of getSensors()
    const std::vector<OasisDevice*>& devices = hwManager->getAllDevices();

    for (const auto& record : _buffer) {
        if (record.sensorIndex >= 0 && record.sensorIndex < devices.size()) {
            OasisDevice* device = devices[record.sensorIndex];
            
            // FIX: Check type and cast to SensorDriver
            if (device->getType() == DEVICE_TYPE_SENSOR_DALLAS || device->getType() == DEVICE_TYPE_SENSOR_DHT) {
                SensorDriver* sensor = static_cast<SensorDriver*>(device);
                
                JsonObject r = readings.add<JsonObject>();
                r["device_id"] = sensor->getGlobalId();
                r["value"] = record.value;
                
                if (record.timestamp > 1000000000) { 
                    r["timestamp"] = TimeManager::epochToISO8601(record.timestamp);
                }
            }
        }
    }

    String payload;
    serializeJson(doc, payload);
    return payload;
}