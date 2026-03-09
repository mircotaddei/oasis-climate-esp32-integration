#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <Arduino.h>
#include <time.h>


// --- TIME MANAGER ------------------------------------------------------------

class TimeManager {
public:
    TimeManager();

    void begin(long gmtOffset, int daylightOffset, const char* ntpServer);
    void update();
    
    void applyTimezone(long gmtOffset, int daylightOffset);

    bool isTimeSet();
    unsigned long getEpoch();
    String getFormattedTime();
    static String epochToISO8601(unsigned long epoch);

private:
    bool _isTimeSet;
    const char* _ntpServer = "pool.ntp.org";
    long _gmtOffset_sec = 0; // UTC
    int _daylightOffset_sec = 0; // No DST handling here, backend handles timezones
};

#endif