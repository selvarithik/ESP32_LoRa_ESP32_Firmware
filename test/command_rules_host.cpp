#include "../src/command/command_rules.h"
#include <cassert>
#include <iostream>
#include <string>
const char* check(const char* name,const char* args){static JsonDocument d;d.clear();d["command_id"]="CMD-TEST";d["command"]=name;JsonDocument a;assert(!deserializeJson(a,args));d["arguments"]=a;return CommandRules::admission(d.as<JsonObjectConst>());}
int main(){for(auto name:CommandCatalog::Names)std::cout<<name<<"\n";for(auto name:OperationRules::Names)std::cout<<name<<"\n";
assert(!strlen(check("PING","{}")));assert(!strlen(check("GET_CAPABILITIES","{}")));
assert(!strlen(check("SET_CONFIGURATION","{\"device_name\":\"Bench\",\"expected_revision\":1}")));
assert(strlen(check("SET_DEVICE_NAME","{\"device_name\":\"x\",\"wifi_password\":\"blocked\"}")));
assert(strlen(check("SET_TELEMETRY_INTERVAL","{\"seconds\":true}")));
assert(strlen(check("SET_TELEMETRY_INTERVAL","{\"seconds\":1}")));
assert(!strlen(check("SET_TELEMETRY_INTERVAL","{\"seconds\":2}")));
assert(strlen(check("PING","{\"echo\":\"wrong field\"}")));
assert(strlen(check("CONFIG_UPDATE","{\"expected_revision\":1}")));
assert(strlen(check("CONFIG_UPDATE","{\"role\":\"NODE\"}")));
assert(!strlen(check("CONFIG_UPDATE","{\"role\":\"NODE\",\"expected_revision\":4}")));
assert(!strlen(check("CONFIG_UPDATE","{\"wifi_ssid\":\"Bench\",\"expected_revision\":4}")));
assert(strlen(check("CONFIG_UPDATE","{\"wifi_password\":\"blocked\",\"expected_revision\":4}")));
assert(strlen(check("CONFIG_UPDATE","{\"local_ip_enabled\":true,\"routing_policy\":\"DIRECT_ONLY\",\"expected_revision\":4}")));
assert(!strlen(check("SET_SLEEP_SCHEDULE","{\"sleep_mode\":\"LIGHT\",\"lora_sleep_mode\":\"DEEP\"}")));
assert(strlen(check("SET_SLEEP_SCHEDULE","{\"sleep_mode\":\"INVALID\"}")));
assert(!strlen(check("CONFIG_UPDATE","{\"transport_action\":\"prepare\",\"transaction_id\":\"TX-1\",\"local_ip_enabled\":true,\"timeout_seconds\":90,\"expected_revision\":4}")));
assert(strlen(check("CONFIG_UPDATE","{\"transport_action\":\"apply\",\"transaction_id\":\"TX-1\",\"admin_password\":\"blocked\",\"expected_revision\":4}")));
assert(!strlen(check("SYNC_TIME","{\"unix_time\":1793250000}")));assert(strlen(check("SYNC_TIME","{\"unix_time\":true}")));
assert(!strlen(check("MQTT_DISCONNECT","{\"restore_after_seconds\":30}")));assert(strlen(check("MQTT_DISCONNECT","{}")));
assert(!strlen(check("SLEEP_NOW","{\"duration_seconds\":60,\"mode\":\"LIGHT\"}")));assert(strlen(check("SLEEP_NOW","{\"duration_seconds\":1,\"mode\":\"LIGHT\"}")));
assert(!strlen(check("CONFIG_UPDATE","{\"lora_channel\":4,\"expected_revision\":1}")));assert(strlen(check("CONFIG_UPDATE","{\"lora_enabled\":true,\"lora_channel\":4,\"expected_revision\":1}")));
assert(strlen(check("BOGUS","{}")));assert(strlen(check("REBOOT","{\"delay\":1}")));
assert(!strlen(check("REBOOT","{}")));std::cerr<<"PASS production command admission rules\n";}
