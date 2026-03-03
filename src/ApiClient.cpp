#include "ApiClient.h"
#include <ArduinoJson.h>


// --- POLL REGISTRATION -------------------------------------------------------

bool ApiClient::pollRegistration(ConfigManager* config, String* outClaimCode) {
    if (WiFi.status() != WL_CONNECTED) {
        DEBUG_PRINTLN("[API] WiFi lost. Skipping poll.");
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
    String fullUrl = urlString + "/hardware/register";
    
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

    unsigned long startReq = millis();
    int httpResponseCode = http.POST(payload);
    unsigned long duration = millis() - startReq;

    DEBUG_PRINTLN("HTTP Code:  (Took %lu ms)", httpResponseCode, duration);

    bool isClaimed = false;
    if (httpResponseCode == 200 || httpResponseCode == 201) {
        String response = http.getString();
        JsonDocument resDoc;
        if (deserializeJson(resDoc, response) == DeserializationError::Ok) {
            String status = resDoc["status"].as<String>();
            if (status == "pending_claim") {
                String claimCode = resDoc["claim_code"].as<String>();
                if (outClaimCode) *outClaimCode = claimCode;
                DEBUG_PRINTLN("*** WAITING FOR CLAIM. CODE:  ***", claimCode);
            } else if (status == "claimed") {
                config->saveIdentity(resDoc["api_key"], resDoc["device_id"]);
                isClaimed = true;
            }
        }
    }
    http.end();
    delete client;
    return isClaimed;
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


// --- REGISTER SENSORS --------------------------------------------------------

void ApiClient::registerSensors(ConfigManager* config, const std::vector<SensorDriver*>& sensors) {
    if (WiFi.status() != WL_CONNECTED) return;

    DEBUG_PRINTLN("--- [API] Syncing Sensors with Cloud ---");

    for (auto sensor : sensors) {
        if (strlen(sensor->getId()) == 0) continue; 

        WiFiClient *client = (String(config->apiUrl).startsWith("https")) ? new WiFiClientSecure : new WiFiClient;
        if (String(config->apiUrl).startsWith("https")) ((WiFiClientSecure*)client)->setInsecure();

        HTTPClient http;
        // UPDATED: Added auto_provision=true query parameter
        String fullUrl = String(config->apiUrl) + "/devices/" + String(config->deviceId) + "/sensors?auto_provision=true";
        
        http.begin(*client, fullUrl);
        http.addHeader("Content-Type", "application/json");
        http.addHeader("X-API-KEY", config->apiKey);
        http.setTimeout(5000);

        JsonDocument doc;
        doc["local_id"] = sensor->getId();
        doc["integration_source"] = "esp32";
        doc["name"] = "Sensor " + String(sensor->getId());
        doc["type"] = (sensor->getType() == SENSOR_TYPE_DALLAS) ? "temp_in" : "temp_in";

        String payload;
        serializeJson(doc, payload);
        DEBUG_PRINTLN("[API] Registering sensor: ", sensor->getId());
        
        int httpCode = http.POST(payload);
        DEBUG_PRINTLN("[API] HTTP Response: ", httpCode);
        
        if (httpCode == 200 || httpCode == 201 || httpCode == 409) {
            String response = http.getString();
            JsonDocument resDoc;
            if (deserializeJson(resDoc, response) == DeserializationError::Ok) {
                
                if (resDoc["device_id"].is<const char*>()) {
                    sensor->setGlobalId(resDoc["device_id"].as<const char*>());
                }

                if (resDoc["is_active"].is<bool>()) {
                    sensor->setActive(resDoc["is_active"].as<bool>());
                } else {
                    sensor->setActive(true); 
                }

                if (resDoc["meta"].is<JsonObject>() && resDoc["meta"]["offset"].is<float>()) {
                    sensor->setOffset(resDoc["meta"]["offset"].as<float>());
                }
                
                config->saveSensorState(sensor->getId(), sensor->getGlobalId(), sensor->isActive(), sensor->getOffset());
                
                DEBUG_PRINTLN("      -> Global ID: ", sensor->getGlobalId());
                DEBUG_PRINTLN("      -> Active: ", sensor->isActive() ? "TRUE" : "FALSE");
                DEBUG_PRINTLN("      -> Offset: ", sensor->getOffset());
            }
        }
        
        http.end();
        delete client;
    }
    DEBUG_PRINTLN("--- [API] Sensor Sync END ---");
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
    DEBUG_PRINTLN("URL: ", fullUrl);

    http.begin(*client, fullUrl);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-API-KEY", config->apiKey);
    http.setTimeout(5000); 

    DEBUG_PRINTLN("Payload: ", payload);

    unsigned long startReq = millis();
    int httpResponseCode = http.POST(payload);
    unsigned long duration = millis() - startReq;

    DEBUG_PRINTLN("HTTP Code: ", httpResponseCode, " (Took ", duration, " ms)");
    
    if (httpResponseCode == 401 || httpResponseCode == 404) {
        config->apiFailureCount++;
        if (config->apiFailureCount >= 3) {
            DEBUG_PRINTLN("!!! REVOKED !!! Factory Resetting...");
            config->factoryReset();
            delay(1000);
            ESP.restart();
        }
    } else if (httpResponseCode > 0) {
        DEBUG_PRINTLN("Response Body: ", http.getString());
        config->apiFailureCount = 0;
    }
    
    DEBUG_PRINTLN("--- [API] Sending Telemetry END ---");
    http.end();
    delete client;
}


// --- POLL ACTIONS ------------------------------------------------------------

bool ApiClient::pollActions(ConfigManager* config, float* outModulation) {
    if (WiFi.status() != WL_CONNECTED) return false;

    WiFiClient *client = (String(config->apiUrl).startsWith("https")) ? new WiFiClientSecure : new WiFiClient;
    if (String(config->apiUrl).startsWith("https")) ((WiFiClientSecure*)client)->setInsecure();

    HTTPClient http;
    // GET /api/v1/hardware/{device_id}/action
    String fullUrl = String(config->apiUrl) + "/hardware/" + String(config->deviceId) + "/action";
    
    http.begin(*client, fullUrl);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-API-KEY", config->apiKey);
    http.setTimeout(3000); // Fast timeout for actions

    int httpCode = http.GET();
    bool actionReceived = false;

    if (httpCode == 200) {
        String response = http.getString();
        JsonDocument doc;
        if (deserializeJson(doc, response) == DeserializationError::Ok) {
            if (doc["modulation"].is<float>()) {
                *outModulation = doc["modulation"].as<float>();
                actionReceived = true;
                DEBUG_PRINTLN("[API] Action received. Modulation: ", *outModulation);
            }
        }
    } else if (httpCode == 401 || httpCode == 404) {
        // Handle revocation logic here if needed, or rely on telemetry to catch it
        DEBUG_PRINTLN("[API] Action poll failed auth: ", httpCode);
    }
    
    http.end();
    delete client;
    return actionReceived;
}