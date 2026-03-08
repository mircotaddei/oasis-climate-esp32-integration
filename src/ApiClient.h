#ifndef API_CLIENT_H
#define API_CLIENT_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <vector>
#include "managers/ConfigManager.h"
#include "managers/ScheduleManager.h"
#include "devices/OasisDevice.h"


// --- CLAIM STATUS ------------------------------------------------------------
// Represents the status of the claiming process

enum ClaimStatus {
    CLAIM_SUCCESS,
    CLAIM_PENDING,
    CLAIM_RECOVERY_NEEDED,
    CLAIM_ERROR
};


// --- API RESPONSE STRUCT ------------------------------------------------------

struct ApiResponse {
    int code;
    String body;
};


// --- ACTION RESPONSE STRUCT --------------------------------------------------
struct ActionResponse {
    bool success;
    float modulation;
    String otaUrl;
    bool synced;
};


// --- API CLIENT --------------------------------------------------------------

class ApiClient {
public:
    ClaimStatus pollRegistration(ConfigManager* config, String* outClaimCode = nullptr);
    bool fetchConfig(ConfigManager* config);
    bool updateThermostatConfig(ConfigManager* config);
    
    void registerDevices(ConfigManager* config, const std::vector<OasisDevice*>& devices);
    bool reportRecovery(ConfigManager* config);
    
    bool sendTelemetry(ConfigManager* config, String payload);
    void sendDiagnostics(ConfigManager* config, const std::vector<OasisDevice*>& devices, const char* resetReason = nullptr, const char* otaStatus = nullptr);    
    
    ActionResponse pollActions(ConfigManager* config);

    bool fetchSchedule(ConfigManager* config, ScheduleManager* scheduleManager);

private:
    ApiResponse executeRequest(ConfigManager* config, const char* method, const char* path, String payload = "", bool authenticated = true);
};

#endif