#pragma once
#include <stdint.h>
#include <ctime>
#include <string>
class String {
 std::string v;
public:
 String()=default;String(const char* s):v(s?s:""){}String(const std::string& s):v(s){}
 const char* c_str()const{return v.c_str();}unsigned length()const{return v.size();}bool isEmpty()const{return v.empty();}
 bool concat(const char* s,size_t n){v.append(s,n);return true;}bool concat(const char* s){v.append(s);return true;}bool concat(char c){v+=c;return true;}
 bool operator==(const String& s)const{return v==s.v;}bool operator!=(const String& s)const{return v!=s.v;}
 friend String operator+(const char* a,const String& b){return std::string(a)+b.v;}
};
#define ARDUINOJSON_ENABLE_ARDUINO_STRING 1
#include <ArduinoJson.h>
extern uint32_t hostMillis;extern time_t hostUtc;extern String hostJournal;extern bool hostStorage;
inline uint32_t millis(){return hostMillis;}
inline time_t hostTime(time_t*){return hostUtc;}
#define time hostTime
enum class DeviceRole {MASTER,NODE};
struct DeviceConfig {DeviceRole role=DeviceRole::MASTER;bool local_ip_enabled=false;uint32_t configuration_revision=7;};
struct HostConfig {DeviceConfig config;DeviceConfig& get(){return config;}bool validate(const DeviceConfig&,String&)const{return true;}bool update(const String& raw,String&){JsonDocument d;if(deserializeJson(d,raw)||!hostStorage)return false;config.local_ip_enabled=d["local_ip_enabled"];++config.configuration_revision;return true;}};
struct HostTransport {bool local=false;unsigned receipts=0;void requestLocalMode(bool m){local=m;}bool localMode()const{return local;}unsigned receiptCounter()const{return receipts;}};
struct HostMqtt {String active()const{return "{}";}String status(bool)const{return "{\"transaction\":{\"state\":\"IDLE\"}}";}};
extern HostConfig configManager;extern HostTransport transportManager;extern HostMqtt mqttStaging;
class Preferences {public:bool begin(const char*,bool){return hostStorage;}bool isKey(const char*){return !hostJournal.isEmpty();}String getString(const char*){return hostJournal;}size_t putString(const char*,const String& s){if(!hostStorage)return 0;hostJournal=s;return s.length();}void end(){}};
namespace TransportSwitch {void begin();void loop();bool busy();bool ready();bool action(JsonObjectConst,bool,String&);void ping(const String&,const String&,JsonDocument&);String status();}
