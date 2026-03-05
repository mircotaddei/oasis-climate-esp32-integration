#include "ApiClient.h"
#include <ArduinoJson.h>


// --- POLL REGISTRATION -------------------------------------------------------

ClaimStatus ApiClient::pollRegistration(ConfigManager* config, String* outClaimCode) {
    if (WiFi.status() != WL_CONNECTED) {
        DEBUG_PRINTLN("[API] WiFi lost. Skipping poll.");
        return CLAIM_ERROR;
    }

    WiFiClient *client = (String(config->apiUrl).startsWith("https")) ? new WiFiClientSecure : new WiFiClient;
    if (String(config->apiUrl).startsWith("https")) ((WiFiClientSecure*)client)->setInsecure();

    HTTPClient http;
    String fullUrl = String(config->apiUrl) + "/hardware/register";
    
    DEBUG_PRINTLN("--- [API] Polling Registration START ---");
    DEBUG_PRINTLN("URL: ", fullUrl);

    http.begin(*client, fullUrl);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(5000);

    JsonDocument doc;
    doc["local_id"] = config->localId;
    doc["integration_source"] = "esp32";
    JsonObject meta = doc["meta"].to<JsonObject>();
    meta["firmware_version"] = FIRMWARE_VERSION;
    meta["device_type"] = DEVICE_TYPE;
    
    String payload;
    serializeJson(doc, payload);
    DEBUG_PRINTLN("Payload: ", payload);

    int httpResponseCode = http.POST(payload);
    DEBUG_PRINTLN("HTTP Code: ", httpResponseCode);

    ClaimStatus result = CLAIM_ERROR;

    if (httpResponseCode == 200 || httpResponseCode == 201) {
        String response = http.getString();
        DEBUG_PRINTLN("Response Body: ", response);
        
        JsonDocument resDoc;
        if (deserializeJson(resDoc, response) == DeserializationError::Ok) {
            String status = resDoc["status"].as<String>();
            
            if (status == "pending_claim") {
                String claimCode = resDoc["claim_code"].as<String>();
                if (outClaimCode) *outClaimCode = claimCode;
                result = CLAIM_PENDING;
            } 
            else if (status == "claimed") {
                if (resDoc["api_key"].is<const char*>() && resDoc["device_id"].is<const char*>()) {
                    config->saveIdentity(resDoc["api_key"], resDoc["device_id"]);
                    result = CLAIM_SUCCESS;
                } else {
                    // Backend says it's claimed, but won't give us a key. This is the recovery case.
                    result = CLAIM_RECOVERY_NEEDED;
                }
            }
        }
    }
    
    DEBUG_PRINTLN("--- [API] Polling Registration END ---");
    http.end();
    delete client;
    return result;
}


// --- FETCH CONFIG ------------------------------------------------------------

bool ApiClient::fetchConfig(ConfigManager* config) {
    if (WiFi.status() != WL_CONNECTED) {
        DEBUG_PRINTLN("[API] WiFi lost. Skipping config fetch.");
        return false;
    }

    WiFiClient *client = nullptr;
    String urlString = String(config->apiUrl);

    if (urlString.startsWith("https")) {
        client = new WiFiClientSecure;
        ((WiFiClientSecure*)client)->setInsecure();
    } else {
        client = new WiFiClient;
    }

    HTTPClient http;
    String fullUrl = urlString + "/telemetry/config";
    
    DEBUG_PRINTLN("--- [API] Fetching Config START ---");
    DEBUG_PRINTLN("URL: ", fullUrl);

    http.begin(*client, fullUrl);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-API-KEY", config->apiKey);
    http.setTimeout(5000); 

    JsonDocument doc;
    doc["device_id"] = config->deviceId;
    
    String payload;
    serializeJson(doc, payload);
    DEBUG_PRINTLN("Payload: ", payload);

    unsigned long startReq = millis();
    int httpResponseCode = http.POST(payload);
    unsigned long duration = millis() - startReq;

    DEBUG_PRINTLN("HTTP Code: ", httpResponseCode);
    DEBUG_PRINTLN(" (Took ", duration, " ms)");
    
    bool success = false;

    if (httpResponseCode == 401 || httpResponseCode == 404) {
        config->apiFailureCount++;
        DEBUG_PRINTLN("[API] Auth failure count: ", config->apiFailureCount);
        if (config->apiFailureCount >= 3) {
            DEBUG_PRINTLN("!!! REVOKED !!! Factory Resetting...");
            config->factoryReset();
            delay(1000);
            ESP.restart();
        }
    } 
    else if (httpResponseCode == 200) {
        String response = http.getString();
        DEBUG_PRINTLN("Response Body: ", response);
        
        // Save the entire response body as the config JSON
        config->saveConfig(response.c_str());
        config->apiFailureCount = 0;
        success = true;
        DEBUG_PRINTLN(">>> CONFIG FETCHED AND SAVED!");
    } else {
        DEBUG_PRINTLN("Request Failed: ", http.errorToString(httpResponseCode));
    }
    
    DEBUG_PRINTLN("--- [API] Fetching Config END ---");

    http.end();
    delete client;
    
    return success;
}


// --- REGISTER DEVICES --------------------------------------------------------

void ApiClient::registerDevices(ConfigManager* config, const std::vector<OasisDevice*>& devices) {
    if (WiFi.status() != WL_CONNECTED) return;

    DEBUG_PRINTLN("--- [API] Syncing Devices with Cloud ---");

    for (auto device : devices) {
        if (strlen(device->getLocalId()) == 0) continue; 

        WiFiClient *client = (String(config->apiUrl).startsWith("https")) ? new WiFiClientSecure : new WiFiClient;
        if (String(config->apiUrl).startsWith("https")) ((WiFiClientSecure*)client)->setInsecure();

        HTTPClient http;
        String fullUrl = String(config->apiUrl) + "/devices/" + String(config->deviceId) + "/sensors?auto_provision=true";
        
        http.begin(*client, fullUrl);
        http.addHeader("Content-Type", "application/json");
        http.addHeader("X-API-KEY", config->apiKey);

        // Build the payload using modern JsonDocument
        JsonDocument doc;
        doc["local_id"] = device->getLocalId();
        doc["integration_source"] = "esp32";
        doc["name"] = String("Device ") + String(device->getLocalId());
        
        switch (device->getType()) {
            case DEVICE_TYPE_SENSOR_DALLAS: doc["type"] = "temp_in"; break;
            case DEVICE_TYPE_ACTUATOR_RELAY: doc["type"] = "relay"; break;
            default: doc["type"] = "generic"; break;
        }

        // Modern syntax for nested objects
        JsonObject metaObj = doc["meta"].to<JsonObject>();
        device->populateMeta(metaObj);

        String payload;
        serializeJson(doc, payload);
        DEBUG_PRINTLN("[API] Registering: ", device->getLocalId());
        
        int httpCode = http.POST(payload);
        
        if (httpCode == 200 || httpCode == 201 || httpCode == 409) {
            String response = http.getString();
            JsonDocument resDoc;
            if (deserializeJson(resDoc, response) == DeserializationError::Ok) {
                
                if (resDoc["device_id"].is<const char*>()) {
                    device->setGlobalId(resDoc["device_id"].as<const char*>());
                }

                if (resDoc["is_active"].is<bool>()) {
                    device->setActive(resDoc["is_active"].as<bool>());
                } else {
                    device->setActive(true); 
                }

                if (resDoc["meta"].is<JsonObject>()) {
                    device->applyMeta(resDoc["meta"]);
                }
                
                JsonDocument finalMetaDoc;
                JsonObject finalMetaObj = finalMetaDoc.to<JsonObject>();
                device->populateMeta(finalMetaObj);

                config->saveDeviceState(device->getLocalId(), device->getGlobalId(), device->isActive(), finalMetaObj);
                
                DEBUG_PRINTLN("      -> Global ID: ", device->getGlobalId());
                DEBUG_PRINTLN("      -> Active: ", device->isActive() ? "TRUE" : "FALSE");
            }
        } else {
            DEBUG_PRINTLN("[API] Error Response: ", http.getString());
        }
        
        http.end();
        delete client;
    }
    DEBUG_PRINTLN("--- [API] Device Sync END ---");
}


// --- REPORT RECOVERY ---------------------------------------------------------

bool ApiClient::reportRecovery(ConfigManager* config) {
    if (WiFi.status() != WL_CONNECTED) {
        DEBUG_PRINTLN("[API] WiFi lost. Cannot report recovery.");
        return false;
    }

    WiFiClient *client = (String(config->apiUrl).startsWith("https")) ? new WiFiClientSecure : new WiFiClient;
    if (String(config->apiUrl).startsWith("https")) ((WiFiClientSecure*)client)->setInsecure();

    HTTPClient http;
    String fullUrl = String(config->apiUrl) + "/hardware/report-recovery";
    
    DEBUG_PRINTLN("--- [API] Reporting Recovery Needed ---");
    http.begin(*client, fullUrl);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(5000);

    JsonDocument doc;
    doc["local_id"] = config->localId;
    doc["integration_source"] = "esp32";
    
    String payload;
    serializeJson(doc, payload);
    
    int httpCode = http.POST(payload);
    DEBUG_PRINTLN("Report Recovery HTTP Code: ", httpCode);
    
    http.end();
    delete client;

    // Return true only if backend acknowledged the report
    return (httpCode == 202);
}


// --- POLL ACTIONS ------------------------------------------------------------

bool ApiClient::pollActions(ConfigManager* config, float* outModulation) {
    if (WiFi.status() != WL_CONNECTED) {
        config->cloudTimeoutCount++; // Increment on WiFi loss
        return false;
    }

    WiFiClient *client = (String(config->apiUrl).startsWith("https")) ? new WiFiClientSecure : new WiFiClient;
    if (String(config->apiUrl).startsWith("https")) ((WiFiClientSecure*)client)->setInsecure();

    HTTPClient http;
    String fullUrl = String(config->apiUrl) + "/hardware/" + String(config->deviceId) + "/action";
    
    http.begin(*client, fullUrl);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-API-KEY", config->apiKey);
    http.setTimeout(3000); 

    int httpCode = http.GET();
    bool actionReceived = false;

    if (httpCode == 200) {
        String response = http.getString();
        JsonDocument doc;
        if (deserializeJson(doc, response) == DeserializationError::Ok) {
            if (doc["modulation"].is<float>()) {
                *outModulation = doc["modulation"].as<float>();
                actionReceived = true;
                config->apiFailureCount = 0; 
                config->cloudTimeoutCount = 0; // SUCCESS: Reset failsafe counter
            }
        }
    } else if (httpCode == 401 || httpCode == 403 || httpCode == 404) {
        config->apiFailureCount++;
        config->cloudTimeoutCount = 0; // Auth error means cloud is reachable, so no failsafe needed
        DEBUG_PRINTLN("[API] Action poll auth error: ", httpCode, " Count: ", config->apiFailureCount);
    } else {
        // Network error, timeout, 500, etc.
        config->cloudTimeoutCount++;
        DEBUG_PRINTLN("[API] Action poll network error: ", httpCode, " Failsafe Count: ", config->cloudTimeoutCount);
    }
    
    http.end();
    delete client;
    return actionReceived;
}


// --- SEND TELEMETRY ----------------------------------------------------------

void ApiClient::sendTelemetry(ConfigManager* config, String payload) {
    if (WiFi.status() != WL_CONNECTED) {
        DEBUG_PRINTLN("[API] WiFi lost. Skipping telemetry.");
        return;
    }

    WiFiClient *client = (String(config->apiUrl).startsWith("https")) ? new WiFiClientSecure : new WiFiClient;
    if (String(config->apiUrl).startsWith("https")) ((WiFiClientSecure*)client)->setInsecure();

    HTTPClient http;
    String fullUrl = String(config->apiUrl) + "/telemetry";
    
    DEBUG_PRINTLN("--- [API] Sending Telemetry START ---");
    http.begin(*client, fullUrl);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-API-KEY", config->apiKey);
    http.setTimeout(5000); 

    int httpResponseCode = http.POST(payload);
    DEBUG_PRINTLN("HTTP Code: ", httpResponseCode);
    
    if (httpResponseCode == 401 || httpResponseCode == 403 || httpResponseCode == 404) {
        config->apiFailureCount++;
        DEBUG_PRINTLN("[API] Telemetry auth error. Count: ", config->apiFailureCount);
    } else if (httpResponseCode > 0) {
        config->apiFailureCount = 0;
    }
    
    DEBUG_PRINTLN("--- [API] Sending Telemetry END ---");
    http.end();
    delete client;
}


// --- FETCH SCHEDULE ----------------------------------------------------------

bool ApiClient::fetchSchedule(ConfigManager* config, ScheduleManager* scheduleManager) {
    if (WiFi.status() != WL_CONNECTED) return false;

    WiFiClient *client = (String(config->apiUrl).startsWith("https")) ? new WiFiClientSecure : new WiFiClient;
    if (String(config->apiUrl).startsWith("https")) ((WiFiClientSecure*)client)->setInsecure();

    HTTPClient http;
    // GET /api/v1/devices/{device_id}/schedule
    String fullUrl = String(config->apiUrl) + "/devices/" + String(config->deviceId) + "/schedule";
    
    DEBUG_PRINTLN("--- [API] Fetching Schedule ---");
    DEBUG_PRINTLN("URL: ", fullUrl);

    http.begin(*client, fullUrl);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-API-KEY", config->apiKey);
    http.setTimeout(5000);

    int httpCode = http.GET();
    bool success = false;

    if (httpCode == 200) {
        String response = http.getString();
        DEBUG_PRINTLN("Schedule received. Size: ", response.length());
        
        if (scheduleManager->updateSchedule(response.c_str())) {
            DEBUG_PRINTLN("Schedule updated successfully.");
            success = true;
        } else {
            DEBUG_PRINTLN("Failed to parse schedule.");
        }
    } else {
        DEBUG_PRINTLN("Schedule fetch failed. HTTP: ", httpCode);
    }
    
    http.end();
    delete client;
    return success;
}