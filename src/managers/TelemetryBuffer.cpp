#include "TelemetryBuffer.h"
#include "TimeManager.h"
#include "ConfigManager.h"


// --- CONSTRUCTOR -------------------------------------------------------------

TelemetryBuffer::TelemetryBuffer(size_t capacity) {
    _capacity = capacity;
    _buffer.reserve(capacity);
}


// --- ADD ---------------------------------------------------------------------

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

    const std::vector<SensorDriver*>& sensors = hwManager->getSensors();

    for (const auto& record : _buffer) {
        if (record.sensorIndex >= 0 && record.sensorIndex < sensors.size()) {
            SensorDriver* sensor = sensors[record.sensorIndex];
            
            JsonObject r = readings.add<JsonObject>();
            r["device_id"] = sensor->getGlobalId();
            r["value"] = record.value;
            
            // NEW: Convert epoch to ISO 8601 string
            if (record.timestamp > 1000000000) { 
                r["timestamp"] = TimeManager::epochToISO8601(record.timestamp);
            }
        }
    }

    String payload;
    serializeJson(doc, payload);
    return payload;
}
