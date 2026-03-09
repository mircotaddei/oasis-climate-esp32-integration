#include "TimeManager.h"
#include "ConfigManager.h" 


// --- CONSTRUCTOR -------------------------------------------------------------

TimeManager::TimeManager() {
    _isTimeSet = false;
}


// --- BEGIN -------------------------------------------------------------------


void TimeManager::begin(long gmtOffset, int daylightOffset, const char* ntpServer) {
    _gmtOffset_sec = gmtOffset;
    _daylightOffset_sec = daylightOffset;
    _ntpServer = ntpServer;
    
    // configTime is non-blocking, it just sets up the parameters.
    // The actual sync happens in the background.
    // TODO check if true ----------------^
    configTime(_gmtOffset_sec, _daylightOffset_sec, _ntpServer);
    DEBUG_PRINTLN("[TIME] NTP Client configured. TZ Offset: ", _gmtOffset_sec);
    DEBUG_PRINTLN("[TIME] Manager initialized. Waiting for WiFi to sync NTP.");
}


// --- UPDATE ------------------------------------------------------------------

void TimeManager::update() {
    // Attempt to configure NTP only once, when WiFi is ready and time is not set.
    if (!_isTimeSet && WiFi.status() == WL_CONNECTED) {
        DEBUG_PRINTLN("[TIME] WiFi connected. Configuring NTP...");
        configTime(_gmtOffset_sec, _daylightOffset_sec, _ntpServer);
        
        // Now check for sync
        struct tm timeinfo;
        if (getLocalTime(&timeinfo, 5000)) { // Wait up to 5s for sync
            _isTimeSet = true;
            DEBUG_PRINTLN("[TIME] Time synchronized: ", getFormattedTime());
        } else {
            DEBUG_PRINTLN("[TIME] WARN: NTP sync failed or timed out.");
            // We don't set _isTimeSet, so it will retry on the next update() cycle
            // To avoid spamming, we could add a timer here, but for now this is fine.
        }
    }
}


// --- APPLY TIMEZONE ----------------------------------------------------------

void TimeManager::applyTimezone(long gmtOffset, int daylightOffset) {
    _gmtOffset_sec = gmtOffset;
    _daylightOffset_sec = daylightOffset;
    
    // Re-configure system time with new offsets
    configTime(_gmtOffset_sec, _daylightOffset_sec, _ntpServer);
    
    DEBUG_PRINTLN("[TIME] Timezone offsets updated: GMT=", _gmtOffset_sec, " DST=", _daylightOffset_sec);
}


// --- IS TIME SET -------------------------------------------------------------

bool TimeManager::isTimeSet() {
    return _isTimeSet;
}


// --- GET EPOCH ---------------------------------------------------------------

unsigned long TimeManager::getEpoch() {
    time_t now;
    time(&now);
    return now;
}


// --- GET FORMATTED TIME ------------------------------------------------------

String TimeManager::getFormattedTime() {
    time_t now;
    time(&now);
    
    // Use localtime instead of gmtime to respect the configured timezone
    struct tm* timeinfo = localtime(&now);
    
    if (timeinfo == nullptr) {
        return "N/A";
    }
    
    char buffer[30];
    strftime(buffer, 30, "%Y-%m-%d %H:%M:%S", timeinfo);
    return String(buffer);
}


// --- EPOCH TO ISO8601 --------------------------------------------------------

String TimeManager::epochToISO8601(unsigned long epoch) {
    if (epoch == 0) return "";
    
    time_t rawTime = (time_t)epoch;
    struct tm* timeinfo = gmtime(&rawTime); 
    
    char buffer[25];
    strftime(buffer, 25, "%Y-%m-%dT%H:%M:%SZ", timeinfo);
    
    return String(buffer);
}