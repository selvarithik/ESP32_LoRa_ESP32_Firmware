#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include "session_core.h"

class LocalWebServer {
public:
    void begin();
    void loop();

private:
    WebServer _server{80};

    void registerRoutes();

    void handleRoot();
    void handleStatus();
    void handleConfigGet();
    void handleConfigPut();
    void handleWifiScan();
    void handleWifiConnect();
    void handleCommand();
    void handleDiagnostics();
    void handleMesh();
    void handleRestart();
    void handleFactoryReset();
    bool authorize(bool mutation = false);
    bool body(size_t maximum);
    void handlePacket();
    void handleLocalChallenge();
    void handleLocalExchange();
    void readLocalBody();
    String localBody;
    BrowserSessions sessions;
    BrowserSessions::Entry* browserSession(bool create=false);
    void handleSession();
    void handleLogin();
    void handleAccount();
    void handleLogout();
    void sendAccessPage(bool account);
    uint32_t loginWindow=0;
    unsigned loginFailures=0;
    uint32_t restartAt = 0;

    void sendJson(int code, const String& json);
};

extern LocalWebServer localWebServer;
