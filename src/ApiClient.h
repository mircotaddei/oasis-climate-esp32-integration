#ifndef API_CLIENT_H
#define API_CLIENT_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "managers/ConfigManager.h"
#include <vector>
#include "sensors/SensorDriver.h"

// --- API CLIENT --------------------------------------------------------------

class ApiClient {
public:
    bool pollRegistration(ConfigManager* config, String* outClaimCode = nullptr);
    bool fetchConfig(ConfigManager* config);
    void registerSensors(ConfigManager* config, const std::vector<SensorDriver*>& sensors);
    void sendTelemetry(ConfigManager* config, String payload);
    bool pollActions(ConfigManager* config, float* outModulation);
};

#endif