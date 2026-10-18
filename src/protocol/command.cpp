#include "command.h"
#include "packet.h"
#include <ArduinoJson.h>

namespace CommandProtocol {
Message buildCommand(const String& source,
                     const String& destination,
                     const String& nextHop,
                     const String& command,
                     const String& commandId,
                     const String& payload,
                     uint8_t ttl) {
    JsonDocument doc;
    doc["command"] = command;
    doc["command_id"] = commandId;

    JsonDocument payloadDoc;
    if (payload.length() > 0 && deserializeJson(payloadDoc, payload) == DeserializationError::Ok) {
        doc["payload"] = payloadDoc.as<JsonVariant>();
    } else {
        doc["payload"] = payload;
    }

    String body;
    serializeJson(doc, body);

    return Packet::make(PacketType::COMMAND, source, destination, nextHop, body, ttl);
}
}
