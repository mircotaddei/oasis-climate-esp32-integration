#include "OasisDevice.h"
#include "../utils/Logger.h"

// --- CONSTRUCTOR & DESTRUCTOR ------------------------------------------------

OasisDevice::OasisDevice() {
    _telemetryCallback = nullptr;
    _sensorType[0] = '\0';
    _reportDelta = 0.0;
    _reportHeartbeat = 30;
    _lastReportedValue = NAN;
    _samplesSinceLastReport = 0;
}

OasisDevice::~OasisDevice() {}


// --- UNIFIED TELEMETRY INTERFACE ---------------------------------------------

const char* OasisDevice::getSensorType() const {
    return _sensorType;
}


// --- META CONFIGURATION (BASE) -----------------------------------------------

void OasisDevice::populateMeta(JsonObject& meta) const {
    meta["sensor_type"] = _sensorType;
    meta["report_delta"] = _reportDelta;
    meta["report_heartbeat"] = _reportHeartbeat;
}

void OasisDevice::applyMeta(JsonObjectConst meta) {
    if (meta["sensor_type"].is<const char*>()) {
        strlcpy(_sensorType, meta["sensor_type"].as<const char*>(), sizeof(_sensorType));
    }
    if (meta["report_delta"].is<float>()) {
        _reportDelta = meta["report_delta"].as<float>();
    }
    if (meta["report_heartbeat"].is<int>()) {
        _reportHeartbeat = meta["report_heartbeat"].as<int>();
    }
}


// --- EVENT DRIVEN TELEMETRY --------------------------------------------------

void OasisDevice::setTelemetryCallback(TelemetryCallback cb) {
    _telemetryCallback = cb;
}

void OasisDevice::emitTelemetry(float value) {
    if (_telemetryCallback && isActive() && isConnected()) {
        _lastReportedValue = value;
        _samplesSinceLastReport = 0;
        _telemetryCallback(this, value);
    }
}

bool OasisDevice::shouldReport(float newValue, bool globalEnable) {
    if (!globalEnable || _reportDelta <= 0.0 || isnan(_lastReportedValue)) {
        _lastReportedValue = newValue;
        _samplesSinceLastReport = 0;
        return true;
    }

    _samplesSinceLastReport++;

    float diff = abs(newValue - _lastReportedValue);
    if (diff >= _reportDelta) {
        _lastReportedValue = newValue;
        _samplesSinceLastReport = 0;
        return true;
    }

    if (_samplesSinceLastReport >= _reportHeartbeat) {
        _lastReportedValue = newValue;
        _samplesSinceLastReport = 0;
        return true;
    }

    return false;
}


// --- DIAGNOSTICS (BASE) ------------------------------------------------------

void OasisDevice::populateDiagnostics(JsonObject& metrics, JsonObject& tags) {
    // Default implementation does nothing. Override in derived classes.
}