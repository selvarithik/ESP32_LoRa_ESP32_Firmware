#pragma once
#include <Arduino.h>
#include "../config/config_schema.h"
#include "../protocol/protocol.h"

class LoRaTransport {
public:
    bool begin(const DeviceConfig& config);
    bool send(const Message& message);
    bool available();
    bool receive(Message& message);
    void loop();

    bool auxReady() const;
    bool isInitialized() const;

private:
    HardwareSerial _serial{2};
    int _rxPin = 16;
    int _txPin = 17;
    int _auxPin = 27;
    int _m0Pin = 25;
    int _m1Pin = 26;
    bool _initialized = false;

    String _lineBuffer;
    bool _discardRx = false;
    bool waitAux(uint32_t timeoutMs = 1000) const;
};

extern LoRaTransport loraTransport;

