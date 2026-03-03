#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <DNSServer.h> // NEW: Include DNS Server
#include "ConfigManager.h"


// --- NETWORK MANAGER ---------------------------------------------------------

class NetworkManager {
public:
    NetworkManager();
    void connect(ConfigManager* config);
    void startClaimingPortal(const char* claimCode);
    void handleClaimingPortal();
    void stopClaimingPortal();

private:
    WebServer* _server;
    DNSServer* _dnsServer;
    bool _isPortalActive;
    
    void connectWokwi();
    void connectPhysical(ConfigManager* config);
    void handleRoot();
};

#endif