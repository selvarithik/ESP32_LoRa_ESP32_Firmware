#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "../config/config_schema.h"

class WiFiManagerESP32 {
public:
    bool begin(const DeviceConfig& config);
    void loop();

    bool connect(const String& ssid, const String& password, uint32_t timeoutMs = 15000);
    bool disconnect();
    void suspend(bool value);
private: bool suspended=false;
public:

    String scanJson();
    String statusJson() const;

    bool isConnected() const;
    int rssi() const;
    IPAddress ip() const;

private:
    String _ssid;
    String _password;
    bool _enabled = true;
    bool _apStarted = false;
    String _apSsid;
};

extern WiFiManagerESP32 wifiManager;
