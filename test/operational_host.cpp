#include <stdint.h>
#include <ctime>
#include <string>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <iostream>
class String {std::string v;public:String()=default;String(const char*s):v(s?s:""){}String(const std::string&s):v(s){}const char*c_str()const{return v.c_str();}unsigned length()const{return v.size();}bool isEmpty()const{return v.empty();}bool startsWith(const char*s)const{return v.rfind(s,0)==0;}bool concat(const char*s,size_t n){v.append(s,n);return true;}bool concat(const char*s){v.append(s);return true;}bool concat(char c){v+=c;return true;}bool operator==(const String&s)const{return v==s.v;}bool operator!=(const String&s)const{return v!=s.v;}friend String operator+(const char*a,const String&b){return std::string(a)+b.v;}friend String operator+(const String&a,const char*b){return a.v+b;}};
#define ARDUINOJSON_ENABLE_ARDUINO_STRING 1
#include <ArduinoJson.h>
#include "../src/command/operation_rules.h"
uint32_t hostNow=100;time_t hostUtc=1800000000;
uint32_t millis(){return hostNow;}time_t hostTime(time_t*){return hostUtc;}
#define time hostTime
int settimeofday(const timeval*t,void*){hostUtc=t->tv_sec;return 0;}
enum class DeviceRole{MASTER,NODE};struct Config{DeviceRole role=DeviceRole::MASTER;bool registration_open=true,lora_enabled=true;String server_host="gateway",wifi_ssid="fixture",wifi_password="not-exported";};struct{Config c;Config&get(){return c;}}configManager;
struct{String snapshot="{\"transaction\":{\"state\":\"IDLE\"}}";String status(bool){return snapshot;}}mqttStaging;
namespace TransportSwitch{bool locked=false;bool busy(){return locked;}}
struct{bool drain=false,manual=false;bool draining(){return drain;}bool requestSleep(uint32_t,const String&,String&e){if(drain){e="POWER_BUSY";return false;}manual=true;return true;}bool cancelPendingSleep(String&e){if(!manual){e="NO_CANCELLABLE_PENDING_SLEEP";return false;}manual=false;return true;}}powerManager;
struct{bool up=true;unsigned attempts=0;bool connect(const String&,const String&,unsigned){++attempts;return true;}bool isConnected(){return up;}}wifiManager;
struct{bool local=true,connected=true;bool localMode(){return local;}bool gatewayConnected(){return connected;}}transportManager;
struct{bool initialized=false,probe=false,allowed=true;String snapshot="{\"state\":\"INSPECTING\"}";bool isInitialized(){return initialized;}bool requestProbe(bool ro){assert(ro);probe=allowed;return allowed;}String statusJson(){return snapshot;}}loraTransport;
struct{bool ready=false;String snapshot="{\"state\":\"SCANNING\"}";bool requestI2cScan(){return ready;}String i2cScanJson(){return snapshot;}}sensorManager;
enum class PacketType{STATUS,HELLO,RESPONSE};struct Message{String messageId="CMD-1",source="GATEWAY",payload="{}",destination,correlationId;uint32_t timestamp=1800000000;String toJson()const{JsonDocument d;d["payload"]=payload;String out;serializeJson(d,out);return out;}};
constexpr unsigned MAX_PAYLOAD_BYTES=2048,MAX_FRAME_BYTES=4096;unsigned packetSeq=0;Message make(PacketType,const String&dest,const String&payload){Message m;m.messageId=std::string("MSG-")+std::to_string(++packetSeq);m.destination=dest;m.payload=payload;return m;}
Message emitted;bool trackOk=true;bool track(const Message&m,const String&,bool=false){emitted=m;return trackOk;}String bodyStatus(){return "{\"device_id\":\"MASTER\"}";}
namespace CanonicalMqtt{bool up=true,enabled=true,requested=false,reconnect=false,publishOk=true;unsigned recycles=0;String state="connected",subscriptions="READY";String status(){JsonDocument d;d["runtime_enabled"]=enabled;d["state"]=state;d["subscription_state"]=subscriptions;d["tls_client_recycles"]=recycles;String out;serializeJson(d,out);return out;}bool connected(){return up;}bool publish(const Message&){return publishOk;}void requestEnabled(bool e){enabled=e;requested=e;}void requestReconnect(){reconnect=true;}}
struct Peer{String id,host;uint32_t seen=0;};Peer peers[12];
struct Cached{String source,id,request,response,responseId;uint32_t seen=0;bool executed=false,success=false;};Cached cache[32];unsigned failed=10,duplicates=3;String latestCommand,latestState;
#include "../src/core/operational_runtime.inc"
JsonDocument args,output;String result;bool deferred=false;
bool start(const char*name,const char*json="{}",const char*channel="IP",bool gateway=true){args.clear();assert(!deserializeJson(args,json));result="";return startOperation(Message{},name,args.as<JsonObjectConst>(),result,deferred,gateway,channel);}
void reset(){operationJob=OperationJob{};mqttRestoreAt=0;mqttRestoreArmed=false;hostNow=100;hostUtc=1800000000;packetSeq=0;emitted=Message{};emitted.payload="{}";configManager.c=Config{};TransportSwitch::locked=false;powerManager.drain=false;powerManager.manual=false;loraTransport.initialized=false;loraTransport.allowed=true;loraTransport.snapshot="{\"state\":\"INSPECTING\"}";sensorManager.ready=false;CanonicalMqtt::up=true;CanonicalMqtt::enabled=true;CanonicalMqtt::state="connected";CanonicalMqtt::recycles=0;CanonicalMqtt::reconnect=false;CanonicalMqtt::publishOk=true;transportManager.local=true;transportManager.connected=true;for(auto&p:peers)p=Peer{};for(auto&c:cache)c=Cached{};}
void tick(unsigned ms=101){hostNow+=ms;pollOperation();}bool responseOk(){output.clear();assert(!deserializeJson(output,emitted.payload));return output["status"]=="SUCCESS";}
int main(){
 reset();mqttStaging.snapshot="{\"transaction\":{\"state\":\"COMMITTING\"}}";assert(!start("MQTT_CONNECT"));mqttStaging.snapshot="{\"transaction\":{\"state\":\"IDLE\"}}";assert(!start("MQTT_CONNECT","{}","IP",false));assert(!operationJob.active);
 assert(start("SYNC_TIME","{\"unix_time\":1800000010}"));assert(hostUtc==1800000010);assert(!start("SYNC_TIME","{\"unix_time\":1800001000}"));
 reset();wifiManager.attempts=0;assert(start("WIFI_RECONNECT","{}","MQTT"));tick();assert(wifiManager.attempts==0);operationJob.acceptance=true;tick();assert(wifiManager.attempts==1&&operationJob.active);++CanonicalMqtt::recycles;tick();assert(responseOk()&&!operationJob.active);
 reset();wifiManager.attempts=0;assert(start("WIFI_RECONNECT","{}","MQTT"));tick(5000);assert(!responseOk()&&wifiManager.attempts==0);
 reset();assert(start("MQTT_RECONNECT"));tick();assert(operationJob.active);assert(CanonicalMqtt::reconnect);++CanonicalMqtt::recycles;tick();assert(!operationJob.active&&responseOk());
 reset();assert(!start("MQTT_DISCONNECT","{\"restore_after_seconds\":10}","MQTT"));assert(CanonicalMqtt::enabled);
 assert(start("MQTT_DISCONNECT","{\"restore_after_seconds\":10}"));CanonicalMqtt::up=false;CanonicalMqtt::state="paused";tick();assert(responseOk());assert(!CanonicalMqtt::enabled);tick(10000);assert(CanonicalMqtt::enabled);
 reset();assert(start("MQTT_CONNECT"));CanonicalMqtt::up=false;CanonicalMqtt::state="connecting";tick(90000);assert(!operationJob.active&&!responseOk());assert(CanonicalMqtt::enabled);
 reset();assert(start("MQTT_TEST"));tick();assert(operationJob.active);operationJob.receipt=true;tick();assert(responseOk()&&output["data"]["gateway_persisted_receipt"]==true);
 reset();assert(start("MQTT_TEST"));tick(15000);assert(!responseOk());
 reset();assert(start("RADIO_INSPECT"));loraTransport.snapshot="{\"state\":\"ERROR\",\"error\":\"MODE_RESTORE_TIMEOUT\",\"public_settings_read\":false}";tick();assert(!responseOk());
 reset();assert(start("RADIO_INSPECT"));loraTransport.snapshot="{\"state\":\"ERROR\",\"error\":\"PROBE_COMPLETE_NO_RF\",\"public_settings_read\":true}";tick();assert(responseOk()&&output["data"]["rf_transmission"]==false);
 reset();assert(!start("I2C_SCAN","{\"confirm\":true}"));sensorManager.ready=true;assert(start("I2C_SCAN","{\"confirm\":true}"));sensorManager.snapshot="{\"state\":\"COMPLETED\",\"addresses\":[64]}";tick();assert(responseOk());
 reset();configManager.c.registration_open=false;assert(!start("DISCOVER_NODES","{\"timeout_seconds\":5}"));configManager.c.registration_open=true;assert(!start("DISCOVER_NODES","{\"timeout_seconds\":5}"));loraTransport.initialized=true;assert(start("DISCOVER_NODES","{\"timeout_seconds\":5}"));peers[0].id="IP-ONLY";peers[0].host="192.168.1.5";peers[0].seen=200;tick(5000);assert(!responseOk());
 reset();loraTransport.initialized=true;assert(start("DISCOVER_NODES","{\"timeout_seconds\":5}"));peers[0].id="SLAVE";peers[0].host="RADIO:SLAVE";peers[0].seen=200;tick(5000);assert(responseOk());
 reset();assert(start("GET_COMMAND_RESULT","{\"command_id\":\"MISSING\"}"));assert(!deserializeJson(output,result));assert(output["state"]=="UNKNOWN_OR_EXPIRED");cache[0].id="OLD";cache[0].source="GATEWAY";cache[0].executed=true;cache[0].success=true;assert(start("GET_COMMAND_RESULT","{\"command_id\":\"OLD\"}"));assert(!deserializeJson(output,result));assert(output["state"]=="COMPLETED"&&output["result_retained"]==false);
 reset();assert(start("RESET_DIAGNOSTIC_COUNTERS","{\"scope\":\"NETWORK\",\"confirm\":true}"));assert(!failed&&!duplicates);
 assert(start("SLEEP_NOW","{\"duration_seconds\":60,\"mode\":\"LIGHT\"}"));assert(!deserializeJson(output,result));assert(output["sleep_entry_confirmed"]==false);assert(start("CANCEL_PENDING_SLEEP"));assert(!start("CANCEL_PENDING_SLEEP"));
 reset();hostNow=uint32_t(0)-10000;assert(start("MQTT_DISCONNECT","{\"restore_after_seconds\":10}"));assert(mqttRestoreArmed&&mqttRestoreAt==0);CanonicalMqtt::up=false;CanonicalMqtt::state="paused";tick();assert(responseOk());assert(!start("MQTT_CONNECT"));tick(10000);assert(!mqttRestoreArmed&&CanonicalMqtt::enabled);
 assert(OperationRules::expired(10,0xfffffff0,20));
 std::cout<<"PASS actual operational runtime: authorization, time bounds, reconnect proof, pause recovery, real-receipt requirement, timeouts, radio/I2C failures, RF-only discovery, cached result scope, counter and sleep-request semantics (mock hardware/network).\n";
}
