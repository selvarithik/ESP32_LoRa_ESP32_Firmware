#include "response.h"
#include "packet.h"
#include <ArduinoJson.h>

namespace ResponseProtocol {
Message buildResponse(const String& source,
                      const String& destination,
                      const String& nextHop,
                      const String& commandId,
                      const String& status,
                      const String& message,
                      const String& data,
                      uint8_t ttl) {
    JsonDocument doc;
    doc["command_id"] = commandId;
    doc["status"] = status;
    doc["message"] = message;

    JsonDocument dataDoc;
    if (data.length() > 0 && deserializeJson(dataDoc, data) == DeserializationError::Ok) {
        doc["data"] = dataDoc.as<JsonVariant>();
    } else {
        doc["data"] = data;
    }

    String body;
    serializeJson(doc, body);

    return Packet::make(PacketType::RESPONSE, source, destination, nextHop, body, ttl);
}
}
