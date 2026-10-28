#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

enum class PacketType : uint8_t {
    UNKNOWN = 0,
    HELLO,
    HEARTBEAT,
    DISCOVERY,
    DISCOVERY_RESPONSE,
    COMMAND,
    ACK,
    RESPONSE,
    TELEMETRY,
    CONFIG_REQUEST,
    CONFIG_RESPONSE,
    DIAGNOSTIC_REQUEST,
    DIAGNOSTIC_RESPONSE,
    ROUTE_REQUEST,
    ROUTE_RESPONSE,
    OTA_REQUEST,
    OTA_RESPONSE
    , STATUS, CONFIG_GET, CONFIG_UPDATE, EVENT, ALARM, DIAGNOSTIC
};

struct Message {
    uint8_t protocolVersion = 1;
    String messageId;
    String source;
    String destination;
    String nextHop;
    PacketType type = PacketType::UNKNOWN;
    uint8_t ttl = 5;
    uint32_t timestamp = 0;
    String payload;
    String origin;
    String correlationId;
    uint32_t sequence = 0;
    uint8_t radioHopCount = 0, radioMaxHops = 0; // RV1 metadata; legacy JSON schema stays unchanged.

    String typeString() const;
    String toJson() const;
    static bool fromJson(const String& input, Message& output);
};

String generateMessageId();
String packetTypeToString(PacketType type);
PacketType packetTypeFromString(const String& type);
constexpr size_t MAX_FRAME_BYTES = 4096;
constexpr size_t MAX_PAYLOAD_BYTES = 2048;
