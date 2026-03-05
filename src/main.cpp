#include <Arduino.h>
#include "managers/ConfigManager.h"
#include "managers/NetworkManager.h"
#include "managers/HardwareManager.h"
#include "managers/LedManager.h"
#include "managers/TimeManager.h"
#include "managers/TelemetryBuffer.h"
#include "managers/ScheduleManager.h"
#include "ApiClient.h"
#include "utils/Logger.h"

namespace Logger {
    TimeProvider _timeProvider = nullptr;

    void printTimestamp() {
        if (_timeProvider) {
            Serial.print(_timeProvider());
        } else {
            Serial.print(millis());
        }
    }
}

// Modules
ConfigManager configManager;
NetworkManager networkManager;
ApiClient apiClient;
HardwareManager hardwareManager;
LedManager ledManager;
TimeManager timeManager;
ScheduleManager scheduleManager;
TelemetryBuffer telemetryBuffer(120);


// --- STATE MACHINE -----------------------------------------------------------

enum DeviceState {
    STATE_INIT,
    STATE_CLAIMING,
    STATE_PROVISIONING,
    STATE_RECOVERY,
    STATE_RUNNING
};

DeviceState currentState = STATE_INIT;


// Factory Reset Timer
unsigned long resetPressStartTime = 0;
bool resetPressActive = false;


String globalTimeProvider() {
    if (timeManager.isTimeSet()) {
        return timeManager.getFormattedTime();
    }
    return String(millis());
}

// --- CHECK FACTORY RESET -----------------------------------------------------

void checkFactoryReset() {

    // 1. Hardware Button Check
    if (digitalRead(FACTORY_RESET_PIN) == LOW) {
        if (!resetPressActive) {
            resetPressActive = true;
            resetPressStartTime = millis();
            DEBUG_PRINTLN("[RESET] Button pressed...");
        } else {
            unsigned long duration = millis() - resetPressStartTime;
            if (duration > FACTORY_RESET_HOLD_MS) {
                DEBUG_PRINTLN("[RESET] Hold time reached! Performing Factory Reset...");
                ledManager.setState(LED_FAST_BLINK);
                configManager.factoryReset();
                delay(1000);
                ESP.restart();
            }
        }
    } else {
        if (resetPressActive) {
            resetPressActive = false;
            DEBUG_PRINTLN("[RESET] Button released before timeout.");
        }
    }

    // 2. Serial Command Check
    if (Serial.available() > 0) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        if (command == "RESET") {
            DEBUG_PRINTLN("[RESET] Serial command received! Performing Factory Reset...");
            configManager.factoryReset();
            delay(1000);
            ESP.restart();
        }
    }
}


// --- LOCAL THERMOSTAT LOGIC --------------------------------------------------

void runLocalThermostat() {
    float currentTemp = hardwareManager.getTemperature();
    float targetSetpoint = scheduleManager.getCurrentSetpoint(&timeManager);
    float hysteresis = 0.5; // Default hysteresis

    DEBUG_PRINTLN("[FAILSAFE] Current Temp: ", currentTemp, " Setpoint: ", targetSetpoint);

    if (!isnan(currentTemp) && !isnan(targetSetpoint)) {
        if (currentTemp < (targetSetpoint - hysteresis)) {
            // if (hardwareManager.getRelayModulation() < 1.0) {
                DEBUG_PRINTLN("[FAILSAFE] Temp low (", currentTemp, "). Heat ON.");
                hardwareManager.setRelayModulation(1.0);
            // }
        } else if (currentTemp > (targetSetpoint + hysteresis)) {
            // if (hardwareManager.getRelayModulation() > 0.0) {
                DEBUG_PRINTLN("[FAILSAFE] Temp high (", currentTemp, "). Heat OFF.");
                hardwareManager.setRelayModulation(0.0);
            // }
        } else {
            DEBUG_PRINTLN("[FAILSAFE] Temp within hysteresis. No action.");
        }
    } else {
        DEBUG_PRINTLN("[FAILSAFE] Missing Temp or Setpoint. Cannot operate.");
    }
}


// --- SETUP -------------------------------------------------------------------

void setup() {
    // 1. Core System Init
    Serial.begin(115200);
    delay(100); 
    
    // Initialize TimeManager early for logging capability
    timeManager.begin();
    
    #ifdef DEBUG_MODE
    Logger::setTimeProvider(&globalTimeProvider);
    #endif

    DEBUG_PRINTLN("\n\n=== OASIS Climate ESP32 - Booting ===");

    DEBUG_PRINTLN("[SETUP] Initializing Factory Reset Pin...");
    pinMode(FACTORY_RESET_PIN, INPUT_PULLUP);

    // 2. Configuration (Must be loaded before Hardware/Network)
    DEBUG_PRINTLN("[SETUP] Initializing Config Manager...");
    configManager.begin();

    // 3. Feedback (LEDs)
    DEBUG_PRINTLN("[SETUP] Initializing LED Manager...");
    ledManager.begin();
    ledManager.setState(LED_BLINK_4); // Signal "Booting/Connecting"

    // 4. Hardware (GPIOs, Buses, Sensors)
    DEBUG_PRINTLN("[SETUP] Initializing Hardware Manager...");
    hardwareManager.begin(&configManager);

    // 5. Filesystem & Schedule
    DEBUG_PRINTLN("[SETUP] Initializing Schedule Manager...");
    scheduleManager.begin();

    // 6. Connectivity (WiFi)
    DEBUG_PRINTLN("[SETUP] Connecting to WiFi...");
    networkManager.connect(&configManager, "OASIS_INIT");
    DEBUG_PRINTLN("[SETUP] WiFi Connection routine finished.");

    // 7. Network Services (NTP)
    // Note: TimeManager was begun early for logging, but update() in loop handles NTP sync
    DEBUG_PRINTLN("[SETUP] Time Manager ready for NTP sync.");

    // 8. Initial State Determination
    DEBUG_PRINTLN("[SETUP] Determining initial state...");
    if (configManager.isClaimed()) {
        if (configManager.isProvisioned()) {
            DEBUG_PRINTLN("[MAIN] Device is claimed and provisioned. -> STATE_RUNNING");
            currentState = STATE_RUNNING;
            ledManager.setState(LED_ON);
        } else {
            DEBUG_PRINTLN("[MAIN] Device is claimed but needs config. -> STATE_PROVISIONING");
            currentState = STATE_PROVISIONING;
            ledManager.setState(LED_FAST_BLINK);
        }
    } else {
        DEBUG_PRINTLN("[MAIN] Device NOT claimed. -> STATE_CLAIMING");
        currentState = STATE_CLAIMING;
        ledManager.setState(LED_BLINK_2);
    }
    
    DEBUG_PRINTLN("[SETUP] Complete. Entering main loop.");
}


// --- LOOP --------------------------------------------------------------------

void loop() {
    static unsigned long lastPollMillis = 0;
    static unsigned long lastTelemetryMillis = 0;
    static unsigned long lastSampleMillis = 0;
    static unsigned long lastActionMillis = 0;
    static bool isFirstTelemetryPending = true;
    static bool recoveryReported = false;

    checkFactoryReset();
    hardwareManager.update();
    ledManager.update();
    networkManager.handleClaimingPortal();
    timeManager.update();

    // Global Check: If API fails too many times, drop to Recovery
    // TODO 3 in settings 
    if (configManager.apiFailureCount >= 3 && currentState != STATE_RECOVERY) {
        DEBUG_PRINTLN("[MAIN] Too many API failures. Entering RECOVERY mode.");
        configManager.apiFailureCount = 0;
        configManager.invalidateApiKey();
        recoveryReported = apiClient.reportRecovery(&configManager);
        currentState = STATE_RECOVERY;
        ledManager.setState(LED_BLINK_5);
        networkManager.startClaimingPortal(&configManager, "RECOVERY", "OASIS_RECOVERY");
    }

    // Heartbeat & Sensor Debug (every 5 seconds)
    static unsigned long lastHeartbeat = 0;
    if (millis() - lastHeartbeat > 5000) {
        lastHeartbeat = millis();
        float currentTemp = hardwareManager.getTemperature();
        if (isnan(currentTemp)) {
            DEBUG_PRINTLN("[MAIN] Heartbeat - Sensors: NO DATA");
        } else {
            DEBUG_PRINTLN("[MAIN] Heartbeat - Temp: ", currentTemp, " C");
        }
    }

    // Timers

    switch (currentState) {

        case STATE_CLAIMING:
            if (lastPollMillis == 0 || millis() - lastPollMillis > configManager.claimIntervalMs) {
                lastPollMillis = millis();
                DEBUG_PRINTLN("[MAIN] STATE CLAIMING");
                DEBUG_PRINTLN("\n[MAIN] Triggering Registration Poll...");
                
                String claimCode = "";
                ClaimStatus status = apiClient.pollRegistration(&configManager, &claimCode);
                
                if (status == CLAIM_SUCCESS) {
                    DEBUG_PRINTLN("[MAIN] Claim Success! -> STATE_PROVISIONING");
                    currentState = STATE_PROVISIONING;
                    ledManager.setState(LED_FAST_BLINK);
                    networkManager.stopClaimingPortal();
                    lastPollMillis = 0;
                } else if (status == CLAIM_PENDING) {
                    networkManager.startClaimingPortal(&configManager, claimCode.c_str(), "OASIS_CLAIMING");
                } else if (status == CLAIM_RECOVERY_NEEDED) {
                    DEBUG_PRINTLN("[MAIN] Recovery Needed! -> STATE_RECOVERY");
                    currentState = STATE_RECOVERY;
                    ledManager.setState(LED_BLINK_5);
                    networkManager.startClaimingPortal(&configManager, "RECOVERY", "OASIS_RECOVERY");
                }
            }
            break;

        case STATE_RECOVERY:
            if (millis() - lastPollMillis > 10000) { // Poll every 10s in recovery
                lastPollMillis = millis();
                
                if (!recoveryReported) {
                    DEBUG_PRINTLN("\n[MAIN] In Recovery. Attempting to report...");
                    if (apiClient.reportRecovery(&configManager)) {
                        recoveryReported = true;
                        DEBUG_PRINTLN("[MAIN] Recovery reported successfully. Now polling for unlock...");
                    }
                } else {
                    DEBUG_PRINTLN("\n[MAIN] Recovery reported. Polling for unlock...");
                    String claimCode = "";
                    ClaimStatus status = apiClient.pollRegistration(&configManager, &claimCode);
                    
                    if (status == CLAIM_PENDING) {
                        DEBUG_PRINTLN("[MAIN] Device unlocked via Dashboard! -> STATE_CLAIMING");
                        currentState = STATE_CLAIMING;
                        ledManager.setState(LED_BLINK_2);
                        recoveryReported = false; // Reset for next time
                    }
                }
            }
            break;

        // case STATE_RECOVERY:
        //     if (millis() - lastPollMillis > 30000) { // Slower poll in recovery (30s)
        //         lastPollMillis = millis();
        //         DEBUG_PRINTLN("[MAIN] STATE RECOVERY");
        //         DEBUG_PRINTLN("\n[MAIN] In Recovery. Polling for unlock...");
                
        //         String claimCode = "";
        //         ClaimStatus status = apiClient.pollRegistration(&configManager, &claimCode);
                
        //         // If the user reset, the status will change to PENDING
        //         if (status == CLAIM_PENDING) {
        //             DEBUG_PRINTLN("[MAIN] Device unlocked! -> STATE_CLAIMING");
        //             currentState = STATE_CLAIMING;
        //             ledManager.setState(LED_BLINK_2);
        //         } else {
        //             networkManager.startClaimingPortal(&configManager, "RECOVERY", "OASIS_RECOVERY");                }
        //     }
        //     break;

        case STATE_PROVISIONING:
            if (lastPollMillis == 0 || millis() - lastPollMillis > 10000) {
                lastPollMillis = millis();
                DEBUG_PRINTLN("[MAIN] STATE PROVISIONING");
                DEBUG_PRINTLN("\n[MAIN] Triggering Config Fetch...");
                
                if (apiClient.fetchConfig(&configManager)) {
                    DEBUG_PRINTLN("[MAIN] Provisioning Success! Registering Devices...");
                    
                    // FIX: Call registerDevices with getAllDevices()
                    apiClient.registerDevices(&configManager, hardwareManager.getAllDevices());
                    
                    DEBUG_PRINTLN("[MAIN] Fetching Schedule...");
                    apiClient.fetchSchedule(&configManager, &scheduleManager);

                    DEBUG_PRINTLN("[MAIN] -> STATE_RUNNING");
                    currentState = STATE_RUNNING;
                    ledManager.setState(LED_ON); 
                    
                    isFirstTelemetryPending = true;
                    lastTelemetryMillis = millis(); 
                } else {
                    DEBUG_PRINTLN("[MAIN] Config fetch failed. Retrying...");
                }
            }
            break;

        case STATE_RUNNING: {
            // 1. Sampling Loop (Fast - e.g., every 60s)
            unsigned long sampleInterval = 60000; 
            if (millis() - lastSampleMillis > sampleInterval) {
                lastSampleMillis = millis();
                DEBUG_PRINTLN("[MAIN] Sampling Devices...");
                
                const auto& devices = hardwareManager.getAllDevices();
                unsigned long now = timeManager.getEpoch();
                
                for (int i = 0; i < devices.size(); i++) {
                    OasisDevice* dev = devices[i];
                    
                    // FIX: Unified interface, no casting needed!
                    if (dev->isActive() && dev->isConnected()) {
                        float val = dev->getTelemetryValue();
                        if (!isnan(val)) {
                            telemetryBuffer.add(now, i, val);
                        }
                    }
                }
            }

            // 2. Telemetry Loop (Slow)
            if (millis() - lastTelemetryMillis > configManager.telemetryIntervalMs) {
                lastTelemetryMillis = millis();
                DEBUG_PRINTLN("[MAIN] STATE RUNNING - Telemetry Loop");
                
                if (!telemetryBuffer.isEmpty()) {
                    DEBUG_PRINTLN("\n[MAIN] Triggering Batch Telemetry...");
                    String payload = telemetryBuffer.getPayload(configManager.deviceId, &hardwareManager);
                    apiClient.sendTelemetry(&configManager, payload);
                    telemetryBuffer.clear(); 
                } else {
                    DEBUG_PRINTLN("[MAIN] Buffer empty, skipping telemetry.");
                }
            }

            // 3. Action Loop (Fast) - Poll every 30 seconds
            unsigned long actionInterval = 30000; 
            if (millis() - lastActionMillis > actionInterval) {
                lastActionMillis = millis();
                DEBUG_PRINTLN("[MAIN] STATE RUNNING - Action Loop");
                float modulation = 0.0;

                if (apiClient.pollActions(&configManager, &modulation)) {
                    // CLOUD AUTHORITY: Success
                    hardwareManager.setRelayModulation(modulation);
                    DEBUG_PRINTLN("[MAIN] Applied cloud modulation: ", modulation);
                } else {
                    // CLOUD FAILED: Check if we should trigger Failsafe
                    if (configManager.cloudTimeoutCount >= 3) {
                        DEBUG_PRINTLN("[MAIN] Cloud unreachable. Running Failsafe...");
                        runLocalThermostat();
                    } else {
                        DEBUG_PRINTLN("[MAIN] Cloud poll failed. Count: ", configManager.cloudTimeoutCount);
                    }
                }
            }

            // 4. Schedule Update Loop (Very Slow - e.g. every 6 hours)
            static unsigned long lastScheduleMillis = 0;
            if (millis() - lastScheduleMillis > (6 * 3600 * 1000)) {
                lastScheduleMillis = millis();
                DEBUG_PRINTLN("[MAIN] STATE RUNNING - Schedule Loop");
                apiClient.fetchSchedule(&configManager, &scheduleManager);
            }
            break;
        }
            
        default:
            break;
    }
}
