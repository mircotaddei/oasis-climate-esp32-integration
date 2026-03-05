#ifndef SCHEDULE_MANAGER_H
#define SCHEDULE_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include "TimeManager.h"

// --- SCHEDULE EVENT STRUCT ---------------------------------------------------

struct ScheduleEvent {
    uint8_t hour;   // 0-23
    uint8_t minute; // 0-59
    float setpoint; // Target temperature
};

// --- SCHEDULE MANAGER CLASS --------------------------------------------------

class ScheduleManager {
public:
    ScheduleManager();
    void begin();
    
    // Parses and saves the schedule JSON received from backend
    bool updateSchedule(const char* jsonString);
    
    // Returns the target setpoint for the current time
    // Returns NAN if no schedule is active or time is invalid
    float getCurrentSetpoint(TimeManager* timeManager);

private:
    // 7 days, vector of events for each day (0=Sunday, 1=Monday...)
    std::vector<ScheduleEvent> _weeklySchedule[7];
    
    float _defaultSetpoint; // Fallback if no event matches
    
    void clearSchedule();
    bool loadFromFilesystem();
    bool saveToFilesystem(const char* jsonString);
};

#endif // SCHEDULE_MANAGER_H