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
        // AUTO MODE: Use 40% of free heap
        uint32_t freeHeap = ESP.getFreeHeap();
        uint32_t safeRam = (freeHeap * 40) / 100; 
        size_t recordSize = sizeof(TelemetryRecord);
        
        _capacity = safeRam / recordSize;
        
        // Cap at 6000 records (~24h for 4 sensors)
        if (_capacity < 10) _capacity = 10; 
        if (_capacity > 6000) _capacity = 6000; 
        
        DEBUG_PRINTLN("[BUFFER] Auto-Sizing (40%). Free: ", freeHeap, " B. Cap: ", _capacity);
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
int TelemetryBuffer::getPayload(const char* deviceId, String& outPayload, int limit) {
    JsonDocument doc;
    doc["device_id"] = deviceId;
    JsonArray readings = doc["readings"].to<JsonArray>();

    int count = 0;
    // Iterate only up to the limit or buffer size
    for (int i = 0; i < _buffer.size() && count < limit; i++) {
        const auto& record = _buffer[i];
        
        if (record.device && strlen(record.device->getGlobalId()) > 0) {
            JsonObject r = readings.add<JsonObject>();
            r["device_id"] = record.device->getGlobalId();
            r["value"] = record.value;
            
            if (record.timestamp > 1000000000) { 
                r["timestamp"] = TimeManager::epochToISO8601(record.timestamp);
            }
            
            if (record.device->getType() == DEVICE_TYPE_SENSOR_DALLAS) {
                SensorDriver* sensor = static_cast<SensorDriver*>(record.device);
                r["error_count"] = sensor->getErrorCount();
                sensor->clearErrorCount();
            }
            count++;
        }
    }

    serializeJson(doc, outPayload);
    return count;
}


// --- REMOVE OLDEST -----------------------------------------------------------

void TelemetryBuffer::removeOldest(int count) {
    if (count <= 0) return;
    if (count >= _buffer.size()) {
        _buffer.clear();
    } else {
        _buffer.erase(_buffer.begin(), _buffer.begin() + count);
    }
    DEBUG_PRINTLN("[BUFFER] Removed ", count, " records. Remaining: ", _buffer.size());
}


// --- GET COUNT ---------------------------------------------------------------

int TelemetryBuffer::getCount() {
    return _buffer.size();
}
