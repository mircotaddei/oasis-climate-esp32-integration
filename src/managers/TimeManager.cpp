#include "TimeManager.h"
#include "ConfigManager.h"


// --- CONSTRUCTOR -------------------------------------------------------------

TimeManager::TimeManager() {
    _isTimeSet = false;
}


// --- BEGIN -------------------------------------------------------------------

void TimeManager::begin() {
    configTime(_gmtOffset_sec, _daylightOffset_sec, _ntpServer);
    DEBUG_PRINTLN("[TIME] NTP Client initialized.");
}


// --- UPDATE ------------------------------------------------------------------

void TimeManager::update() {
    if (!_isTimeSet) {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo, 0)) { // 0ms wait, non-blocking check
            _isTimeSet = true;
            DEBUG_PRINTLN("[TIME] Time synchronized: ", getFormattedTime());
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
    struct tm* timeinfo = gmtime(&rawTime); // Use gmtime for UTC
    
    char buffer[25];
    // Format: YYYY-MM-DDTHH:MM:SSZ
    strftime(buffer, 25, "%Y-%m-%dT%H:%M:%SZ", timeinfo);
    
    return String(buffer);
}
