#include "protocol.h"
#include <esp_system.h>
#include "../config/strict_json.h"
#include "packet_validation_core.h"

namespace {
uint32_t counter=0;
uint32_t crc32(const String& text) {return PacketValidationCore::crc32(text.c_str(),text.length());}
bool id(const String& s) {return PacketValidationCore::identity(s.c_str(),s.length());}
}
String packetTypeToString(PacketType t) {
#define TYPE(n) case PacketType::n:return #n;
    switch(t) { TYPE(HELLO) TYPE(HEARTBEAT) TYPE(COMMAND) TYPE(ACK) TYPE(RESPONSE) TYPE(TELEMETRY) TYPE(STATUS) TYPE(CONFIG_GET) TYPE(CONFIG_UPDATE) TYPE(EVENT) TYPE(ALARM) TYPE(DIAGNOSTIC) default:return "UNKNOWN"; }
#undef TYPE
}
PacketType packetTypeFromString(const String& s) {
    const PacketType types[]={PacketType::HELLO,PacketType::HEARTBEAT,PacketType::COMMAND,PacketType::ACK,PacketType::RESPONSE,PacketType::TELEMETRY,PacketType::STATUS,PacketType::CONFIG_GET,PacketType::CONFIG_UPDATE,PacketType::EVENT,PacketType::ALARM,PacketType::DIAGNOSTIC};
    for(auto t:types)if(s==packetTypeToString(t))return t;return PacketType::UNKNOWN;
}
String generateMessageId() {
    // Boot entropy plus a monotonic counter; retransmissions retain the original ID.
    static uint32_t boot1=esp_random(),boot2=esp_random();
    char b[48];snprintf(b,sizeof(b),"MSG-%08lX%08lX-%08lX",(unsigned long)boot1,(unsigned long)boot2,(unsigned long)++counter);return b;
}
String Message::typeString() const { return packetTypeToString(type); }
String Message::toJson() const {
    JsonDocument d;
    d["protocol_version"]=protocolVersion;d["message_id"]=messageId;d["source"]=source;d["destination"]=destination;
    d["origin_id"]=origin.length()?origin:source;d["final_destination_id"]=destination;d["next_hop"]=nextHop;
    d["type"]=typeString();d["ttl"]=ttl;d["timestamp"]=timestamp;d["sequence_number"]=sequence;
    d["correlation_id"]=correlationId;d["payload"]=payload;d["payload_length"]=payload.length();d["payload_crc32"]=crc32(payload);
    String s;serializeJson(d,s);return s;
}
bool Message::fromJson(const String& input, Message& out) {
    if(!StrictJson::valid(input,MAX_FRAME_BYTES))return false;
    JsonDocument d;if(deserializeJson(d,input,DeserializationOption::NestingLimit(8))||!d.is<JsonObject>())return false;
    if(d.size()!=15)return false;
    const char* strings[]={"message_id","source","destination","origin_id","final_destination_id","type","payload","correlation_id"};
    for(auto k:strings)if(!d[k].is<const char*>())return false;
    const char* numbers[]={"protocol_version","ttl","timestamp","sequence_number","payload_length","payload_crc32"};
    for(auto k:numbers)if(!d[k].is<uint32_t>()||d[k].is<bool>())return false;
    if(d["protocol_version"].as<int>()!=1||d["ttl"].as<int>()<1||d["ttl"].as<int>()>15)return false;
    Message m;m.protocolVersion=1;m.messageId=d["message_id"].as<String>();m.source=d["source"].as<String>();m.destination=d["destination"].as<String>();
    m.origin=d["origin_id"].as<String>();m.correlationId=d["correlation_id"].as<String>();m.nextHop=d["next_hop"]|"";
    m.type=packetTypeFromString(d["type"].as<String>());m.ttl=d["ttl"];m.timestamp=d["timestamp"];m.sequence=d["sequence_number"];m.payload=d["payload"].as<String>();
    if(!id(m.messageId)||!id(m.source)||!id(m.destination)||!id(m.origin)||(!m.correlationId.isEmpty()&&!id(m.correlationId)))return false;
    if(m.origin!=m.source||!id(m.nextHop)||d["final_destination_id"].as<String>()!=m.destination||m.type==PacketType::UNKNOWN||m.payload.length()>MAX_PAYLOAD_BYTES)return false;
    if(d["payload_length"].as<size_t>()!=m.payload.length()||d["payload_crc32"].as<uint32_t>()!=crc32(m.payload))return false;
    JsonDocument p;if(!StrictJson::valid(m.payload,MAX_PAYLOAD_BYTES)||deserializeJson(p,m.payload,DeserializationOption::NestingLimit(6))||!p.is<JsonObject>())return false;
    out=m;return true;
}
