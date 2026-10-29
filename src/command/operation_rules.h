#pragma once
#include <ArduinoJson.h>
#include <string.h>
#include <stdint.h>
namespace OperationRules {
constexpr const char* Names[]={"SYNC_TIME","MQTT_CONNECT","MQTT_RECONNECT","MQTT_DISCONNECT","MQTT_TEST","RADIO_INSPECT","DISCOVER_NODES","I2C_SCAN","GET_COMMAND_RESULT","RESET_DIAGNOSTIC_COUNTERS","SLEEP_NOW","CANCEL_PENDING_SLEEP"};
inline bool supported(const char* n){for(auto v:Names)if(!strcmp(n,v))return true;return false;}
inline bool integer(JsonVariantConst v,uint32_t lo,uint32_t hi){return !v.is<bool>()&&v.is<uint32_t>()&&v.as<uint32_t>()>=lo&&v.as<uint32_t>()<=hi;}
inline bool identifier(const char* s){if(!s||!*s||strlen(s)>48)return false;for(;*s;++s)if(!((*s>='A'&&*s<='Z')||(*s>='a'&&*s<='z')||(*s>='0'&&*s<='9')||strchr("_.:-",*s)))return false;return true;}
inline const char* validate(const char* n,JsonObjectConst a){
 if(!strcmp(n,"SYNC_TIME"))return a.size()==1&&integer(a["unix_time"],1700000000,2147483647)?"":"INVALID_TIME";
 if(!strcmp(n,"GET_COMMAND_RESULT"))return a.size()==1&&a["command_id"].is<const char*>()&&identifier(a["command_id"])?"":"INVALID_COMMAND_ID";
 if(!strcmp(n,"RESET_DIAGNOSTIC_COUNTERS"))return a.size()==2&&a["scope"]=="NETWORK"&&a["confirm"].is<bool>()&&a["confirm"].as<bool>()?"":"COUNTER_CONFIRMATION_REQUIRED";
 if(!strcmp(n,"SLEEP_NOW"))return a.size()==2&&integer(a["duration_seconds"],60,86400)&&a["mode"].is<const char*>()&&(!strcmp(a["mode"],"DEEP")||!strcmp(a["mode"],"LIGHT"))?"":"INVALID_SLEEP_REQUEST";
 if(!strcmp(n,"MQTT_DISCONNECT"))return a.size()==1&&integer(a["restore_after_seconds"],10,120)?"":"RECOVERY_DEADLINE_REQUIRED";
 if(!strcmp(n,"DISCOVER_NODES"))return a.size()==1&&integer(a["timeout_seconds"],5,30)?"":"INVALID_DISCOVERY_TIMEOUT";
 if(!strcmp(n,"I2C_SCAN"))return a.size()==1&&a["confirm"].is<bool>()&&a["confirm"].as<bool>()?"":"SCAN_CONFIRMATION_REQUIRED";
 return a.size()?"UNEXPECTED_ARGUMENTS":"";
}
inline bool radioField(const char* k){const char* fields[]={"lora_address_high","lora_address_low","lora_baud","lora_channel","lora_air_rate","lora_tx_power","lora_packet_bytes","lora_wor_cycle_ms","lora_work_mode","lora_fixed_mode","lora_rssi","lora_lbt"};for(auto field:fields)if(!strcmp(k,field))return true;return false;}
inline bool radioValue(const char* k,JsonVariantConst v){
 if(!strcmp(k,"lora_work_mode"))return v.is<const char*>()&&(!strcmp(v,"NORMAL")||!strcmp(v,"WOR_TX")||!strcmp(v,"WOR_RX"));
 if(!strcmp(k,"lora_fixed_mode")||!strcmp(k,"lora_rssi")||!strcmp(k,"lora_lbt"))return v.is<bool>();
 if(!strcmp(k,"lora_address_high")||!strcmp(k,"lora_address_low"))return integer(v,0,255);
 if(!strcmp(k,"lora_channel"))return integer(v,0,80);if(!strcmp(k,"lora_air_rate"))return integer(v,0,7);
 if(!integer(v,0,115200))return false;unsigned n=v.as<unsigned>();
 if(!strcmp(k,"lora_baud"))return n==1200||n==2400||n==4800||n==9600||n==19200||n==38400||n==57600||n==115200;
 if(!strcmp(k,"lora_tx_power"))return n==10||n==13||n==17||n==22;
 if(!strcmp(k,"lora_packet_bytes"))return n==64||n==128||n==200;
 if(!strcmp(k,"lora_wor_cycle_ms"))return n>=500&&n<=4000&&n%500==0;return false;
}
// Shared wrap-safe deadline primitive; all operational waits are <=120 seconds.
inline bool expired(uint32_t now,uint32_t since,uint32_t timeout){return uint32_t(now-since)>=timeout;}
}
