#include "ConfigManager.h"
#include <HTTPClient.h> 


// --- CONSTRUCTOR -------------------------------------------------------------

ConfigManager::ConfigManager() {
    apiKey[0] = '\0';
    apiUrl[0] = '\0';
    macAddress[0] = '\0';
    localId[0] = '\0';
    deviceId[0] = '\0';

    // Default Timings (Safe Production Defaults)
    telemetryIntervalMs = 900000;  // 15 min
    sensorSampleMs = 60000;        // 60 sec
    actionPollMs = 30000;          // 30 sec
    claimPollMs = 10000;           // 10 sec
    provisioningRetryMs = 10000;   // 10 sec
    recoveryPollMs = 30000;        // 30 sec
    scheduleUpdateMs = 21600000;   // 6 hours
    heartbeatIntervalMs = 5000;    // 5 sec
    httpTimeoutMs = 5000;          // 5 sec
    diagnosticIntervalMs = 3600000; // Default 1 hour

    // Default Logic
    maxAuthFailures = 3;
    maxNetworkFailures = 3;
    failsafeHysteresis = 0.5;
    
    // Buffer Defaults
    telemetryBufferSize = -1;      // Auto
    telemetryAutoBufferSize = 0;
    firstTelemetryDelayMs = 5000;
    telemetryMaxBatchSize = 50;

    apiFailureCount = 0;
    cloudTimeoutCount = 0;
}


// --- LOAD MAC ADDRESS --------------------------------------------------------

void ConfigManager::loadMacAddress() {
    String mac = WiFi.macAddress();
    if (mac == "00:00:00:00:00:00" || mac == "") {
        DEBUG_PRINTLN("!!! CRITICAL: Invalid MAC address detected. Rebooting in 3s...");
        delay(3000);
        ESP.restart();
    }
    strlcpy(macAddress, mac.c_str(), sizeof(macAddress));
    
    String local = mac;
    local.replace(":", "");
    strlcpy(localId, local.c_str(), sizeof(localId));
}


// --- BEGIN -------------------------------------------------------------------

void ConfigManager::begin() {
    preferences.begin("oasis-app", false);
    loadMacAddress();

    #ifdef WOKWI_SIMULATION
        syncNvsFromCloud();
    #endif

    String savedApiKey = preferences.getString("api_key", "");
    strlcpy(apiKey, savedApiKey.c_str(), sizeof(apiKey));
    
    String savedDeviceId = preferences.getString("device_id", "");
    strlcpy(deviceId, savedDeviceId.c_str(), sizeof(deviceId));

    String savedApiUrl = preferences.getString("api_url", "");
    if (savedApiUrl.length() == 0 && strlen(FALLBACK_API_URL) > 0) {
        savedApiUrl = FALLBACK_API_URL;
    }
    strlcpy(apiUrl, savedApiUrl.c_str(), sizeof(apiUrl));

    String savedConfig = preferences.getString("device_config", "{}");
    deserializeJson(deviceConfig, savedConfig);

    #ifdef WOKWI_SIMULATION
        strlcpy(apiUrl, FALLBACK_API_URL, sizeof(apiUrl));
        
        claimIntervalMs = 5000;      
        telemetryIntervalMs = 10000; 

        DEBUG_PRINTLN("--- [CONFIG] Wokwi Mode Active ---");
        DEBUG_PRINTLN("MAC Address: ", macAddress);
        DEBUG_PRINTLN("Local ID: ", localId);
        DEBUG_PRINTLN("API URL: ", apiUrl);
        DEBUG_PRINTLN("----------------------------------");

        #ifdef FORCE_UNCLAIMED
            DEBUG_PRINTLN("--- [WOKWI] FORCE_UNCLAIMED is active. Clearing NVS. ---");
            preferences.clear();
            apiKey[0] = '\0';
            deviceId[0] = '\0';
            deviceConfig.clear();
        #endif
    #endif
}


// --- IS CLAIMED --------------------------------------------------------------

bool ConfigManager::isClaimed() {
    return strlen(apiKey) > 0 && strlen(deviceId) > 0;
}


// --- IS PROVISIONED ----------------------------------------------------------

bool ConfigManager::isProvisioned() {
    return deviceConfig.size() > 0;
}


// --- SAVE IDENTITY -----------------------------------------------------------

void ConfigManager::saveIdentity(const char* newApiKey, const char* newDeviceId) {
    // EMERGENCY FIX: Protect against null pointers
    if (newApiKey == nullptr || newDeviceId == nullptr) {
        DEBUG_PRINTLN("[CONFIG] ERROR: Attempted to save null identity!");
        return;
    }

    strlcpy(apiKey, newApiKey, sizeof(apiKey));
    preferences.putString("api_key", apiKey);
    
    strlcpy(deviceId, newDeviceId, sizeof(deviceId));
    preferences.putString("device_id", deviceId);

    DEBUG_PRINTLN("Identity data saved to local NVS.");

    #ifdef WOKWI_SIMULATION
        syncNvsToCloud();
    #endif
}


// --- CLEAR IDENTITY ----------------------------------------------------------

void ConfigManager::invalidateApiKey() {
    DEBUG_PRINTLN("[CONFIG] Invalidating API Key (keeping Device ID)...");
    
    preferences.remove("api_key");
    apiKey[0] = '\0';
    
    #ifdef WOKWI_SIMULATION
        syncNvsToCloud(); 
    #endif
}


// --- SAVE CONFIG -------------------------------------------------------------

void ConfigManager::saveConfig(const char* jsonConfigString) {
    preferences.putString("device_config", jsonConfigString);
    deserializeJson(deviceConfig, jsonConfigString);
    
    // Parse Timings
    if (deviceConfig["telemetry_interval_min"].is<unsigned long>()) {
        telemetryIntervalMs = deviceConfig["telemetry_interval_min"].as<unsigned long>() * 60 * 1000;
    }
    if (deviceConfig["sensor_sample_sec"].is<unsigned long>()) {
        sensorSampleMs = deviceConfig["sensor_sample_sec"].as<unsigned long>() * 1000;
    }
    if (deviceConfig["action_poll_sec"].is<unsigned long>()) {
        actionPollMs = deviceConfig["action_poll_sec"].as<unsigned long>() * 1000;
    }
    if (deviceConfig["claim_poll_sec"].is<unsigned long>()) {
        claimPollMs = deviceConfig["claim_poll_sec"].as<unsigned long>() * 1000;
    }
    if (deviceConfig["provisioning_retry_sec"].is<unsigned long>()) {
        provisioningRetryMs = deviceConfig["provisioning_retry_sec"].as<unsigned long>() * 1000;
    }
    if (deviceConfig["recovery_poll_sec"].is<unsigned long>()) {
        recoveryPollMs = deviceConfig["recovery_poll_sec"].as<unsigned long>() * 1000;
    }
    if (deviceConfig["schedule_update_hours"].is<unsigned long>()) {
        scheduleUpdateMs = deviceConfig["schedule_update_hours"].as<unsigned long>() * 3600 * 1000;
    }
    if (deviceConfig["heartbeat_interval_sec"].is<unsigned long>()) {
        heartbeatIntervalMs = deviceConfig["heartbeat_interval_sec"].as<unsigned long>() * 1000;
    }
    if (deviceConfig["http_timeout_ms"].is<unsigned long>()) {
        httpTimeoutMs = deviceConfig["http_timeout_ms"].as<unsigned long>();
    }
    if (deviceConfig["diagnostic_interval_seconds"].is<unsigned long>()) {
        diagnosticIntervalMs = deviceConfig["diagnostic_interval_seconds"].as<unsigned long>() * 1000;
    }

    // Parse Logic
    if (deviceConfig["max_auth_failures"].is<int>()) {
        maxAuthFailures = deviceConfig["max_auth_failures"].as<int>();
    }
    if (deviceConfig["max_network_failures"].is<int>()) {
        maxNetworkFailures = deviceConfig["max_network_failures"].as<int>();
    }
    if (deviceConfig["failsafe_hysteresis"].is<float>()) {
        failsafeHysteresis = deviceConfig["failsafe_hysteresis"].as<float>();
    }

    // Buffer Defaults
    if (deviceConfig["telemetry_buffer_size"].is<int>()) {
        telemetryBufferSize = deviceConfig["telemetry_buffer_size"].as<int>();
    }
    if (deviceConfig["first_telemetry_delay_sec"].is<unsigned long>()) {
        firstTelemetryDelayMs = deviceConfig["first_telemetry_delay_sec"].as<unsigned long>() * 1000;
    }
    if (deviceConfig["telemetry_max_batch_size"].is<int>()) {
        telemetryMaxBatchSize = deviceConfig["telemetry_max_batch_size"].as<int>();
    }


    DEBUG_PRINTLN("Provisioning config saved and applied.");

    #ifdef WOKWI_SIMULATION
        syncNvsToCloud();
    #endif
}


// --- FACTORY RESET -----------------------------------------------------------

void ConfigManager::factoryReset() {
    DEBUG_PRINTLN("Performing Factory Reset...");
    
    preferences.clear();
    apiKey[0] = '\0';
    deviceId[0] = '\0';
    deviceConfig.clear();

    WiFi.disconnect(true, true); 
    DEBUG_PRINTLN("WiFi credentials erased.");

    #ifdef WOKWI_SIMULATION
        syncNvsToCloud(); 
    #endif
}


// --- GET PIN -----------------------------------------------------------------

int ConfigManager::getPin(const char* function, int defaultValue) {
    if (deviceConfig["pin_config"][function].is<int>()) {
        return deviceConfig["pin_config"][function].as<int>();
    }
    return defaultValue;
}


// --- SAVE DEVICE STATE -------------------------------------------------------

void ConfigManager::saveDeviceState(const char* localId, const char* globalId, bool isActive, JsonObjectConst meta) {
    // Modern syntax for creating nested objects
    if (!deviceConfig["devices_map"].is<JsonObject>()) {
        deviceConfig["devices_map"].to<JsonObject>();
    }
    
    JsonObject deviceObj = deviceConfig["devices_map"][localId].to<JsonObject>();
    deviceObj["global_id"] = globalId;
    deviceObj["is_active"] = isActive;
    
    deviceObj["meta"] = meta;

    String configStr;
    serializeJson(deviceConfig, configStr);
    preferences.putString("device_config", configStr);
    
    DEBUG_PRINTLN("[CONFIG] Saved state for device ", localId);

    #ifdef WOKWI_SIMULATION
        syncNvsToCloud();
    #endif
}


// --- LOAD DEVICE STATE -------------------------------------------------------

bool ConfigManager::loadDeviceState(const char* localId, char* outGlobalId, size_t maxLen, bool* outIsActive, JsonObject& outMeta) {
    if (deviceConfig["devices_map"][localId].is<JsonObject>()) {
        JsonObject deviceObj = deviceConfig["devices_map"][localId];
        
        if (deviceObj["global_id"].is<const char*>()) {
            strlcpy(outGlobalId, deviceObj["global_id"].as<const char*>(), maxLen);
        }
        
        if (deviceObj["is_active"].is<bool>()) {
            *outIsActive = deviceObj["is_active"].as<bool>();
        }
        
        if (deviceObj["meta"].is<JsonObject>()) {
            outMeta.set(deviceObj["meta"]);
        }
        
        return true;
    }
    return false;
}


#ifdef WOKWI_SIMULATION
// --- SYNC NVS FROM CLOUD -----------------------------------------------------

void ConfigManager::syncNvsFromCloud() {
    if (WiFi.status() != WL_CONNECTED) {
        DEBUG_PRINTLN("[NVS SYNC] WiFi not ready, skipping cloud sync.");
        return;
    }
    
    HTTPClient http;
    String url = String(FALLBACK_API_URL) + "/dev/nvs/" + String(localId);
    http.begin(url);
    http.setTimeout(3000);

    DEBUG_PRINTLN("--- [NVS SYNC] Fetching from Cloud ---");
    DEBUG_PRINTLN("URL: ", url);

    int httpCode = http.GET();
    if (httpCode == 200) {
        String payload = http.getString();
        DEBUG_PRINTLN("Response: ", payload);
        
        JsonDocument doc;
        if (deserializeJson(doc, payload) == DeserializationError::Ok && !doc.isNull()) {
            if (doc["api_key"].is<const char*>() && strlen(doc["api_key"].as<const char*>()) > 0) {
                preferences.putString("api_key", doc["api_key"].as<const char*>());
            }
            if (doc["device_id"].is<const char*>() && strlen(doc["device_id"].as<const char*>()) > 0) {
                preferences.putString("device_id", doc["device_id"].as<const char*>());
            }
            if (doc["device_config"].is<const char*>() && strlen(doc["device_config"].as<const char*>()) > 0) {
                preferences.putString("device_config", doc["device_config"].as<const char*>());
            }
            DEBUG_PRINTLN("NVS state restored from cloud.");
        } else {
            DEBUG_PRINTLN("[NVS SYNC] Cloud returned empty or invalid JSON. Doing nothing.");
        }
    } else {
        DEBUG_PRINTLN("Failed to fetch NVS state, HTTP: ", httpCode);
    }
    http.end();
    DEBUG_PRINTLN("--- [NVS SYNC] Fetch END ---");
}


// --- SYNC NVS TO CLOUD -------------------------------------------------------

void ConfigManager::syncNvsToCloud() {
    if (WiFi.status() != WL_CONNECTED) {
        DEBUG_PRINTLN("[NVS SYNC] WiFi not ready, skipping cloud sync.");
        return;
    }

    HTTPClient http;
    String url = String(FALLBACK_API_URL) + "/dev/nvs/" + String(localId);
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(3000);

    DEBUG_PRINTLN("--- [NVS SYNC] Saving to Cloud ---");

    JsonDocument doc;
    doc["api_key"] = apiKey;
    doc["device_id"] = deviceId;
    
    String configStr;
    serializeJson(deviceConfig, configStr);
    doc["device_config"] = configStr;
    
    String payload;
    serializeJson(doc, payload);
    DEBUG_PRINTLN("Payload: ", payload);

    int httpCode = http.POST(payload);
    if (httpCode == 200) {
        DEBUG_PRINTLN("NVS state saved to cloud.");
    } else {
        DEBUG_PRINTLN("Failed to save NVS state, HTTP: ", httpCode);
    }
    http.end();
    DEBUG_PRINTLN("--- [NVS SYNC] Save END ---");
}
#endif