#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// Contract 1.0.1. Metadata lives outside the canonical profile object.
namespace MqttContract {
constexpr uint32_t schemaVersion = 1;
constexpr size_t maxRequestBytes = 8192;
const char* const fields[] = {"profile_name","provider","host","port","mqtt_version","tls",
    "ca_cert","username","password","client_id","keepalive_s","timeout_s","qos_default","topic_prefix","gateway_id"};
const char* const messageTypes[] = {"telemetry","status","command","command_ack","config","events",
    "discovery","chat/message","chat/ack","chat/status","chat/command"};
bool validate(JsonObjectConst profile, String& field, bool connectionReady = false);
bool topic(JsonObjectConst profile, const String& messageType, String& result);
void defaults(JsonObject profile);
bool isPreservedSecret(JsonVariantConst value);
}
