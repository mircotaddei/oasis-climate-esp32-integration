#ifndef BUILD_CONFIG_H
#define BUILD_CONFIG_H

// =============================================================================
// OASIS CLIMATE - FIRMWARE BUILD CONFIGURATION
// =============================================================================


// --- SYSTEM IDENTITY ---------------------------------------------------------

#define FIRMWARE_VERSION "0.1.0"
#define DEVICE_TYPE "OASIS_THERMOSTAT_V1"


// --- DEBUGGING ---------------------------------------------------------------

// Uncomment to enable debug prints on physical hardware
#define DEBUG_MODE_PHYSICAL


// --- HARDWARE MODULES ACTIVATION ---------------------------------------------
// Comment out a line to completely remove the module from the compiled firmware


// Actuators
#define ENABLE_BOILER_RELAY


// Sensors
#define ENABLE_DALLAS_SENSOR
// #define ENABLE_PIR_SENSOR   // Future expansion
// #define ENABLE_BME280       // Future expansion


// --- SYSTEM HARDWARE PINS ----------------------------------------------------

#define FACTORY_RESET_PIN 0       // BOOT button on ESP32 DevKit
#define FACTORY_RESET_HOLD_MS 3000 // Hold for 3 seconds to reset
#define STATUS_LED_PIN 2          // Built-in LED


// --- DEVELOPMENT FALLBACKS ---------------------------------------------------

#define FALLBACK_SSID ""
#define FALLBACK_PASS ""
// Replace with your computer's local IP address (e.g., 192.168.1.100)
#define FALLBACK_API_URL "http://192.168.10.103:8000/api/v1" 

#endif // BUILD_CONFIG_H