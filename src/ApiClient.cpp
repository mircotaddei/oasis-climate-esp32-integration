#include "ApiClient.h"
#include "build_config.h"
#include <ArduinoJson.h>


// --- EXECUTE REQUEST ---------------------------------------------------------

ApiResponse ApiClient::executeRequest(ConfigManager* config, const char* method, const char* path, String payload, bool authenticated) {
    ApiResponse response = {-1, ""};

    if (WiFi.status() != WL_CONNECTED) {
        DEBUG_PRINTLN("[API] WiFi disconnected. Aborting ", method, " ", path);
        return response;
    }

    WiFiClient *client = (String(config->apiUrl).startsWith("https")) ? new WiFiClientSecure : new WiFiClient;
    if (String(config->apiUrl).startsWith("https")) ((WiFiClientSecure*)client)->setInsecure();

    {
        HTTPClient http;
        String fullUrl = String(config->apiUrl) + path;
        
        DEBUG_PRINTLN("--- [API] ", method, " START ---");
        DEBUG_PRINTLN("URL: ", fullUrl);

        http.begin(*client, fullUrl);
        http.addHeader("Content-Type", "application/json");
        if (authenticated) {
            http.addHeader("X-API-KEY", config->apiKey);
        }
        http.setTimeout(config->httpTimeoutMs);

        unsigned long startReq = millis();
        
        // Print payload for methods that typically carry a body
        if (payload.length() > 0 && (strcmp(method, "POST") == 0 || strcmp(method, "PATCH") == 0 || strcmp(method, "PUT") == 0)) {
            DEBUG_PRINTLN("Payload: ", payload);
        }

        // Execute the correct HTTP method
        if (strcmp(method, "POST") == 0) {
            response.code = http.POST(payload);
        } else if (strcmp(method, "PATCH") == 0) {
            response.code = http.PATCH(payload);
        } else if (strcmp(method, "PUT") == 0) {
            response.code = http.PUT(payload);
        } else if (strcmp(method, "DELETE") == 0) {
            // DELETE usually doesn't have a body, but some APIs accept it.
            // HTTPClient's sendRequest allows custom methods with payloads.
            if (payload.length() > 0) {
                response.code = http.sendRequest("DELETE", payload);
            } else {
                response.code = http.sendRequest("DELETE");
            }
        } else {
            // Default to GET
            response.code = http.GET();
        }
        
        unsigned long duration = millis() - startReq;

        DEBUG_PRINTLN("HTTP Code: ", response.code, " (Took ", duration, " ms)");
        
        if (response.code > 0) {
            response.body = http.getString();
        } else {
            DEBUG_PRINTLN("Request Failed: ", http.errorToString(response.code));
        }

        http.end();
        DEBUG_PRINTLN("--- [API] ", method, " END ---");
        
    } // http destructor called here

    delete client; 
    return response;
}
// --- POLL REGISTRATION -------------------------------------------------------

ClaimStatus ApiClient::pollRegistration(ConfigManager* config, String* outClaimCode) {
    JsonDocument doc;
    doc["local_id"] = config->localId;
    doc["integration_source"] = "esp32";
    JsonObject meta = doc["meta"].to<JsonObject>();
    meta["firmware_version"] = FIRMWARE_VERSION;
    meta["device_type"] = DEVICE_TYPE;
    meta["telemetry_auto_buffer_size"] = config->telemetryAutoBufferSize;
    
    String payload;
    serializeJson(doc, payload);

    ApiResponse res = executeRequest(config, "POST", "/hardware/register", payload, false);

    if (res.code == 200 || res.code == 201) {
        JsonDocument resDoc;
        if (deserializeJson(resDoc, res.body) == DeserializationError::Ok) {
            String status = resDoc["status"].as<String>();
            if (status == "pending_claim") {
                if (outClaimCode) *outClaimCode = resDoc["claim_code"].as<String>();
                return CLAIM_PENDING;
            } else if (status == "claimed") {
                if (resDoc["api_key"].is<const char*>()) {
                    config->saveIdentity(resDoc["api_key"], resDoc["device_id"]);
                    return CLAIM_SUCCESS;
                }
                return CLAIM_RECOVERY_NEEDED;
            }
        }
    }
    return CLAIM_ERROR;
}


// --- FETCH CONFIG ------------------------------------------------------------

bool ApiClient::fetchConfig(ConfigManager* config) {
    JsonDocument doc;
    doc["device_id"] = config->deviceId;
    String payload;
    serializeJson(doc, payload);

    ApiResponse res = executeRequest(config, "POST", "/telemetry/config", payload, true);

    if (res.code == 200) {
        config->saveConfig(res.body.c_str());
        config->apiFailureCount = 0;
        return true;
    } else if (res.code == 401 || res.code == 403 || res.code == 404) {
        config->apiFailureCount++;
    }
    return false;
}


// --- UPDATE THERMOSTAT CONFIG ------------------------------------------------

bool ApiClient::updateThermostatConfig(ConfigManager* config) {
    JsonDocument doc;
    
    // 1. Capabilities based on compile-time flags
    JsonObject capabilities = doc["capabilities"].to<JsonObject>();
    
    #ifdef ENABLE_BOILER_RELAY
        capabilities["can_heat"] = true;
    #else
        capabilities["can_heat"] = false;
    #endif
    
    // Future proofing: if we add cooling relays
    capabilities["can_cool"] = false; 
    
    // 2. Meta block: Dump of all internal runtime configurations
    JsonObject meta = doc["meta"].to<JsonObject>();
    
    meta["telemetry_buffer_size"] = config->telemetryBufferSize;
    meta["telemetry_auto_buffer_size"] = config->telemetryAutoBufferSize;
    
    // Convert ms back to seconds/minutes for the backend representation
    meta["telemetry_interval_min"] = config->telemetryIntervalMs / 60000;
    meta["sensor_sample_sec"] = config->sensorSampleMs / 1000;
    meta["action_poll_sec"] = config->actionPollMs / 1000;
    meta["claim_poll_sec"] = config->claimPollMs / 1000;
    meta["provisioning_retry_sec"] = config->provisioningRetryMs / 1000;
    meta["recovery_poll_sec"] = config->recoveryPollMs / 1000;
    meta["schedule_update_hours"] = config->scheduleUpdateMs / 3600000;
    meta["heartbeat_interval_sec"] = config->heartbeatIntervalMs / 1000;
    
    meta["max_auth_failures"] = config->maxAuthFailures;
    meta["max_network_failures"] = config->maxNetworkFailures;
    meta["failsafe_hysteresis"] = config->failsafeHysteresis;

    String payload;
    serializeJson(doc, payload);

    String path = "/devices/" + String(config->deviceId) + "/config";
    
    // Use PATCH method as defined in OpenAPI spec
    ApiResponse res = executeRequest(config, "PATCH", path.c_str(), payload, true);

    if (res.code == 200) {
        DEBUG_PRINTLN("[API] Thermostat config synced with backend.");
        return true;
    } else {
        DEBUG_PRINTLN("[API] Failed to sync thermostat config.");
        return false;
    }
}


// --- REGISTER DEVICES --------------------------------------------------------

void ApiClient::registerDevices(ConfigManager* config, const std::vector<OasisDevice*>& devices) {
    for (auto device : devices) {
        if (strlen(device->getLocalId()) == 0) continue;

        JsonDocument doc;
        doc["local_id"] = device->getLocalId();
        doc["integration_source"] = "esp32";
        doc["name"] = String("Device ") + String(device->getLocalId());
        doc["type"] = device->getSensorType();
        JsonObject metaObj = doc["meta"].to<JsonObject>();
        device->populateMeta(metaObj);

        String payload;
        serializeJson(doc, payload);
        String path = "/devices/" + String(config->deviceId) + "/sensors?auto_provision=true";

        ApiResponse res = executeRequest(config, "POST", path.c_str(), payload, true);

        if (res.code == 200 || res.code == 201 || res.code == 409) {
            JsonDocument resDoc;
            if (deserializeJson(resDoc, res.body) == DeserializationError::Ok) {
                if (resDoc["device_id"].is<const char*>()) device->setGlobalId(resDoc["device_id"]);
                if (resDoc["is_active"].is<bool>()) device->setActive(resDoc["is_active"]);
                if (resDoc["meta"].is<JsonObject>()) device->applyMeta(resDoc["meta"]);
                
                JsonDocument finalMetaDoc;
                JsonObject finalMetaObj = finalMetaDoc.to<JsonObject>();
                device->populateMeta(finalMetaObj);
                config->saveDeviceState(device->getLocalId(), device->getGlobalId(), device->isActive(), finalMetaObj);
            }
        }
    }
}


// --- SEND TELEMETRY ----------------------------------------------------------

bool ApiClient::sendTelemetry(ConfigManager* config, String payload) {
    ApiResponse res = executeRequest(config, "POST", "/telemetry", payload, true);

    if (res.code == 401 || res.code == 403 || res.code == 404) {
        config->apiFailureCount++;
        if (config->apiFailureCount >= 3) {
            DEBUG_PRINTLN("!!! REVOKED !!! Factory Resetting...");
            config->factoryReset();
            delay(1000);
            ESP.restart();
        }
    } else if (res.code > 0) {
        config->apiFailureCount = 0;
    }
    
    return (res.code == 201);
}


// --- POLL ACTIONS ------------------------------------------------------------

bool ApiClient::pollActions(ConfigManager* config, float* outModulation) {
    String path = "/hardware/" + String(config->deviceId) + "/action";
    ApiResponse res = executeRequest(config, "GET", path.c_str(), "", true);

    if (res.code == 200) {
        JsonDocument doc;
        if (deserializeJson(doc, res.body) == DeserializationError::Ok) {
            if (doc["modulation"].is<float>()) {
                *outModulation = doc["modulation"].as<float>();
                config->apiFailureCount = 0;
                config->cloudTimeoutCount = 0;
                return true;
            }
        }
    } else if (res.code == 401 || res.code == 403 || res.code == 404) {
        config->apiFailureCount++;
        config->cloudTimeoutCount = 0;
    } else {
        config->cloudTimeoutCount++;
    }
    return false;
}


// --- REPORT RECOVERY ---------------------------------------------------------

bool ApiClient::reportRecovery(ConfigManager* config) {
    JsonDocument doc;
    doc["local_id"] = config->localId;
    doc["integration_source"] = "esp32";
    String payload;
    serializeJson(doc, payload);

    ApiResponse res = executeRequest(config, "POST", "/hardware/report-recovery", payload, false);
    return (res.code == 202);
}


// --- FETCH SCHEDULE ----------------------------------------------------------

bool ApiClient::fetchSchedule(ConfigManager* config, ScheduleManager* scheduleManager) {
    String path = "/devices/" + String(config->deviceId) + "/schedule";
    ApiResponse res = executeRequest(config, "GET", path.c_str(), "", true);

    if (res.code == 200) {
        return scheduleManager->updateSchedule(res.body.c_str());
    }
    return false;
}
