#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include "../utils/Logger.h"
#include "../build_config.h"

// --- DEBUGGING MACROS --------------------------------------------------------

#define DEBUG_MODE_PHYSICAL

#if defined(WOKWI_SIMULATION) || defined(DEBUG_MODE_PHYSICAL)
    #define DEBUG_MODE
#endif

#if defined(DEBUG_MODE) && defined(LOG_ENABLED)
    
    #define DEBUG_PRINT(x) \
        do { if (Logger::shouldLog(LOG_TAG)) Serial.print(x); } while(0)

    #define DEBUG_PRINTLN(...) \
        do { if (Logger::shouldLog(LOG_TAG)) Logger::println(LOG_TAG, __VA_ARGS__); } while(0)

#else
    // If LOG_ENABLED is not defined in the .cpp file, compile to nothing
    #define DEBUG_PRINT(x)
    #define DEBUG_PRINTLN(...)
#endif


// --- CONFIG MANAGER CLASS ----------------------------------------------------

class ConfigManager {
public:
    char apiKey[128];
    char apiUrl[128];
    char macAddress[20];
    char localId[20]; 
    char deviceId[64]; 
    
    // --- TIMING CONFIGURATIONS (in Milliseconds) ---
    unsigned long telemetryIntervalMs;
    unsigned long sensorSampleMs;
    unsigned long actionPollMs;
    unsigned long claimPollMs;
    unsigned long provisioningRetryMs;
    unsigned long recoveryPollMs;
    unsigned long scheduleUpdateMs;
    unsigned long heartbeatIntervalMs;
    unsigned long httpTimeoutMs;
    unsigned long diagnosticIntervalMs;
    unsigned long configSyncIntervalMs;

    // --- RESILIENCE & LOGIC CONFIGURATIONS ---
    int maxAuthFailures;
    int maxNetworkFailures;
    float failsafeHysteresis;
    
    // --- BUFFER & TELEMETRY CONFIG ---
    int telemetryBufferSize;      // Configured value (-1 = Auto)
    int telemetryAutoBufferSize;  // Calculated value
    unsigned long firstTelemetryDelayMs;
    int telemetryMaxBatchSize;
        bool globalSendOnDelta;

    // --- TIMEZONE CONFIGURATION ---
    char timezone[64];
    long gmtOffsetSec;
    int daylightOffsetSec;

    // --- DEBUG CONFIGURATION ---
    String activeDebugTags; // NEW: e.g., "MAIN,API" or "*"

    // State variables
    int apiFailureCount;
    int cloudTimeoutCount;

    JsonDocument deviceConfig; 

    ConfigManager();
    void begin();
    
    bool isClaimed();
    bool isProvisioned();
    
    void saveIdentity(const char* newApiKey, const char* newDeviceId);
    void invalidateApiKey();
    void saveConfig(const char* jsonConfigString);
    void factoryReset();

    int getPin(const char* function, int defaultValue);

    // Generic Device State Management
    void saveDeviceState(const char* localId, const char* globalId, bool isActive, JsonObjectConst meta);
    bool loadDeviceState(const char* localId, char* outGlobalId, size_t maxLen, bool* outIsActive, JsonObject& outMeta);

private:
    Preferences preferences;
    void loadMacAddress();

    #ifdef WOKWI_SIMULATION
    void syncNvsFromCloud();
    void syncNvsToCloud();
    #endif
};

#endif