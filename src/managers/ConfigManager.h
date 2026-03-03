#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include "../utils/Logger.h"


// --- DEBUGGING MACROS --------------------------------------------------------

// Uncomment to enable debug prints on physical hardware
#define DEBUG_MODE_PHYSICAL

#if defined(WOKWI_SIMULATION) || defined(DEBUG_MODE_PHYSICAL)
    #define DEBUG_MODE
#endif

#ifdef WOKWI_SIMULATION
    // Uncomment the line below to force the device into CLAIMING state on boot
    // #define FORCE_UNCLAIMED
#endif


// --- DEBUGGING MACROS --------------------------------------------------------

#if defined(WOKWI_SIMULATION) || defined(DEBUG_MODE_PHYSICAL)
    #define DEBUG_MODE
#endif

#ifdef DEBUG_MODE
    // Raw print without timestamp or newline
    #define DEBUG_PRINT(x) Serial.print(x)

    // Smart macro: accepts multiple arguments of any type
    // Usage: DEBUG_PRINTLN("Value is: ", myInt, " and string is: ", myString);
    #define DEBUG_PRINTLN(...) Logger::println(__VA_ARGS__)
#else
    #define DEBUG_PRINT(x)
    #define DEBUG_PRINTLN(...)
#endif


// --- FIRMWARE IDENTITY -------------------------------------------------------

#define FIRMWARE_VERSION "1.0.0"
#define DEVICE_TYPE "OASIS_RELAY_V1"


// --- HARDWARE CONFIGURATION --------------------------------------------------

#define FACTORY_RESET_PIN 0       // BOOT button on ESP32 DevKit
#define FACTORY_RESET_HOLD_MS 3000 // Hold for 3 seconds to reset


// --- DEVELOPMENT FALLBACKS ---------------------------------------------------

#define FALLBACK_SSID ""
#define FALLBACK_PASS ""

// #define FALLBACK_API_URL "http://oasis-climate.com/api/v1" 
#define FALLBACK_API_URL "http://192.168.10.103:8000/api/v1" 
// #define FALLBACK_API_URL "http://host.wokwi.internal:8000/api/v1" 


// --- CONFIG MANAGER ----------------------------------------------------------

class ConfigManager {
public:
    char apiKey[128];
    char apiUrl[128];
    char macAddress[20];
    char localId[20]; // MAC without colons
    char deviceId[64]; // Global ID assigned by backend
    
    // Timing Configurations (in Milliseconds)
    unsigned long claimIntervalMs;
    unsigned long telemetryIntervalMs;
    
    // State variables
    int apiFailureCount;

    JsonDocument deviceConfig; // Holds the dynamic JSON config from backend

    ConfigManager();
    void begin();
    
    bool isClaimed();
    bool isProvisioned();
    
    void saveIdentity(const char* newApiKey, const char* newDeviceId);
    void saveConfig(const char* jsonConfigString);
    void factoryReset();

    // Generic accessors for hardware config
    int getPin(const char* function, int defaultValue);

    void saveSensorState(const char* localId, const char* globalId, bool isActive, float offset);
    bool loadSensorState(const char* localId, char* outGlobalId, size_t maxLen, bool* outIsActive, float* outOffset);

private:
    Preferences preferences;
    void loadMacAddress();

    #ifdef WOKWI_SIMULATION
    void syncNvsFromCloud();
    void syncNvsToCloud();
    #endif
};

#endif