#include "transport_trial_host_port.h"
#include <cassert>
#include <iostream>
uint32_t hostMillis=100;time_t hostUtc=1800000000;String hostJournal;bool hostStorage=true;
HostConfig configManager;HostTransport transportManager;HostMqtt mqttStaging;
bool action(const char* op,const char* id,unsigned revision=7,bool gateway=true){JsonDocument d;d["transport_action"]=op;d["transaction_id"]=id;d["expected_revision"]=revision;if(!strcmp(op,"prepare")){d["local_ip_enabled"]=true;d["timeout_seconds"]=90;}String result;return TransportSwitch::action(d.as<JsonObjectConst>(),gateway,result);}
String phase(){JsonDocument d;assert(!deserializeJson(d,TransportSwitch::status()));return d["state"].as<String>();}
void reset(){hostJournal="";hostStorage=true;configManager.config=DeviceConfig{};transportManager=HostTransport{};hostMillis=100;hostUtc=1800000000;TransportSwitch::begin();}
void trial(const char* id){assert(action("prepare",id));assert(action("apply",id));hostMillis+=3001;TransportSwitch::loop();assert(transportManager.local);}
int main(){
 reset();assert(!action("prepare","TX-1",6));assert(!action("prepare","TX-1",7,false));assert(action("prepare","TX-1"));assert(!transportManager.local);assert(!action("prepare","TX-2"));
 TransportSwitch::begin();assert(phase()=="CANCELLED"&&!transportManager.local);
 reset();trial("TX-reboot");TransportSwitch::begin();assert(phase()=="ROLLED_BACK"&&!transportManager.local&&!configManager.config.local_ip_enabled);
 reset();trial("TX-timeout");hostMillis+=90000;TransportSwitch::loop();assert(phase()=="ROLLED_BACK"&&!transportManager.local);
 reset();trial("TX-proof");assert(!action("confirm","TX-proof"));JsonDocument r;TransportSwitch::ping("TRANSPORT:TX-proof","MQTT",r);++transportManager.receipts;assert(!action("confirm","TX-proof"));
 TransportSwitch::ping("TRANSPORT:TX-proof","IP",r);assert(action("confirm","TX-proof"));assert(configManager.config.local_ip_enabled&&phase()=="COMMITTED");TransportSwitch::begin();assert(phase()=="COMMITTED");
 reset();trial("TX-cancel");assert(action("cancel","TX-cancel"));assert(phase()=="ROLLED_BACK"&&!transportManager.local);
 reset();hostStorage=false;assert(!action("prepare","TX-storage"));assert(!TransportSwitch::ready());
 hostStorage=true;hostJournal="malformed";TransportSwitch::begin();assert(!TransportSwitch::ready());
 std::cout<<"PASS actual firmware transport transaction: revisions, authorization boundary, staging, proof, commit, cancel, timeout, restart recovery, corrupt/storage failures (mock NVS/network)\n";
}
