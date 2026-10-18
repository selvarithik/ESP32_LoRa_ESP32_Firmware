#include "ack.h"
#include "packet.h"
#include <ArduinoJson.h>

namespace AckProtocol {
Message buildAck(const String& source,
                 const String& destination,
                 const String& nextHop,
                 const String& commandId,
                 const String& status,
                 const String& message,
                 uint8_t ttl) {
    JsonDocument doc;
    doc["command_id"] = commandId;
    doc["status"] = status;
    doc["message"] = message;

    String body;
    serializeJson(doc, body);

    return Packet::make(PacketType::ACK, source, destination, nextHop, body, ttl);
}
}
