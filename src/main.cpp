#include <Arduino.h>
#include "managers/ConfigManager.h"
#include "managers/NetworkManager.h"
#include "managers/HardwareManager.h"
#include "managers/LedManager.h"
#include "managers/TimeManager.h"
#include "managers/TelemetryBuffer.h"
#include "ApiClient.h"

// Modules
ConfigManager configManager;
NetworkManager networkManager;
ApiClient apiClient;
HardwareManager hardwareManager;
LedManager ledManager;
TimeManager timeManager;
TelemetryBuffer telemetryBuffer(120);

enum DeviceState {
    STATE_INIT,
    STATE_CLAIMING,
    STATE_PROVISIONING,
    STATE_RUNNING
};

DeviceState currentState = STATE_INIT;

// Factory Reset Timer
unsigned long resetPressStartTime = 0;
bool resetPressActive = false;


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


// --- SETUP -------------------------------------------------------------------

void setup() {
    Serial.begin(115200);
    timeManager.begin();

    #ifdef DEBUG_MODE
    Logger::setTimeProvider([]() -> String {
        if (timeManager.isTimeSet()) {
            return timeManager.getFormattedTime();
        }
        return String(millis());
    });
    #endif
    
    DEBUG_PRINTLN("\n\n=== OASIS Climate ESP32 - Booting ===");

    pinMode(FACTORY_RESET_PIN, INPUT_PULLUP);
    ledManager.begin();
    ledManager.setState(LED_BLINK_4);

    configManager.begin();
    hardwareManager.begin(&configManager);
    networkManager.connect(&configManager);
    timeManager.begin();
    

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
}

// --- LOOP --------------------------------------------------------------------

void loop() {
    // Check for Factory Reset request (Hardware or Serial)
    checkFactoryReset();

    // Update Hardware & LED
    hardwareManager.update();
    ledManager.update(); // Non-blocking LED animation
    
    // Handle Claiming Portal (if active)
    networkManager.handleClaimingPortal();
    timeManager.update(); // Keep time synced

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
    static unsigned long lastPollMillis = 0;
    static unsigned long lastTelemetryMillis = 0;
    static unsigned long lastSampleMillis = 0;
    static unsigned long lastActionMillis = 0;
    static bool isFirstTelemetryPending = true;

    switch (currentState) {
        case STATE_CLAIMING:
            if (lastPollMillis == 0 || millis() - lastPollMillis > configManager.claimIntervalMs) {
                lastPollMillis = millis();
                DEBUG_PRINTLN("\n[MAIN] Triggering Registration Poll...");
                
                String claimCode = "";
                if (apiClient.pollRegistration(&configManager, &claimCode)) {
                    DEBUG_PRINTLN("[MAIN] Claim Success! -> STATE_PROVISIONING");
                    currentState = STATE_PROVISIONING;
                    ledManager.setState(LED_FAST_BLINK);
                    networkManager.stopClaimingPortal();
                    lastPollMillis = 0;
                } else if (claimCode.length() > 0) {
                    networkManager.startClaimingPortal(claimCode.c_str());
                }
            }
            break;

        case STATE_PROVISIONING:
            if (lastPollMillis == 0 || millis() - lastPollMillis > 10000) {
                lastPollMillis = millis();
                DEBUG_PRINTLN("\n[MAIN] Triggering Config Fetch...");
                
                if (apiClient.fetchConfig(&configManager)) {
                    DEBUG_PRINTLN("[MAIN] Provisioning Success! Registering Sensors...");
                    apiClient.registerSensors(&configManager, hardwareManager.getSensors());
                    
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
            // Use configManager.telemetryIntervalMs / 15 as a heuristic, or a fixed 60s
            unsigned long sampleInterval = 60000; 
            
            if (millis() - lastSampleMillis > sampleInterval) {
                lastSampleMillis = millis();
                DEBUG_PRINTLN("[MAIN] Sampling Sensors...");
                
                const auto& sensors = hardwareManager.getSensors();
                unsigned long now = timeManager.getEpoch();
                
                for (int i = 0; i < sensors.size(); i++) {
                    if (sensors[i]->isActive() && sensors[i]->isConnected()) {
                        float val = sensors[i]->getTemperature();
                        if (!isnan(val)) {
                            telemetryBuffer.add(now, i, val);
                        }
                    }
                }
            }

            // 2. Telemetry Loop (Slow - e.g., every 15m)
            // Send whatever is in the buffer
            if (millis() - lastTelemetryMillis > configManager.telemetryIntervalMs) {
                lastTelemetryMillis = millis();
                
                if (!telemetryBuffer.isEmpty()) {
                    DEBUG_PRINTLN("\n[MAIN] Triggering Batch Telemetry...");
                    String payload = telemetryBuffer.getPayload(configManager.deviceId, &hardwareManager);
                    apiClient.sendTelemetry(&configManager, payload);
                    telemetryBuffer.clear(); // Clear buffer after sending
                } else {
                    DEBUG_PRINTLN("[MAIN] Buffer empty, skipping telemetry.");
                }
            }

            // 3. Action Loop (Fast) - Poll every 30 seconds
            unsigned long actionInterval = 30000; 
            if (millis() - lastActionMillis > actionInterval) {
                lastActionMillis = millis();
                float modulation = 0.0;
                if (apiClient.pollActions(&configManager, &modulation)) {
                    hardwareManager.setRelayState(modulation > 0.0);
                    DEBUG_PRINTLN("[MAIN] Applied modulation: ", modulation);
                }
            }
            break;
        }
            
        default:
            break;
    }
}
