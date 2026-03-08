#ifndef BUILD_CONFIG_H
#define BUILD_CONFIG_H


// --- SYSTEM IDENTITY ---------------------------------------------------------

#define FIRMWARE_VERSION    "0.2.0"
#define DEVICE_TYPE         "OASIS_THERMOSTAT_V1"


// --- SECURITY DEFAULTS ------------------------------------------------------
// Can be disabled in build_dev_env.h for local development.
#define ENABLE_SSL_VALIDATION


// --- HARDWARE MODULES ACTIVATION --------------------------------------------
#define ENABLE_BOILER_RELAY
#define ENABLE_DALLAS_SENSOR


// --- DEFAULT OPERATIONAL TIMINGS (Seconds/Minutes) ---------------------------

#define DEFAULT_TELEMETRY_INTERVAL_MIN  15
#define DEFAULT_SENSOR_SAMPLE_SEC       60
#define DEFAULT_ACTION_POLL_SEC         30
#define DEFAULT_CLAIM_POLL_SEC          10
#define DEFAULT_PROVISIONING_RETRY_SEC  10
#define DEFAULT_RECOVERY_POLL_SEC       30
#define DEFAULT_SCHEDULE_UPDATE_HOURS   6
#define DEFAULT_HEARTBEAT_INTERVAL_SEC  5
#define DEFAULT_HTTP_TIMEOUT_MS         5000
#define DEFAULT_CONFIG_SYNC_HOURS       6
#define DEFAULT_DIAGNOSTIC_INTERVAL_SEC 3600 


// --- DEFAULT LOGIC & RESILIENCE ----------------------------------------------

#define DEFAULT_MAX_AUTH_FAILURES           3
#define DEFAULT_MAX_NETWORK_FAILURES        3
#define DEFAULT_FAILSAFE_HYSTERESIS         0.5
#define DEFAULT_TELEMETRY_MAX_BATCH_SIZE    50


// --- SYSTEM HARDWARE PINS ----------------------------------------------------

#define FACTORY_RESET_PIN       0
#define FACTORY_RESET_HOLD_MS   3000 
#define STATUS_LED_PIN          2


// --- DEVELOPMENT MODE --------------------------------------------------------

#define DEVEL_MODE


// --- DEVELOPMENT FALLBACKS ---------------------------------------------------

#ifdef DEVEL_MODE
    #include "build_dev_env.h"
#else
    #define FALLBACK_SSID       ""
    #define FALLBACK_PASS       ""
    #define FALLBACK_API_URL    "https://oasis-climate.com/api/v1" 
#endif

#endif // BUILD_CONFIG_H