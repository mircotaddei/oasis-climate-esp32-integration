#include "ConfigManager.h"
#include <HTTPClient.h> // Needed for NVS Sync


// --- CONSTRUCTOR -------------------------------------------------------------

ConfigManager::ConfigManager() {
    apiKey[0] = '\0';
    apiUrl[0] = '\0';
    macAddress[0] = '\0';
    localId[0] = '\0';
    deviceId[0] = '\0';
    apiFailureCount = 0;
    
    // Default timings (Safe Production Defaults)
    claimIntervalMs = 10000;       // 10 seconds
    telemetryIntervalMs = 900000;  // 15 minutes
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
    
    // Create localId (MAC without colons)
    String local = mac;
    local.replace(":", "");
    strlcpy(localId, local.c_str(), sizeof(localId));
}


// --- BEGIN -------------------------------------------------------------------

void ConfigManager::begin() {
    preferences.begin("oasis-app", false);
    loadMacAddress();

    #ifdef WOKWI_SIMULATION
        // In Wokwi, sync NVS from cloud to simulate persistence
        syncNvsFromCloud();
    #endif

    // Load Identity
    String savedApiKey = preferences.getString("api_key", "");
    strlcpy(apiKey, savedApiKey.c_str(), sizeof(apiKey));
    
    String savedDeviceId = preferences.getString("device_id", "");
    strlcpy(deviceId, savedDeviceId.c_str(), sizeof(deviceId));

    // Load API URL
    String savedApiUrl = preferences.getString("api_url", "");
    if (savedApiUrl.length() == 0 && strlen(FALLBACK_API_URL) > 0) {
        savedApiUrl = FALLBACK_API_URL;
    }
    strlcpy(apiUrl, savedApiUrl.c_str(), sizeof(apiUrl));

    // Load Dynamic Config
    String savedConfig = preferences.getString("device_config", "{}");
    deserializeJson(deviceConfig, savedConfig);

    // Wokwi / Debug Overrides
    #ifdef WOKWI_SIMULATION
        strlcpy(apiUrl, FALLBACK_API_URL, sizeof(apiUrl));
        
        // Aggressive timings for development
        claimIntervalMs = 5000;      // 5 seconds
        telemetryIntervalMs = 10000; // 10 seconds

        DEBUG_PRINTLN("--- [CONFIG] Wokwi Mode Active ---");
        DEBUG_PRINTLN("MAC Address: ", macAddress);
        DEBUG_PRINTLN("Local ID: ", localId);
        DEBUG_PRINTLN("API URL: ", apiUrl);
        DEBUG_PRINTLN("Claim Interval: ", claimIntervalMs);
        DEBUG_PRINTLN("Telemetry Interval: ", telemetryIntervalMs);
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
    // A device is provisioned if it has a config object with at least one key
    return deviceConfig.size() > 0;
}


// --- SAVE IDENTITY -----------------------------------------------------------

void ConfigManager::saveIdentity(const char* newApiKey, const char* newDeviceId) {
    strlcpy(apiKey, newApiKey, sizeof(apiKey));
    preferences.putString("api_key", apiKey);
    
    strlcpy(deviceId, newDeviceId, sizeof(deviceId));
    preferences.putString("device_id", deviceId);

    DEBUG_PRINTLN("Identity data saved to local NVS.");

    #ifdef WOKWI_SIMULATION
        syncNvsToCloud();
    #endif
}


// --- SAVE CONFIG -------------------------------------------------------------

void ConfigManager::saveConfig(const char* jsonConfigString) {
    preferences.putString("device_config", jsonConfigString);
    deserializeJson(deviceConfig, jsonConfigString);
    
    if (deviceConfig["telemetry_interval_min"].is<unsigned long>()) {
        telemetryIntervalMs = deviceConfig["telemetry_interval_min"].as<unsigned long>() * 60 * 1000;
    }

    DEBUG_PRINTLN("Provisioning config saved to local NVS.");

    #ifdef WOKWI_SIMULATION
        syncNvsToCloud();
    #endif
}


// --- SAVE SENSOR STATE -------------------------------------------------------

void ConfigManager::saveSensorState(const char* localId, const char* globalId, bool isActive, float offset) {
    // Ensure the sensors_map object exists
    if (!deviceConfig["sensors_map"].is<JsonObject>()) {
        deviceConfig["sensors_map"] = deviceConfig.createNestedObject("sensors_map");
    }
    
    JsonObject sensorObj = deviceConfig["sensors_map"][localId].to<JsonObject>();
    sensorObj["global_id"] = globalId;
    sensorObj["is_active"] = isActive;
    sensorObj["offset"] = offset; // NEW

    // Serialize and save to NVS
    String configStr;
    serializeJson(deviceConfig, configStr);
    preferences.putString("device_config", configStr);
    
    DEBUG_PRINTLN("[CONFIG] Saved state for sensor ", localId);

    #ifdef WOKWI_SIMULATION
        syncNvsToCloud();
    #endif
}


// --- LOAD SENSOR STATE -------------------------------------------------------

bool ConfigManager::loadSensorState(const char* localId, char* outGlobalId, size_t maxLen, bool* outIsActive, float* outOffset) {
    if (deviceConfig["sensors_map"][localId].is<JsonObject>()) {
        JsonObject sensorObj = deviceConfig["sensors_map"][localId];
        
        if (sensorObj["global_id"].is<const char*>()) {
            strlcpy(outGlobalId, sensorObj["global_id"].as<const char*>(), maxLen);
        }
        
        if (sensorObj["is_active"].is<bool>()) {
            *outIsActive = sensorObj["is_active"].as<bool>();
        }
        
        if (sensorObj["offset"].is<float>()) {
            *outOffset = sensorObj["offset"].as<float>();
        } else {
            *outOffset = 0.0; // Default if not present
        }
        
        return true;
    }
    return false;
}


// --- FACTORY RESET -----------------------------------------------------------

void ConfigManager::factoryReset() {
    DEBUG_PRINTLN("Performing Factory Reset...");
    
    // 1. Clear Application NVS
    preferences.clear();
    apiKey[0] = '\0';
    deviceId[0] = '\0';
    deviceConfig.clear();

    // 2. Clear WiFi Credentials (System NVS)
    // WiFi.disconnect(wifiOff, eraseAp)
    WiFi.disconnect(true, true); 
    DEBUG_PRINTLN("WiFi credentials erased.");

    #ifdef WOKWI_SIMULATION
        syncNvsToCloud(); // Sync the cleared state to the cloud
    #endif
}


// --- GET PIN -----------------------------------------------------------------

int ConfigManager::getPin(const char* function, int defaultValue) {
    if (deviceConfig["pin_config"][function].is<int>()) {
        return deviceConfig["pin_config"][function].as<int>();
    }
    return defaultValue;
}


#ifdef WOKWI_SIMULATION
// --- SYNC NVS FROM CLOUD ----------------------------------------------------

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
        DEBUG_PRINTLN("[NVS SYNC] Doing nothing on error.");
    }
    http.end();
    DEBUG_PRINTLN("--- [NVS SYNC] Fetch END ---");
}


// --- SYNC NVS TO CLOUD ------------------------------------------------------

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