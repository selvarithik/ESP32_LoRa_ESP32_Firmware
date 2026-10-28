#pragma once
#include "command_catalog.h"
#include <ArduinoJson.h>
#include <stdint.h>
#include <string.h>
namespace CommandRules {
inline bool integer(JsonVariantConst v,uint32_t lo,uint32_t hi){return !v.is<bool>()&&v.is<uint32_t>()&&v.as<uint32_t>()>=lo&&v.as<uint32_t>()<=hi;}
inline const char* admission(JsonObjectConst body){
 if(body.size()!=3||!body["command"].is<const char*>()||!body["arguments"].is<JsonObjectConst>())return "INVALID_COMMAND";
 const char* name=body["command"];if(!CommandCatalog::supported(name))return "UNSUPPORTED_COMMAND";
 const char* type=CommandCatalog::canonical(name);auto args=body["arguments"].as<JsonObjectConst>();
 if(!strcmp(type,"PING")){if(args.size()>1||(args.size()&&(!args["message"].is<const char*>()||strlen(args["message"].as<const char*>())>128)))return "INVALID_PING_ARGUMENTS";return "";}
 if(!strcmp(type,"SET_TELEMETRY_INTERVAL")||!strcmp(type,"SET_HEARTBEAT_INTERVAL")){if(args.size()!=1||!integer(args["seconds"],!strcmp(type,"SET_TELEMETRY_INTERVAL")?2:5,3600))return "INVALID_INTERVAL";return "";}
 bool nameOnly=!strcmp(type,"SET_DEVICE_NAME"),schedule=!strcmp(type,"SET_SLEEP_SCHEDULE"),config=!strcmp(type,"CONFIG_UPDATE");
 if(nameOnly||schedule||config){
  if(args.size()==0)return "EMPTY_UPDATE";unsigned changed=0;
  for(JsonPairConst pair:args){const char* key=pair.key().c_str();auto v=pair.value();
   if(!strcmp(key,"expected_revision")){if(!integer(v,0,0xfffffffe))return "INVALID_REVISION";continue;}
   ++changed;
   if(!strcmp(key,"device_name")&&!schedule){if(!v.is<const char*>()||!strlen(v.as<const char*>())||strlen(v.as<const char*>())>64)return "INVALID_NAME";for(const char* p=v.as<const char*>();*p;++p)if((unsigned char)*p<32)return "INVALID_NAME";continue;}
   if(nameOnly)return "NAME_FIELDS_ONLY";
   if(!strcmp(key,"sleep_enabled")||(!schedule&&(!strcmp(key,"enabled")||!strcmp(key,"wifi_dhcp")))){if(!v.is<bool>())return "BOOLEAN_REQUIRED";continue;}
   if(!strcmp(key,"sleep_start_minute")){if(!integer(v,0,1439))return "RANGE_ERROR";continue;}
   if(!strcmp(key,"sleep_duration_seconds")){if(!integer(v,60,86400))return "RANGE_ERROR";continue;}
   if(!strcmp(key,"sleep_days_mask")){if(!integer(v,1,127))return "RANGE_ERROR";continue;}
   if(!strcmp(key,"utc_offset_minutes")){if(v.is<bool>()||!v.is<int>()||v.as<int>()<-720||v.as<int>()>840)return "RANGE_ERROR";continue;}
   if(schedule)return "SCHEDULE_FIELDS_ONLY";
   if(!strcmp(key,"telemetry_seconds")||!strcmp(key,"heartbeat_seconds")){if(!integer(v,!strcmp(key,"telemetry_seconds")?2:5,3600))return "INVALID_INTERVAL";continue;}
   if(!strcmp(key,"wifi_ip")||!strcmp(key,"wifi_subnet")||!strcmp(key,"wifi_gateway")||!strcmp(key,"wifi_dns")||!strcmp(key,"wifi_dns2")){if(!v.is<const char*>()||strlen(v.as<const char*>())>15)return "INVALID_IP_ADDRESS";continue;}
   if(!strcmp(key,"role")){if(!v.is<const char*>()||(strcmp(v.as<const char*>(),"MASTER")&&strcmp(v.as<const char*>(),"NODE")))return "INVALID_ROLE";continue;}
   if(!strcmp(key,"master_id")){if(!v.is<const char*>()||strlen(v.as<const char*>())>48)return "INVALID_MASTER_ID";for(const char* p=v.as<const char*>();*p;++p)if(!((*p>='A'&&*p<='Z')||(*p>='a'&&*p<='z')||(*p>='0'&&*p<='9')||strchr("_.:-",*p)))return "INVALID_MASTER_ID";continue;}
   if(!strcmp(key,"wifi_ssid")){if(!v.is<const char*>()||!strlen(v.as<const char*>())||strlen(v.as<const char*>())>32)return "INVALID_SSID";for(const char* p=v.as<const char*>();*p;++p)if((unsigned char)*p<32)return "INVALID_SSID";continue;}
   if(!strcmp(key,"local_ip_enabled")){if(!v.is<bool>())return "BOOLEAN_REQUIRED";continue;}
   if(!strcmp(key,"routing_policy")){if(!v.is<const char*>()||(strcmp(v.as<const char*>(),"MASTER_ONLY")&&strcmp(v.as<const char*>(),"DIRECT_ONLY")&&strcmp(v.as<const char*>(),"DIRECT_PREFERRED")))return "INVALID_ROUTING_POLICY";continue;}
   return "LOCAL_CONFIGURATION_REQUIRED";
  }
  if((!args["role"].isUnbound()||!args["master_id"].isUnbound()||!args["wifi_ssid"].isUnbound()||!args["local_ip_enabled"].isUnbound()||!args["routing_policy"].isUnbound())&&args["expected_revision"].isUnbound())return "REVISION_REQUIRED";
  return changed?"":"EMPTY_UPDATE";
 }
 return args.size()?"UNEXPECTED_ARGUMENTS":"";
}
}
