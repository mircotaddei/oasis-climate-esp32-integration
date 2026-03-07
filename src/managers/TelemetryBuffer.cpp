#include "TelemetryBuffer.h"
#include "TimeManager.h"
#include "ConfigManager.h" 


// --- CONSTRUCTOR -------------------------------------------------------------

TelemetryBuffer::TelemetryBuffer(size_t capacity) {
    _capacity = capacity;
    _buffer.reserve(capacity);
}


// --- INIT --------------------------------------------------------------------

int TelemetryBuffer::init(int requestedSize) {
    if (requestedSize >= 0) {
        _capacity = requestedSize;
        DEBUG_PRINTLN("[BUFFER] Size set to fixed value: ", _capacity);
    } else {
        // AUTO MODE
        // Calculate safe size based on free heap
        // Reserve 10% of free heap for buffer
        uint32_t freeHeap = ESP.getFreeHeap();
        uint32_t safeRam = freeHeap / 10; 
        size_t recordSize = sizeof(TelemetryRecord);
        
        _capacity = safeRam / recordSize;
        
        // Cap at reasonable limits
        if (_capacity < 10) _capacity = 10; // Minimum
        if (_capacity > 1000) _capacity = 1000; // Maximum
        
        DEBUG_PRINTLN("[BUFFER] Auto-Sizing. Free Heap: ", freeHeap, " B. Allocated: ", _capacity, " records.");
    }
    
    _buffer.reserve(_capacity);
    return _capacity;
}


// --- ADD RECORD --------------------------------------------------------------

void TelemetryBuffer::add(unsigned long timestamp, OasisDevice* device, float value) {
    if (_buffer.size() >= _capacity) {
        _buffer.erase(_buffer.begin());
        DEBUG_PRINTLN("[BUFFER] Full! Dropping oldest record.");
    }
    
    TelemetryRecord record;
    record.timestamp = timestamp;
    record.device = device;
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

String TelemetryBuffer::getPayload(const char* deviceId) {
    JsonDocument doc;
    doc["device_id"] = deviceId;
    JsonArray readings = doc["readings"].to<JsonArray>();

    for (const auto& record : _buffer) {
        if (record.device && strlen(record.device->getGlobalId()) > 0) {
            JsonObject r = readings.add<JsonObject>();
            r["device_id"] = record.device->getGlobalId();
            r["value"] = record.value;
            
            if (record.timestamp > 1000000000) { 
                r["timestamp"] = TimeManager::epochToISO8601(record.timestamp);
            }
            
            // If it's a sensor, add error count
            if (record.device->getType() == DEVICE_TYPE_SENSOR_DALLAS) {
                SensorDriver* sensor = static_cast<SensorDriver*>(record.device);
                r["error_count"] = sensor->getErrorCount();
                sensor->clearErrorCount();
            }
        }
    }

    String payload;
    serializeJson(doc, payload);
    return payload;
}