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
TelemetryBuffer telemetryBuffer;


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
    float hysteresis = configManager.failsafeHysteresis;

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


// --- ON DEVICE EVENT ---------------------------------------------------------

void onDeviceEvent(OasisDevice* device, float value) {
    unsigned long now = timeManager.getEpoch();
    telemetryBuffer.add(now, device, value);
    DEBUG_PRINTLN("[EVENT] Device ", device->getLocalId(), " emitted value: ", value);
}


// --- PERFORM FULL SYNC -------------------------------------------------------

bool performFullSync() {
    DEBUG_PRINTLN("[SYNC] Starting full configuration sync...");
    
    if (apiClient.fetchConfig(&configManager)) {
        DEBUG_PRINTLN("[SYNC] Config fetched. Syncing Thermostat state...");
        apiClient.updateThermostatConfig(&configManager);
        
        DEBUG_PRINTLN("[SYNC] Registering Devices...");
        apiClient.registerDevices(&configManager, hardwareManager.getAllDevices());
        
        DEBUG_PRINTLN("[SYNC] Fetching Schedule...");
        apiClient.fetchSchedule(&configManager, &scheduleManager);
        
        DEBUG_PRINTLN("[SYNC] Full sync complete.");
        return true;
    }
    
    DEBUG_PRINTLN("[SYNC] Full sync failed at fetchConfig.");
    return false;
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
    hardwareManager.setTelemetryCallback(onDeviceEvent);

    // 5. Filesystem & Schedule
    DEBUG_PRINTLN("[SETUP] Initializing Schedule Manager...");
    scheduleManager.begin();

    // 6. Telemetry Buffer
    DEBUG_PRINTLN("[SETUP] Initializing Telemetry Buffer...");
    configManager.telemetryAutoBufferSize = telemetryBuffer.init(configManager.telemetryBufferSize);

    // 7. Connectivity (WiFi)
    DEBUG_PRINTLN("[SETUP] Connecting to WiFi...");
    networkManager.connect(&configManager, "OASIS_INIT");
    DEBUG_PRINTLN("[SETUP] WiFi Connection routine finished.");

    // 8. Network Services (NTP)
    // Note: TimeManager was begun early for logging, but update() in loop handles NTP sync
    DEBUG_PRINTLN("[SETUP] Time Manager ready for NTP sync.");

    // 9. Initial State Determination
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
    static unsigned long lastScheduleMillis = 0;
    static unsigned long lastDiagnosticMillis = 0;
    static unsigned long lastConfigSyncMillis = 0;
    static bool isFirstTelemetryPending = true;
    static bool recoveryReported = false;

    checkFactoryReset();
    hardwareManager.update();
    ledManager.update();
    networkManager.handleClaimingPortal();
    timeManager.update();

    // Global Check: If API fails too many times, drop to Recovery
    if (configManager.apiFailureCount >= configManager.maxAuthFailures && currentState != STATE_RECOVERY) {
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
    if (millis() - lastHeartbeat > configManager.heartbeatIntervalMs) {
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
            if (lastPollMillis == 0 || millis() - lastPollMillis > configManager.claimPollMs) {
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
            if (millis() - lastPollMillis > configManager.recoveryPollMs) { // Poll every 10s in recovery
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

        case STATE_PROVISIONING:
            if (lastPollMillis == 0 || millis() - lastPollMillis > configManager.provisioningRetryMs) {
                
                lastPollMillis = millis();
                DEBUG_PRINTLN("\n[MAIN] Triggering Config Fetch...");
                
                if (performFullSync()) {
                    DEBUG_PRINTLN("[MAIN] Provisioning Success! -> STATE_RUNNING");
                    currentState = STATE_RUNNING;
                    ledManager.setState(LED_ON); 
                    isFirstTelemetryPending = true;
                    lastTelemetryMillis = millis(); 
                    lastConfigSyncMillis = millis();
                } else {
                    DEBUG_PRINTLN("[MAIN] Provisioning failed. Retrying...");
                }

            }
            break;

        case STATE_RUNNING: {
            // 1. Sampling Loop (Fast - e.g., every 60s)
            // ONLY for continuous analog sensors. Actuators use the callback.
            unsigned long sampleInterval = configManager.sensorSampleMs; 
            if (millis() - lastSampleMillis > sampleInterval) {
                lastSampleMillis = millis();
                DEBUG_PRINTLN("[MAIN] Sampling Devices...");
                
                const auto& devices = hardwareManager.getAllDevices();
                unsigned long now = timeManager.getEpoch();
                
                for (auto dev : devices) {
                    // Only sample continuous sensors here
                    if (dev->getType() == DEVICE_TYPE_SENSOR_DALLAS || dev->getType() == DEVICE_TYPE_SENSOR_DHT) {
                        if (dev->isActive() && dev->isConnected()) {
                            float val = dev->getTelemetryValue();
                            if (!isnan(val)) {
                                telemetryBuffer.add(now, dev, val);
                            }
                        }
                    }
                }
            }

            // 2. Telemetry Loop (Slow)
            unsigned long interval = isFirstTelemetryPending ? 5000 : configManager.telemetryIntervalMs;
            if (millis() - lastTelemetryMillis > interval) {
                lastTelemetryMillis = millis();
                isFirstTelemetryPending = false;
                
                if (!telemetryBuffer.isEmpty()) {
                    DEBUG_PRINTLN("\n[MAIN] Triggering Batch Telemetry...");
                    
                    // Keep sending chunks until buffer is empty or an error occurs
                    while (!telemetryBuffer.isEmpty()) {
                        String payload;
                        int count = telemetryBuffer.getPayload(configManager.deviceId, payload, configManager.telemetryMaxBatchSize);
                        
                        if (count > 0) {
                            DEBUG_PRINTLN("[MAIN] Sending chunk of ", count, " records...");
                            if (apiClient.sendTelemetry(&configManager, payload)) {
                                telemetryBuffer.removeOldest(count);
                            } else {
                                DEBUG_PRINTLN("[MAIN] Telemetry failed. Retrying next cycle.");
                                break; // Stop sending if one chunk fails
                            }
                        } else {
                            break; // Should not happen if buffer not empty
                        }
                        
                        // Small delay between chunks to let WiFi stack breathe
                        delay(100); 
                    }
                }
            }

            // 3. Action Loop (Fast) - Poll every 30 seconds
            if (millis() - lastActionMillis > configManager.actionPollMs) {
                lastActionMillis = millis();
                DEBUG_PRINTLN("[MAIN] STATE RUNNING - Action Loop");
                float modulation = 0.0;

                ActionResponse action = apiClient.pollActions(&configManager);
                
                if (action.success) {
                    // CLOUD AUTHORITY
                    hardwareManager.setRelayModulation(action.modulation);
                    DEBUG_PRINTLN("[MAIN] Applied cloud modulation: ", action.modulation);
                    
                    // PIGGYBACK SYNC CHECK
                    if (!action.synced) {
                        DEBUG_PRINTLN("[MAIN] Cloud indicates out-of-sync. Triggering immediate sync.");
                        performFullSync();
                        lastConfigSyncMillis = millis(); // Reset slow timer
                    }
                    
                } else {
                    // LOCAL AUTHORITY (Failsafe)
                    if (configManager.cloudTimeoutCount >= configManager.maxNetworkFailures) {
                        DEBUG_PRINTLN("[MAIN] Cloud unreachable. Running Failsafe...");
                        runLocalThermostat();
                    }
                }
            }

            // 4. Diagnostic Loop (Very Slow - e.g. every 1 hour)
            if (millis() - lastDiagnosticMillis > configManager.diagnosticIntervalMs) {
                lastDiagnosticMillis = millis();
                DEBUG_PRINTLN("\n[MAIN] Triggering Diagnostics...");
                apiClient.sendDiagnostics(&configManager, hardwareManager.getAllDevices());
            }

            // 5. Schedule Update Loop (Very Slow - e.g. every 6 hours)
            if (millis() - lastScheduleMillis > configManager.scheduleUpdateMs) {
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
