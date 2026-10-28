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
 bool idle() const;
 String sender() const;
 String statusJson() const;
 bool requestProbe(bool readOnly=true);
 bool delivery(String& messageId,bool& success);
};
extern LoRaTransport loraTransport;
