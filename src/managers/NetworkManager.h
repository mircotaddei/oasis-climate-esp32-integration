#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "ConfigManager.h"


// --- NETWORK MANAGER ---------------------------------------------------------

class NetworkManager {
public:
    NetworkManager();
    void connect(ConfigManager* config, const char* prefix = "OASIS_INIT");
    void startClaimingPortal(ConfigManager* config, const char* claimCode, const char* prefix);
    void handleClaimingPortal();
    void stopClaimingPortal();

    static WiFiClient* createHttpClient(const String& url);

private:
    WebServer* _server;
    DNSServer* _dnsServer;
    bool _isPortalActive;
    
    void connectWokwi();
    void connectPhysical(ConfigManager* config, const char* prefix);
    void handleRoot();
};

#endif