#pragma once
#include <string.h>
#include "operation_rules.h"
namespace CommandCatalog {
inline const char* canonical(const char* name){
 if(!strcmp(name,"GET_CONFIGURATION"))return "GET_CONFIG";
 if(!strcmp(name,"SET_CONFIGURATION"))return "CONFIG_UPDATE";
 if(!strcmp(name,"REBOOT"))return "RESTART";
 return name;
}
constexpr const char* Names[]={"PING","GET_STATUS","STATUS","DIAGNOSTICS","GET_TELEMETRY","GET_CONFIG","GET_CONFIGURATION","GET_CAPABILITIES","GET_SENSOR_STATUS","GET_MESH_STATUS","GET_NODE_STATUS","GET_ROUTE_INFO","SET_CONFIGURATION","SET_DEVICE_NAME","CONFIG_UPDATE","SET_TELEMETRY_INTERVAL","SET_HEARTBEAT_INTERVAL","SET_SLEEP_SCHEDULE","GET_NETWORK","WIFI_SCAN","WIFI_RECONNECT","GET_RADIO_STATUS","RADIO_TEST","GET_POWER_STATUS","RESTART","REBOOT"};
constexpr const char* Primary[]={"PING","GET_STATUS","GET_CAPABILITIES","GET_CONFIG","GET_NETWORK","GET_TELEMETRY","GET_SENSOR_STATUS","GET_POWER_STATUS","GET_RADIO_STATUS","GET_MESH_STATUS","GET_NODE_STATUS","GET_ROUTE_INFO","DIAGNOSTICS","SET_DEVICE_NAME","SET_TELEMETRY_INTERVAL","SET_HEARTBEAT_INTERVAL","SET_SLEEP_SCHEDULE","CONFIG_UPDATE","WIFI_SCAN","WIFI_RECONNECT","RESTART"};
inline bool supported(const char* name){for(auto item:Names)if(!strcmp(item,name))return true;return OperationRules::supported(name);}
}
