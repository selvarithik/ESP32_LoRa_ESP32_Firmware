#pragma once
#include <string.h>
#include <stddef.h>
namespace NetworkCommissionCore {
inline bool allowed(const char* key){
 if(!key)return false;
 const char* fields[]={"wifi_ssid","wifi_password","wifi_dhcp","master_id","master_host","mesh_key","local_ip_enabled","routing_policy","server_host","server_port","server_https","local_ip_token","allowed_peers_csv"};
 for(const char* field:fields)if(strcmp(key,field)==0)return true;
 return false;
}
inline bool count(size_t size){return size>0&&size<=13;}
}
