#include "ScheduleManager.h"
#include "ConfigManager.h"
#include <LittleFS.h>


// --- CONSTRUCTOR -------------------------------------------------------------

ScheduleManager::ScheduleManager() {
    _defaultSetpoint = 18.0; // Safe default
}


// --- BEGIN -------------------------------------------------------------------

void ScheduleManager::begin() {
    // Try to mount without forcing format first to check integrity
    if (!LittleFS.begin(false)) { 
        DEBUG_PRINTLN("[SCHED] Mount failed or corrupted. Attempting format...");
        if (LittleFS.format()) {
            DEBUG_PRINTLN("[SCHED] Format successful. Retrying mount...");
            if (!LittleFS.begin(true)) {
                DEBUG_PRINTLN("[SCHED] CRITICAL: Mount failed after format.");
                return;
            }
        } else {
            DEBUG_PRINTLN("[SCHED] CRITICAL: Format failed.");
            return;
        }
    }
    
    DEBUG_PRINTLN("[SCHED] LittleFS Mounted successfully.");

    if (loadFromFilesystem()) {
        DEBUG_PRINTLN("[SCHED] Schedule loaded from LittleFS.");
    } else {
        DEBUG_PRINTLN("[SCHED] No saved schedule found.");
    }
}


// --- UPDATE SCHEDULE ---------------------------------------------------------

bool ScheduleManager::updateSchedule(const char* jsonString) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonString);

    if (error) {
        DEBUG_PRINTLN("[SCHED] JSON Parsing failed: ", error.c_str());
        return false;
    }

    clearSchedule();

    // Parse "week_schedule" object
    JsonObject week = doc["week_schedule"];
    const char* days[] = {"sunday", "monday", "tuesday", "wednesday", "thursday", "friday", "saturday"};

    for (int i = 0; i < 7; i++) {
        if (week[days[i]].is<JsonArray>()) {
            JsonArray events = week[days[i]];
            for (JsonObject event : events) {
                ScheduleEvent e;
                // Parse "HH:MM" string
                String timeStr = event["start_time"].as<String>();
                int splitIndex = timeStr.indexOf(':');
                if (splitIndex != -1) {
                    e.hour = timeStr.substring(0, splitIndex).toInt();
                    e.minute = timeStr.substring(splitIndex + 1).toInt();
                    
                    // Assuming preset logic or direct temp. Let's support direct temp for now.
                    // If backend sends preset_id, we need a map. Assuming backend resolves to temp here.
                    // Or we look for "temp_heat" in the event if provided.
                    // Let's assume the backend sends a simplified structure for the device:
                    // {"start_time": "08:00", "setpoint": 21.0}
                    if (event["setpoint"].is<float>()) {
                        e.setpoint = event["setpoint"].as<float>();
                        _weeklySchedule[i].push_back(e);
                    }
                }
            }
        }
    }

    saveToFilesystem(jsonString);
    return true;
}


// --- GET CURRENT SETPOINT ----------------------------------------------------

float ScheduleManager::getCurrentSetpoint(TimeManager* timeManager) {
    if (!timeManager->isTimeSet()) return NAN;

    unsigned long epoch = timeManager->getEpoch();
    struct tm* timeinfo = localtime((time_t*)&epoch);
    
    int day = timeinfo->tm_wday; // 0-6 (Sunday is 0)
    int currentHour = timeinfo->tm_hour;
    int currentMinute = timeinfo->tm_min;

    // Find the active event for today
    float target = _defaultSetpoint;
    bool found = false;

    // Iterate events (assuming they are sorted by time)
    for (const auto& event : _weeklySchedule[day]) {
        if (currentHour > event.hour || (currentHour == event.hour && currentMinute >= event.minute)) {
            target = event.setpoint;
            found = true;
        } else {
            // Events are sorted, so if we pass current time, we stop
            break; 
        }
    }

    // If no event found for today yet (e.g. 01:00 AM but first event is 06:00 AM),
    // we should look at the LAST event of YESTERDAY.
    if (!found) {
        int prevDay = (day == 0) ? 6 : day - 1;
        if (!_weeklySchedule[prevDay].empty()) {
            target = _weeklySchedule[prevDay].back().setpoint;
        }
    }

    return target;
}


// --- CLEAR SCHEDULE ----------------------------------------------------------

void ScheduleManager::clearSchedule() {
    for (int i = 0; i < 7; i++) {
        _weeklySchedule[i].clear();
    }
}


// --- SAVE TO FILESYSTEM ------------------------------------------------------

bool ScheduleManager::saveToFilesystem(const char* jsonString) {
    File file = LittleFS.open("/schedule.json", "w");
    if (!file) return false;
    file.print(jsonString);
    file.close();
    return true;
}


// --- LOAD FROM FILESYSTEM ----------------------------------------------------

bool ScheduleManager::loadFromFilesystem() {
    
    if (!LittleFS.exists("/schedule.json")) {
        DEBUG_PRINTLN("[SCHED] No schedule file found on LittleFS.");
        return false;
    }
    
    File file = LittleFS.open("/schedule.json", "r");
    if (!file) {
        DEBUG_PRINTLN("[SCHED] ERROR: Failed to open schedule file for reading.");
        return false;
    }
    
    String json = file.readString();
    file.close();
    
    // Re-parse the loaded string to populate vectors
    // Note: This is slightly inefficient (double parsing) but keeps logic simple
    // We call the internal parsing logic, but skip saving again
    JsonDocument doc;
    deserializeJson(doc, json);
    
    clearSchedule();
    JsonObject week = doc["week_schedule"];
    const char* days[] = {"sunday", "monday", "tuesday", "wednesday", "thursday", "friday", "saturday"};

    for (int i = 0; i < 7; i++) {
        if (week[days[i]].is<JsonArray>()) {
            JsonArray events = week[days[i]];
            for (JsonObject event : events) {
                ScheduleEvent e;
                String timeStr = event["start_time"].as<String>();
                int splitIndex = timeStr.indexOf(':');
                if (splitIndex != -1) {
                    e.hour = timeStr.substring(0, splitIndex).toInt();
                    e.minute = timeStr.substring(splitIndex + 1).toInt();
                    if (event["setpoint"].is<float>()) {
                        e.setpoint = event["setpoint"].as<float>();
                        _weeklySchedule[i].push_back(e);
                    }
                }
            }
        }
    }
    return true;
}
