#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "../config/config_schema.h"
#include "../protocol/protocol.h"
class LoRaTransport {
public:
 bool begin(const DeviceConfig& config);
 bool send(const Message& message);
 bool canReply() const;
 bool canPoll() const;
 bool lastDeliveryCompact(const String& id) const;
 bool supportsCompact(const String& peer) const;
 uint32_t airtimeRefusals() const;
 bool available();
 bool receive(Message& message);
 void loop();
 bool auxReady() const;
 bool isInitialized() const;
 bool hasFreshReception() const;
 bool idle() const;
 String sender() const;
 void appendSignalStatus(JsonObject status) const;
 void appendRadioConfig(JsonObject status) const;
 void appendRadioMetrics(JsonObject status) const;
 String radioConfigFingerprint() const;
 String statusJson() const;
 bool requestProbe(bool readOnly=true,uint8_t diagnosticPort=2);
#ifdef CONDUCTED_BENCH
 bool requestBenchChannel(uint8_t channel);
#endif
 String isolatedUartLoopback();
 bool powerSleep(bool sleep);
 bool delivery(String& messageId,bool& success);
};
extern LoRaTransport loraTransport;
