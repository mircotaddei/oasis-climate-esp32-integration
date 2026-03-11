#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

// --- FALLBACK TAG ------------------------------------------------------------
// If a .cpp file forgets to define LOG_TAG before including headers, use this.
#ifndef LOG_TAG
    #define LOG_TAG "MISSING_TAG"
#endif

namespace Logger {

    // Function pointer type for time provider
    typedef String (*TimeProvider)();
    
    // External declarations
    extern TimeProvider _timeProvider;
    extern String _activeDebugTags; // Populated by ConfigManager
    
    void printTimestamp();

    inline void setTimeProvider(TimeProvider provider) {
        _timeProvider = provider;
    }

    // --- RUN-TIME FILTER -----------------------------------------------------
    inline bool shouldLog(const char* tag) {
        if (_activeDebugTags == "*") return true;
        if (_activeDebugTags.length() == 0) return false;
        
        // Simple substring search (e.g., "MAIN,API,HW" contains "API")
        return _activeDebugTags.indexOf(tag) >= 0;
    }

    // --- VARIADIC PRINTING ---------------------------------------------------
    inline void printArgs() {
        Serial.println();
    }

    template<typename T, typename... Args>
    inline void printArgs(T first, Args... args) {
        Serial.print(first);
        printArgs(args...); 
    }

    template<typename... Args>
    inline void println(const char* tag, Args... args) {
        Serial.print("[");
        printTimestamp();
        Serial.print("] [");
        Serial.print(tag);
        Serial.print("] ");
        printArgs(args...);
    }

} // namespace Logger

#endif // LOGGER_H