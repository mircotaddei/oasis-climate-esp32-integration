#include "OtaManager.h"
#include "NetworkManager.h"
#include "../certs/root_ca.h"


// --- CONSTRUCTOR -------------------------------------------------------------
OtaManager::OtaManager() {}


// --- TRY UPDATE --------------------------------------------------------------

bool OtaManager::tryUpdate(ConfigManager* config, const String& url) {
    if (WiFi.status() != WL_CONNECTED || url.length() == 0) {
        return false;
    }

    DEBUG_PRINTLN("--- [OTA] Starting Firmware Update ---");
    DEBUG_PRINTLN("URL: ", url);

    WiFiClient *client = NetworkManager::createHttpClient(url);


    // Configure HTTPUpdate
    httpUpdate.rebootOnUpdate(true); // Auto-reboot on success
    
    // Optional: Add callbacks for progress reporting
    httpUpdate.onStart([]() { DEBUG_PRINTLN("[OTA] Download started..."); });
    httpUpdate.onEnd([]() { DEBUG_PRINTLN("[OTA] Download finished. Rebooting..."); });
    httpUpdate.onProgress([](int current, int total) {
        static int lastPercent = 0;
        int percent = (current * 100) / total;
        if (percent != lastPercent && percent % 10 == 0) {
            DEBUG_PRINTLN("[OTA] Progress: ", percent, "%");
            lastPercent = percent;
        }
    });
    httpUpdate.onError([](int err) {
        DEBUG_PRINTLN("[OTA] Error: ", httpUpdate.getLastErrorString().c_str());
    });

    // Execute Update
    t_httpUpdate_return ret = httpUpdate.update(*client, url);

    bool success = false;
    switch (ret) {
        case HTTP_UPDATE_FAILED:
            DEBUG_PRINTLN("[OTA] Update Failed. Error: ", httpUpdate.getLastErrorString().c_str());
            break;
        case HTTP_UPDATE_NO_UPDATES:
            DEBUG_PRINTLN("[OTA] No updates available.");
            break;
        case HTTP_UPDATE_OK:
            DEBUG_PRINTLN("[OTA] Update OK."); // Will likely not print due to auto-reboot
            success = true;
            break;
    }

    delete client;
    return success;
}