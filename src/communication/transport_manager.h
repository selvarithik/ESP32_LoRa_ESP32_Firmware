#pragma once
#include "../protocol/protocol.h"

class TransportManager {
public:
    void begin();
    void loop();
    bool sendLoRa(const Message& message);
    bool send(const Message& message, const String& host = "", bool gateway = false);
    bool receive(Message& message, String& remote, bool& gateway);
    bool enqueue(const String& raw, const String& remote, bool gateway = false);
    bool delivery(String& id, bool& success, bool& gateway);
    uint32_t minimumStackFreeBytes() const;
    bool gatewayConnected() const;
    bool mqttConnected() const;
    uint32_t lastSuccess() const;
    uint32_t lastGatewayReceipt() const;
    uint32_t dropped() const;
    void gatewayReceipt();
    bool idle() const;
    String masterAddress() const;
    int lastError() const;
    String internetStatus() const;
    String localStatus() const;
};

extern TransportManager transportManager;
