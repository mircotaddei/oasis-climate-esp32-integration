#include <ArduinoJson.h>
#include "ApiClient.h"
#include "build_config.h"
#include "managers/NetworkManager.h"
#include "certs/root_ca.h"


// --- EXECUTE REQUEST ---------------------------------------------------------

ApiResponse ApiClient::executeRequest(ConfigManager* config, const char* method, const char* path, String payload, bool authenticated) {
    ApiResponse response = {-1, ""};

    if (WiFi.status() != WL_CONNECTED) {
        DEBUG_PRINTLN("[API] WiFi disconnected. Aborting ", method, " ", path);
        return response;
    }

    String fullUrl = String(config->apiUrl) + path;
    WiFiClient *client = NetworkManager::createHttpClient(fullUrl);

    {
        HTTPClient http;
        
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
    meta["http_timeout_ms"] = config->httpTimeoutMs;
    meta["diagnostic_interval_seconds"] = config->diagnosticIntervalMs / 1000;

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

// --- REGISTER DEVICES --------------------------------------------------------

void ApiClient::registerDevices(ConfigManager* config, const std::vector<OasisDevice*>& devices) {
    if (WiFi.status() != WL_CONNECTED) return;

    DEBUG_PRINTLN("--- [API] Syncing Devices with Cloud ---");

    for (auto device : devices) {
        if (strlen(device->getLocalId()) == 0) continue;

        bool exists = (strlen(device->getGlobalId()) > 0);
        String method = exists ? "PATCH" : "POST";
        String path;
        
        if (exists) {
            // Update existing sensor: /api/v1/sensors/{sensor_id}
            path = "/sensors/" + String(device->getGlobalId());
        } else {
            // Register new sensor: /api/v1/devices/{thermostat_id}/sensors
            path = "/devices/" + String(config->deviceId) + "/sensors?auto_provision=true";
        }

        JsonDocument doc;
        if (!exists) {
            doc["local_id"] = device->getLocalId();
            doc["integration_source"] = "esp32";
            doc["type"] = device->getSensorType();
        }
        
        doc["name"] = String("Device ") + String(device->getLocalId());
        doc["is_active"] = device->isActive();

        JsonObject metaObj = doc["meta"].to<JsonObject>();
        device->populateMeta(metaObj);

        String payload;
        serializeJson(doc, payload);
        
        DEBUG_PRINTLN("[API] ", method, " device: ", device->getLocalId());
        ApiResponse res = executeRequest(config, method.c_str(), path.c_str(), payload, true);

        // Handle case where sensor was deleted from backend but exists in NVS
        if (exists && res.code == 404) {
            DEBUG_PRINTLN("[API] Sensor ", device->getGlobalId(), " not found on server. Re-registering...");
            device->setGlobalId(""); // FIXME Clear ID and it will be caught in the next sync cycle or retry
            // For simplicity, we skip re-POST in this same loop to avoid complexity, 
            // it will be handled in the next performFullSync or reboot.
            continue; 
        }

        if (res.code == 200 || res.code == 201 || res.code == 409) {
            JsonDocument resDoc;
            if (deserializeJson(resDoc, res.body) == DeserializationError::Ok) {
                
                if (resDoc["device_id"].is<const char*>()) {
                    device->setGlobalId(resDoc["device_id"].as<const char*>());
                }

                if (resDoc["is_active"].is<bool>()) {
                    device->setActive(resDoc["is_active"].as<bool>());
                }

                if (resDoc["meta"].is<JsonObject>()) {
                    device->applyMeta(resDoc["meta"]);
                }
                
                // Finalize and persist local state
                JsonDocument finalMetaDoc;
                JsonObject finalMetaObj = finalMetaDoc.to<JsonObject>();
                device->populateMeta(finalMetaObj);
                config->saveDeviceState(device->getLocalId(), device->getGlobalId(), device->isActive(), finalMetaObj);
            }
        }
    }
    DEBUG_PRINTLN("--- [API] Device Sync END ---");
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


// --- SEND DIAGNOSTICS --------------------------------------------------------

void ApiClient::sendDiagnostics(ConfigManager* config, const std::vector<OasisDevice*>& devices, const char* resetReason, const char* otaStatus) {
    JsonDocument doc;
    doc["device_id"] = config->deviceId;
    
    // TODO Optional ISO 8601 Timestamp
    // Assuming TimeManager is globally accessible or we pass the epoch. 
    // For simplicity, we'll let the backend assign the timestamp if we don't have it,
    // but if you want to include it, you'd need to pass TimeManager* or the string.
    // Let's omit it for now to keep the signature clean; backend defaults to func.now().
    
    // Controller Metrics
    JsonObject metrics = doc["metrics"].to<JsonObject>();
    metrics["rssi"] = WiFi.RSSI();
    metrics["free_heap"] = ESP.getFreeHeap();
    metrics["uptime_sec"] = millis() / 1000;
    
    // Controller Tags
    JsonObject tags = doc["tags"].to<JsonObject>();
    tags["firmware_version"] = FIRMWARE_VERSION;
    tags["wifi_ssid"] = WiFi.SSID();
    tags["ip_address"] = WiFi.localIP().toString();
    
    // Optional Event Tags
    if (resetReason != nullptr) {
        tags["reset_reason"] = resetReason;
    }
    if (otaStatus != nullptr) {
        tags["ota_status"] = otaStatus;
    }

    // Nested Sensor Diagnostics
    JsonArray sensors = doc["sensors"].to<JsonArray>();
    for (auto device : devices) {
        if (strlen(device->getGlobalId()) > 0) {
            JsonObject s = sensors.add<JsonObject>();
            s["device_id"] = device->getGlobalId();
            
            JsonObject sMetrics = s["metrics"].to<JsonObject>();
            JsonObject sTags = s["tags"].to<JsonObject>();
            
            device->populateDiagnostics(sMetrics, sTags);
        }
    }

    String payload;
    serializeJson(doc, payload);
    
    DEBUG_PRINTLN("--- [API] Sending Diagnostics ---");
    if (resetReason || otaStatus) {
        DEBUG_PRINTLN("Event included. Payload: ", payload);
    }
    
    ApiResponse res = executeRequest(config, "POST", "/telemetry/diagnostics", payload, true);
    
    if (res.code == 202) {
        DEBUG_PRINTLN("[API] Diagnostics accepted.");
    } else {
        DEBUG_PRINTLN("[API] Diagnostics failed: ", res.code);
    }
}


// --- POLL ACTIONS ------------------------------------------------------------

ActionResponse ApiClient::pollActions(ConfigManager* config) {
    ActionResponse result = {false, 0.0, "", true}; // Default: failed, 0 mod, synced

    String path = "/hardware/" + String(config->deviceId) + "/action";
    ApiResponse res = executeRequest(config, "GET", path.c_str(), "", true);

    if (res.code == 200) {
        JsonDocument doc;
        if (deserializeJson(doc, res.body) == DeserializationError::Ok) {
            if (doc["modulation"].is<float>()) {
                result.modulation = doc["modulation"].as<float>();
            }
            if (doc["ota_url"].is<const char*>()) {
                result.otaUrl = doc["ota_url"].as<String>();
            }
            if (doc["synced"].is<bool>()) {
                result.synced = doc["synced"].as<bool>();
            }
            
            result.success = true;
            config->apiFailureCount = 0;
            config->cloudTimeoutCount = 0;
        }
    } else if (res.code == 401 || res.code == 403 || res.code == 404) {
        config->apiFailureCount++;
        config->cloudTimeoutCount = 0;
    } else {
        config->cloudTimeoutCount++;
    }
    
    return result;
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
