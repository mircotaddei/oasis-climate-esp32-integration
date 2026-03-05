#include "TimeManager.h"
#include "ConfigManager.h" 


// --- CONSTRUCTOR -------------------------------------------------------------

TimeManager::TimeManager() {
    _isTimeSet = false;
}


// --- BEGIN -------------------------------------------------------------------

void TimeManager::begin() {
    // Do not call configTime here as it can be blocking.
    // It will be called in update() once WiFi is connected.
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
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return "N/A";
    }
    char buffer[30];
    strftime(buffer, 30, "%Y-%m-%d %H:%M:%S", &timeinfo);
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