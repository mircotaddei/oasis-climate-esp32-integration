#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>
#include "ConfigManager.h"

class OtaManager {
public:
    OtaManager();
    
    // Attempts to download and flash the firmware from the given URL.
    // If successful, the device will automatically reboot.
    // Returns false if the update fails.
    bool tryUpdate(ConfigManager* config, const String& url);
};

#endif // OTA_MANAGER_H