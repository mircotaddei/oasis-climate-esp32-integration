#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <Arduino.h>
#include <time.h>


// --- TIME MANAGER ------------------------------------------------------------

class TimeManager {
public:
    TimeManager();
    void begin();
    void update();
    bool isTimeSet();
    unsigned long getEpoch();
    String getFormattedTime();
    static String epochToISO8601(unsigned long epoch);

private:
    bool _isTimeSet;
    const char* _ntpServer = "pool.ntp.org";
    const long _gmtOffset_sec = 0; // UTC
    const int _daylightOffset_sec = 0; // No DST handling here, backend handles timezones
};

#endif