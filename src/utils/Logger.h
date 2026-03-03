#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>


// --- LOGGER ------------------------------------------------------------------

namespace Logger {

    // Function pointer type for time provider
    typedef String (*TimeProvider)();
    
    // Internal pointer to the callback
    static TimeProvider _timeProvider = nullptr;

    // Setup function to inject the time provider
    inline void setTimeProvider(TimeProvider provider) {
        _timeProvider = provider;
    }

    // Base case for the recursive template: prints a newline
    inline void printArgs() {
        Serial.println();
    }

    // Recursive template to print an arbitrary number of arguments
    template<typename T, typename... Args>
    inline void printArgs(T first, Args... args) {
        Serial.print(first);
        printArgs(args...); 
    }

    // Main println function
    template<typename... Args>
    inline void println(Args... args) {
        Serial.print("[");
        if (_timeProvider) {
            Serial.print(_timeProvider());
        } else {
            Serial.print(millis());
        }
        Serial.print("] ");
        printArgs(args...);
    }

} // namespace Logger

#endif