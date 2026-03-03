#include "NetworkManager.h"


// --- GLOBALS -----------------------------------------------------------------

// Global flag for WiFiManager callback
bool shouldSaveConfig = false;
String globalClaimCode = ""; // To pass to lambda handler

void saveConfigCallback() {
    DEBUG_PRINTLN("Should save config");
    shouldSaveConfig = true;
}


// --- CONSTRUCTOR -------------------------------------------------------------

NetworkManager::NetworkManager() {
    _server = nullptr;
    _dnsServer = nullptr; // NEW: Initialize to null
    _isPortalActive = false;
}


// --- CONNECT -----------------------------------------------------------------

void NetworkManager::connect(ConfigManager* config) {
    #ifdef WOKWI_SIMULATION
        connectWokwi();
    #else
        connectPhysical(config);
    #endif
}


// --- CONNECT WOKWI -----------------------------------------------------------

void NetworkManager::connectWokwi() {
    DEBUG_PRINTLN("Running in Wokwi Simulation mode...");
    WiFi.mode(WIFI_STA); // Explicitly set Station mode for Wokwi
    WiFi.begin("Wokwi-GUEST", "", 6);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        DEBUG_PRINT(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        DEBUG_PRINTLN("\nConnected to Wokwi WiFi!");
        DEBUG_PRINTLN("IP Address: ", WiFi.localIP());
    } else {
        DEBUG_PRINTLN("\nFailed to connect to Wokwi WiFi.");
    }
}


// --- CONNECT PHYSICAL --------------------------------------------------------

void NetworkManager::connectPhysical(ConfigManager* config) {
    // Attempt fallback connection first if no credentials saved
    if (WiFi.SSID() == "" && strlen(FALLBACK_SSID) > 0) {
        DEBUG_PRINTLN("Trying fallback WiFi credentials...");
        WiFi.begin(FALLBACK_SSID, FALLBACK_PASS);
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 10) {
            delay(500);
            DEBUG_PRINT(".");
            attempts++;
        }
        DEBUG_PRINTLN("");
    }

    // Setup WiFiManager (the external library)
    WiFiManager wifiManager;
    wifiManager.setSaveConfigCallback(saveConfigCallback);

    // Start Connection/Portal
    String apName = "OASIS_" + String(config->macAddress).substring(12);
    apName.replace(":", "");

    // Set timeout to avoid blocking forever if WiFi is configured but AP not found
    wifiManager.setConfigPortalTimeout(180); 

    if (!wifiManager.autoConnect(apName.c_str())) {
        DEBUG_PRINTLN("Failed to connect and hit timeout");
        delay(3000);
        ESP.restart();
    }

    DEBUG_PRINTLN("Connected to WiFi!");
    
    // FIX: If not claimed yet, ensure AP stays ON for the claiming portal
    if (!config->isClaimed()) {
        DEBUG_PRINTLN("Device not claimed. Enabling AP+STA mode for Claiming Portal.");
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP(apName.c_str()); // Restart AP with same name
    }
}


// --- START CLAIMING PORTAL ---------------------------------------------------

void NetworkManager::startClaimingPortal(const char* claimCode) {
    if (_isPortalActive) return;

    globalClaimCode = String(claimCode);
    
    // 1. Start DNS Server to hijack all requests
    _dnsServer = new DNSServer();
    _dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
    _dnsServer->start(53, "*", WiFi.softAPIP()); // Redirect all to AP IP

    // 2. Start Web Server
    _server = new WebServer(80);
    _server->on("/", [this]() {
        this->handleRoot();
    });
    // Catch-all handler for captive portal detection (Android/iOS checks)
    _server->onNotFound([this]() {
        this->handleRoot();
    });
    _server->begin();
    
    _isPortalActive = true;
    DEBUG_PRINTLN("Claiming Portal Started (DNS + HTTP)");
    DEBUG_PRINTLN("Access at: http://", WiFi.softAPIP());
}


// --- HANDLE CLAIMING PORTAL --------------------------------------------------

void NetworkManager::handleClaimingPortal() {
    if (_isPortalActive) {
        if (_dnsServer) _dnsServer->processNextRequest(); // Handle DNS
        if (_server) _server->handleClient();             // Handle HTTP
    }
}


// --- STOP CLAIMING PORTAL ----------------------------------------------------

void NetworkManager::stopClaimingPortal() {
    if (_isPortalActive) {
        if (_server) {
            _server->stop();
            delete _server;
            _server = nullptr;
        }
        
        if (_dnsServer) {
            _dnsServer->stop();
            delete _dnsServer;
            _dnsServer = nullptr;
        }

        _isPortalActive = false;
        
        DEBUG_PRINTLN("Claiming Portal Stopped");
        
        // Turn off AP to save power, go to STA only
        WiFi.mode(WIFI_STA);
        DEBUG_PRINTLN("Switched to STA mode only.");
    }
}


// --- HANDLE ROOT (HTML PAGE) -------------------------------------------------

void NetworkManager::handleRoot() {
    String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>body{font-family:sans-serif;text-align:center;padding:20px;}h1{color:#007bff;}code{font-size:2em;background:#eee;padding:10px;display:block;margin:20px 0;}</style>";
    html += "</head><body>";
    html += "<h1>OASIS Climate</h1>";
    html += "<p>Device Connected to WiFi!</p>";
    html += "<p>Please go to your Dashboard and enter this code:</p>";
    html += "<code>" + globalClaimCode + "</code>";
    html += "<p>Waiting for claim...</p>";
    html += "</body></html>";
    
    _server->send(200, "text/html", html);
}