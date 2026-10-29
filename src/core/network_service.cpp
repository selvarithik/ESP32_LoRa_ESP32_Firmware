#include "../config/hardware_profile.h"
#include "../config/firmware_version.h"
#include "network_service.h"
#include "../config/transport_trial.h"
#include "../config/mqtt_staging.h"
#include "config_manager.h"
#include "logger.h"
#include "system_manager.h"
#include "../communication/transport_manager.h"
#include "../communication/wifi_manager.h"
#include "../diagnostics/diagnostics_manager.h"
#include "../sensors/sensor_manager.h"
#include "../sensors/telemetry_validation.h"
#include "../protocol/packet.h"
#include <ArduinoJson.h>
#include <time.h>
#include <cmath>
#include <limits.h>
#include "../communication/local_ip_core.h"
#include "power_manager.h"
#include "../command/command_rules.h"
#include <esp_mac.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <sys/time.h>
#include "../communication/lora_transport.h"
#include "../communication/e220_core.h"
#include "../communication/canonical_mqtt.h"
#include "../communication/delivery_core.h"

NetworkService networkService;
namespace {
struct Peer {String id,host,address,telemetry;uint32_t seen=0,heartbeat=30;};
struct Pending {bool used=false,gateway=false,command=false,acknowledged=false,inFlight=false;Message packet;String host;uint8_t attempts=0;uint32_t next=0,created=0;};
struct Cached {String source,id,request,response,responseId;uint32_t seen=0;bool executed=false,success=false;};
Peer peers[12];Pending pending[12];Cached cache[32];uint8_t cacheNext=0;
E220Core::Routes radioRoutes;String discoveredMaster;uint32_t radioAnnounceAt=0,helloJitter=0;
struct Forwarded {String source,id;uint32_t at=0;};Forwarded forwarded[32];uint8_t forwardedNext=0;
uint32_t heartbeatAt=0,telemetryAt=0,helloAt=0,pollAt=0,sequence=0,failed=0,duplicates=0;
uint32_t sensorCheckedAt=0,diagnosticAt=0,recoveryHelloAt=0;
String previousSensorStatus,lastPowerIntent;
String latestCommand,latestState="IDLE",latestResult="{}";
Peer* peer(const String& id){for(auto& p:peers)if(p.id==id)return &p;return nullptr;}
bool listed(const String& id){String csv=","+configManager.get().allowed_peers_csv+",";csv.replace(" ","");return csv.indexOf(","+id+",")>=0;}
String masterId(){return configManager.get().master_id.isEmpty()?discoveredMaster:configManager.get().master_id;}
String radioHost(const String& requested){String destination=requested=="GATEWAY"&&configManager.get().role==DeviceRole::NODE?masterId():requested;auto route=radioRoutes.find(destination.c_str(),millis(),configManager.get().route_timeout_seconds*1000UL);if(route)return "RADIO:"+String(route->nextHop);if(destination==masterId()&&!masterId().isEmpty())return "RADIO:"+masterId();return "";}
bool forwardDuplicate(const Message& m){for(auto& item:forwarded)if(item.id==m.messageId&&item.source==m.source&&millis()-item.at<120000)return true;return false;}
void forwardedNow(const Message& m){auto& item=forwarded[forwardedNext++%32];item.id=m.messageId;item.source=m.source;item.at=millis();}
Message make(PacketType type,const String& destination,const String& body) {Message m=Packet::make(type,configManager.get().device_id,destination,destination,body,configManager.get().max_hops);m.origin=m.source;m.sequence=++sequence;return m;}
bool track(const Message& m,const String& host,bool gateway=false) {
    if(host.isEmpty()&&!gateway)return false;
    if(m.payload.length()>MAX_PAYLOAD_BYTES||m.toJson().length()>MAX_FRAME_BYTES){++failed;return false;}
    for(auto& p:pending)if(p.used&&p.packet.messageId==m.messageId&&p.gateway==gateway)return true;
    if(m.type==PacketType::TELEMETRY&&!host.startsWith("RADIO:")&&m.source==configManager.get().device_id){
        JsonDocument fresh;if(!deserializeJson(fresh,m.payload)){
            const char* sensor=fresh["sensor_id"]|"";
            for(auto& p:pending)if(p.used&&p.gateway==gateway&&p.host==host&&p.packet.type==PacketType::TELEMETRY&&p.packet.source==m.source){
                JsonDocument older;if(deserializeJson(older,p.packet.payload))continue;
                if(DeliveryCore::replaceTelemetry(true,true,p.inFlight,sensor,older["sensor_id"]|"")){p=Pending{};p.used=true;p.gateway=gateway;p.packet=m;p.host=host;p.created=millis();p.next=p.created;return true;}
            }
        }
    }
    for(auto& p:pending)if(!p.used){p=Pending{};p.used=true;p.packet=m;p.host=host;p.gateway=gateway;p.command=m.type==PacketType::COMMAND||m.type==PacketType::CONFIG_GET||m.type==PacketType::CONFIG_UPDATE;p.created=millis();p.next=p.created;return true;}
    if(m.type==PacketType::ACK||m.type==PacketType::RESPONSE||m.type==PacketType::COMMAND){for(auto& item:pending)if(!item.command&&(item.packet.type==PacketType::TELEMETRY||item.packet.type==PacketType::STATUS||item.packet.type==PacketType::HEARTBEAT)){item.used=false;++failed;return track(m,host,gateway);}}
    ++failed;logger.warn("DELIVERY","Pending queue full; new message rejected");return false;
}
bool upstream(const Message& input) {
 auto& c=configManager.get();Message m=input;
 if(transportManager.localMode()){m.destination="GATEWAY";m.nextHop="GATEWAY";}
 if(c.role==DeviceRole::NODE&&loraTransport.isInitialized()){String parent=masterId();if(parent.isEmpty()&&m.type!=PacketType::HELLO)return false;m.destination=parent.isEmpty()?"BROADCAST":parent;m.nextHop=m.destination;return track(m,"RADIO:"+m.destination);}
 if(c.role==DeviceRole::MASTER)return (transportManager.mqttConnected()||LocalIpCore::gatewayCredential(transportManager.localMode(),!c.gateway_token.isEmpty(),!c.local_ip_token.isEmpty()))&&track(m,c.server_host,true);
 bool direct=LocalIpCore::direct(false,transportManager.localMode(),c.routing_policy.c_str(),(!c.gateway_token.isEmpty()||!c.local_ip_token.isEmpty())&&!c.server_host.isEmpty());
 if(direct)return track(m,c.server_host,true);
 return (!transportManager.localMode()||c.routing_policy!="DIRECT_ONLY")&&!c.mesh_key.isEmpty()&&!c.master_id.isEmpty()&&track(m,c.master_host);
}
void complete(const String& id,const String& source) {for(auto& p:pending)if(p.used&&p.command&&p.packet.messageId==id&&p.packet.destination==source)p.used=false;}
String bodyStatus() {
    JsonDocument d;auto& c=configManager.get();d["device_id"]=c.device_id;d["device_name"]=c.device_name;d["role"]=configManager.roleString();d["master_id"]=c.role==DeviceRole::MASTER?c.device_id:masterId();d["node_address"]=c.node_address;
    uint8_t mac[6];esp_read_mac(mac,ESP_MAC_WIFI_STA);char macText[18];snprintf(macText,sizeof(macText),"%02X:%02X:%02X:%02X:%02X:%02X",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);d["hardware_mac"]=macText;d["firmware_version"]=FIRMWARE_VERSION;d["configuration_revision"]=c.configuration_revision;d["heartbeat_seconds"]=c.heartbeat_seconds;d["uptime_s"]=millis()/1000;d["free_heap"]=ESP.getFreeHeap();d["wifi_connected"]=wifiManager.isConnected();if(wifiManager.isConnected())d["wifi_rssi"]=wifiManager.rssi();
    d["gateway_reachable"]=transportManager.gatewayConnected();d["transport"]=transportManager.mqttConnected()?"MQTT":"HTTP";
    d["telemetry_seconds"]=c.telemetry_seconds;d["simulated"]=false;JsonDocument power;deserializeJson(power,powerManager.statusJson());d["operating_state"]=power["state"];
    JsonDocument transportState;deserializeJson(transportState,TransportSwitch::status());d["transport_switch"]=transportState.as<JsonVariant>();d["reset_reason"]=systemManager.resetReason();d["internet_status"]=transportManager.internetStatus();
    JsonDocument sensor;deserializeJson(sensor,sensorManager.telemetryJson());d["sensor_status"]=sensor["status"];JsonDocument health;deserializeJson(health,sensorManager.healthJson());d["sensors"]=health;d["last_sensor_sample_uptime_ms"]=sensor["sampled_uptime_ms"];d["last_transport_accept_uptime_ms"]=transportManager.lastSuccess();d["last_gateway_receipt_uptime_ms"]=transportManager.lastGatewayReceipt();d["sample_time_basis"]="DEVICE_UPTIME_MS";d["enabled"]=c.enabled;
    String s;serializeJson(d,s);return s;
}
#include "operational_runtime.inc"
bool execute(const Message& m,String& result,bool gateway=false,const String& channel="") {
    JsonDocument d;deserializeJson(d,m.payload);String type=d["command"]|"";
    type=CommandCatalog::canonical(type.c_str());
    if(m.type==PacketType::CONFIG_GET)type="GET_CONFIG";
    if(m.type==PacketType::CONFIG_UPDATE)type="CONFIG_UPDATE";
    auto arguments=d["arguments"].as<JsonObject>();
    bool configCommand=type=="SET_DEVICE_NAME"||type=="CONFIG_UPDATE"||type=="SET_SLEEP_SCHEDULE"||type=="SET_TELEMETRY_INTERVAL"||type=="SET_HEARTBEAT_INTERVAL";
    if((configCommand||type=="WIFI_RECONNECT"||type=="WIFI_SCAN")&&(operationJob.active||mqttRestoreArmed))return operationError(result,"OPERATION_BUSY");
    if(!configCommand&&!OperationRules::supported(type.c_str())&&type!="PING"&&arguments.size()){result="{\"error\":\"UNEXPECTED_ARGUMENTS\"}";return false;}
    if(type=="PING"&&(arguments.size()>1||(arguments.size()&&(!arguments["message"].is<const char*>()||arguments["message"].as<String>().length()>128)))){result="{\"error\":\"INVALID_PING_ARGUMENTS\"}";return false;}
    if(type=="CONFIG_UPDATE"&&arguments["transport_action"].is<const char*>())return TransportSwitch::action(arguments,gateway,result);
    if(OperationRules::supported(type.c_str())||type=="WIFI_RECONNECT"){bool deferred=false;bool ok=startOperation(m,type,arguments,result,deferred,gateway,channel);return ok;}
    if(type=="PING"){JsonDocument pong;TransportSwitch::ping(arguments["message"]|"",channel,pong);pong["ok"]=true;if(arguments["message"].is<const char*>())pong["message"]=arguments["message"];serializeJson(pong,result);}
    else if(type=="GET_STATUS"||type=="STATUS")result=bodyStatus();
    else if(type=="DIAGNOSTICS"){JsonDocument all,out;deserializeJson(all,diagnosticsManager.statusJson());out["device"]=all["device"];out["wifi"]=all["wifi"];out["power"]=all["power"];out["lora_status"]=all["lora_status"];out["ethernet_status"]=all["ethernet_status"];out["gateway_reachable"]=all["gateway_reachable"];out["internet_status"]=all["internet_status"];serializeJson(out,result);}
    else if(type=="GET_CAPABILITIES"){JsonDocument out;out["protocol_version"]=1;out["role"]=configManager.roleString();out["lora_driver"]="E220_SHARED_RV1_HARDWARE_UNVERIFIED";out["remote_cancellation"]=false;auto list=out["commands"].to<JsonArray>();for(auto name:CommandCatalog::Primary)list.add(name);for(auto name:OperationRules::Names)list.add(name);out["operational_extension_version"]=1;serializeJson(out,result);}
    else if(type=="GET_SENSOR_STATUS")result=sensorManager.healthJson();
    else if(type=="GET_MESH_STATUS"||type=="GET_NODE_STATUS"||type=="GET_ROUTE_INFO"){JsonDocument all,out;deserializeJson(all,networkService.statusJson());out["radio_state"]=loraTransport.isInitialized()?"READY_LINK_UNVERIFIED":"DISABLED_OR_ERROR";out["hardware_verified"]=false;if(type=="GET_ROUTE_INFO")out["routes"]=all["radio_routes"];else if(type=="GET_NODE_STATUS"){auto list=out["nodes"].to<JsonArray>();for(JsonObject node:all["nodes"].as<JsonArray>()){auto entry=list.add<JsonObject>();entry["device_id"]=node["device_id"];entry["status"]=node["status"];entry["age_ms"]=node["age_ms"];}}else{out["node_count"]=all["nodes"].size();out["route_count"]=all["radio_routes"].size();out["pending"]=all["pending"];out["delivery_failures"]=all["delivery_failures"];out["duplicates"]=all["duplicates"];}serializeJson(out,result);}
    else if(type=="GET_TELEMETRY")result=sensorManager.telemetryJson();
    else if(type=="GET_CONFIG"){JsonDocument all,out;deserializeJson(all,configManager.toJson());const char* fields[]={"device_id","device_name","role","master_id","configuration_version","configuration_revision","telemetry_seconds","heartbeat_seconds","enabled","sleep_enabled","sleep_mode","lora_sleep_mode","sleep_start_minute","sleep_duration_seconds","sleep_days_mask","utc_offset_minutes","local_ip_enabled","routing_policy","server_host","server_port","wifi_dhcp","wifi_ip","wifi_subnet","wifi_gateway","wifi_dns","wifi_dns2","dht_enabled","dht_pin","ina219_enabled","ina219_address","ina219_shunt_milliohms","mq135_enabled","mq135_warmup_seconds"};for(const char* key:fields)out[key]=all[key];out["config_scope"]="RUNTIME";serializeJson(out,result);}
    else if(type=="GET_NETWORK")result=wifiManager.statusJson();
    else if(type=="WIFI_SCAN") {
        // An all-channel scan can interrupt the active TLS telemetry stream.
        // Require an explicitly paused MQTT session rather than dropping it.
        if(transportManager.mqttConnected()){result="{\"error\":\"WIFI_SCAN_BUSY_MQTT_ACTIVE\"}";return false;}
        JsonDocument out,scan;deserializeJson(scan,wifiManager.scanJson());out["results"]=scan;out["state"]="SCANNING_OR_RESULTS";serializeJson(out,result);
    }
    else if(type=="GET_RADIO_STATUS")result=loraTransport.statusJson();
    else if(type=="RADIO_TEST"){result="{\"error\":\"USE_ADDRESSED_PING_DURING_APPROVED_HARDWARE_TEST\"}";return false;}
    else if(type=="GET_POWER_STATUS")result=powerManager.statusJson();
    else if(type=="SET_TELEMETRY_INTERVAL"||type=="SET_HEARTBEAT_INTERVAL"||type=="SET_SLEEP_SCHEDULE"||type=="CONFIG_UPDATE"||type=="SET_DEVICE_NAME") {
        JsonDocument update;auto args=d["arguments"].as<JsonObject>();
        bool radioUpdate=false;for(JsonPair p:args)if(OperationRules::radioField(p.key().c_str()))radioUpdate=true;
        if(radioUpdate&&(!gateway||configManager.get().lora_enabled||configManager.get().lora_apply_settings||loraTransport.isInitialized()||operationCoordinatorBusy()||powerManager.draining()))return operationError(result,"RADIO_UPDATE_REQUIRES_DISABLED_LOCAL_COMMISSIONING");

        if(!args["local_ip_enabled"].isUnbound()){result="{\"error\":\"TRANSPORT_TRANSACTION_REQUIRED\"}";return false;}
        if(type=="SET_DEVICE_NAME"||type=="CONFIG_UPDATE"||type=="SET_SLEEP_SCHEDULE") {
            if(args.isNull()||args.size()==0){result="{\"error\":\"EMPTY_UPDATE\"}";return false;}
            if(!args["expected_revision"].as<JsonVariantConst>().isUnbound()){if(!args["expected_revision"].is<uint32_t>()||args["expected_revision"].is<bool>()||args["expected_revision"].as<uint32_t>()!=configManager.get().configuration_revision){result="{\"error\":\"REVISION_CONFLICT\"}";return false;}}
            for(JsonPair p:args){String k=p.key().c_str();if(k=="expected_revision")continue;if(type=="SET_DEVICE_NAME"&&k!="device_name"){result="{\"error\":\"NAME_FIELDS_ONLY\"}";return false;}if(type=="SET_SLEEP_SCHEDULE"&&k!="sleep_enabled"&&k!="sleep_mode"&&k!="lora_sleep_mode"&&k!="sleep_start_minute"&&k!="sleep_duration_seconds"&&k!="sleep_days_mask"&&k!="utc_offset_minutes"){result="{\"error\":\"SCHEDULE_FIELDS_ONLY\"}";return false;}if(!OperationRules::radioField(k.c_str())&&k!="role"&&k!="master_id"&&k!="wifi_ssid"&&k!="local_ip_enabled"&&k!="routing_policy"&&k!="telemetry_seconds"&&k!="heartbeat_seconds"&&k!="enabled"&&k!="device_name"&&k!="wifi_dhcp"&&k!="wifi_ip"&&k!="wifi_subnet"&&k!="wifi_gateway"&&k!="wifi_dns"&&k!="wifi_dns2"&&k!="sleep_enabled"&&k!="sleep_mode"&&k!="lora_sleep_mode"&&k!="sleep_start_minute"&&k!="sleep_duration_seconds"&&k!="sleep_days_mask"&&k!="utc_offset_minutes"){result="{\"error\":\"LOCAL_CONFIGURATION_REQUIRED\"}";return false;}update[k]=p.value();}
        } else {
            if(!args["seconds"].is<uint32_t>()){result="{\"error\":\"INTEGER_SECONDS_REQUIRED\"}";return false;}
            update[type=="SET_TELEMETRY_INTERVAL"?"telemetry_seconds":"heartbeat_seconds"]=args["seconds"];
        }
        if(update["role"].is<const char*>()&&!HardwareProfile::roleMatches(update["role"].as<String>()=="MASTER")){result="{\"error\":\"MATCHING_ROLE_FIRMWARE_REQUIRED\"}";return false;}
        bool restartRequired=false;for(JsonPair item:update.as<JsonObject>()){String k=item.key().c_str();if(k.startsWith("wifi_")||k=="role"||k=="master_id"||k=="local_ip_enabled"||k=="routing_policy")restartRequired=true;}
        String raw,error;serializeJson(update,raw);
        if(!configManager.update(raw,error)){JsonDocument r;r["error"]=error;result="";serializeJson(r,result);return false;}
        JsonDocument saved;saved["saved"]=true;saved["configuration_revision"]=configManager.get().configuration_revision;saved["restart_required"]=restartRequired;if(radioUpdate){saved["radio_activation"]="PENDING_LOCAL_COMMISSIONING";saved["radio_registers_written"]=false;saved["readback_verified"]=false;}result="";serializeJson(saved,result);
    } else {result="{\"error\":\"UNSUPPORTED_COMMAND\"}";return false;}
    return true;
}
void handleCommand(const Message& m,const String& remote,bool gateway) {
    JsonDocument d;deserializeJson(d,m.payload);String cid=d["command_id"]|m.messageId;
    if(cid!=m.messageId||!d["arguments"].is<JsonObject>()||d.size()!=3||!d["command"].is<const char*>()){++failed;return;}
    time_t utc=time(nullptr);
    String operation=CommandCatalog::canonical(d["command"].as<const char*>());bool readOnly=operation=="PING"||operation.startsWith("GET_")||operation=="STATUS"||operation=="DIAGNOSTICS"||operation=="WIFI_SCAN";
    if((gateway||!readOnly)&&operation!="SYNC_TIME"&&(!m.timestamp||utc<1700000000)){++failed;return;}
    if(m.timestamp&&utc>1600000000&&(m.timestamp>utc+30||utc-m.timestamp>configManager.get().message_max_age_ms/1000)){++failed;return;}
    for(auto& c:cache)if(c.source==m.source&&c.id==cid&&c.request!=m.payload){++failed;return;}
    String rejection=CommandRules::admission(d.as<JsonObjectConst>());bool repeated=false;
    for(auto& c:cache)if(c.source==m.source&&c.id==cid)repeated=true;
    unsigned freeSlots=0;for(auto& p:pending)if(!p.used)++freeSlots;
    bool memoryReady=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)>=24000&&heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)>=8192;
    if(freeSlots<2||!memoryReady){++failed;if(freeSlots){Message busy=make(PacketType::ACK,m.source,"{\"command_id\":\""+cid+"\",\"status\":\"BUSY\"}");busy.correlationId=cid;track(busy,gateway?configManager.get().server_host:remote,gateway);}return;} // Never execute without space for both acceptance and result.
    latestCommand=cid;latestState="RECEIVED";
    JsonDocument receipt;receipt["command_id"]=cid;receipt["status"]=rejection.length()?"REJECTED":repeated?"DUPLICATE":"ACCEPTED";if(rejection.length())receipt["error_code"]=rejection;String ackBody;serializeJson(receipt,ackBody);
    Message ack=make(PacketType::ACK,m.source,ackBody);ack.correlationId=cid;
    if(gateway)track(ack,configManager.get().server_host,true);else track(ack,remote);
    if(rejection.length()){latestState="REJECTED";return;}
    for(auto& c:cache)if(c.source==m.source&&c.id==cid) {
        ++duplicates;
        if(c.request!=m.payload){++failed;return;}
        if(c.response.length()){Message r;if(Message::fromJson(c.response,r)){if(gateway)track(r,configManager.get().server_host,true);else track(r,remote);}}
        return;
    }
    while(operationJob.active&&cache[cacheNext%32].id==operationJob.request.messageId)++cacheNext;auto& entry=cache[cacheNext++%32];entry=Cached{};entry.source=m.source;entry.id=cid;entry.request=m.payload;entry.seen=millis();
    Message replay;
    if(powerManager.replayRestart(m,replay)){entry.response=replay.toJson();entry.responseId=replay.messageId;if(gateway)track(replay,configManager.get().server_host,true);else track(replay,remote);return;}
    if(String(CommandCatalog::canonical(d["command"].as<const char*>()))=="RESTART"){
        String error;if(operationJob.active||mqttRestoreArmed)error="OPERATION_BUSY";if(d["arguments"].size())error="UNEXPECTED_ARGUMENTS";if(error.isEmpty()&&powerManager.requestRestart(m,error)){latestState="ACKNOWLEDGED";return;}
        entry.response="";
        // Failed persistence must produce a real failed response, never reboot.
        JsonDocument failure;failure["command_id"]=cid;failure["status"]="FAILED";failure["data"]["error"]=error;String raw;serializeJson(failure,raw);Message response=make(PacketType::RESPONSE,m.source,raw);response.correlationId=cid;entry.response=response.toJson();entry.responseId=response.messageId;if(gateway)track(response,configManager.get().server_host,true);else track(response,remote);latestState="FAILED";return;
    }
    latestState="EXECUTING";uint32_t executionAt=millis();String data;bool ok=execute(m,data,gateway,remote);
    if(operationJob.active&&operationJob.request.messageId==cid){latestState="EXECUTING";return;}
    JsonDocument response;response["command_id"]=cid;response["status"]=ok?"SUCCESS":"FAILED";response["execution_duration_ms"]=(uint32_t)(millis()-executionAt);
    JsonDocument parsed;if(!deserializeJson(parsed,data))response["data"]=parsed.as<JsonVariant>();
    String body;serializeJson(response,body);Message r=make(PacketType::RESPONSE,m.source,body);r.correlationId=cid;
    if(r.toJson().length()>MAX_FRAME_BYTES||r.payload.length()>MAX_PAYLOAD_BYTES){r.payload="{\"command_id\":\""+cid+"\",\"status\":\"FAILED\",\"data\":{\"error\":\"RESULT_TOO_LARGE\"}}";ok=false;}
    entry.response=r.toJson();entry.responseId=r.messageId;entry.executed=true;entry.success=ok;latestResult=data;latestState=ok?"COMPLETED":"FAILED";
    if(gateway)track(r,configManager.get().server_host,true);else track(r,remote);
}
}
void NetworkService::begin(){helloAt=millis()-30000;helloJitter=esp_random()%2000;heartbeatAt=millis();telemetryAt=millis();pollAt=millis();}
bool NetworkService::accept(const Message& m,const String& remote,bool gateway) {
    auto& c=configManager.get();
    if(m.source==c.device_id)return false;
    if(!c.enabled&&m.type!=PacketType::COMMAND&&m.type!=PacketType::CONFIG_GET&&m.type!=PacketType::CONFIG_UPDATE)return false;
    if(gateway)return (c.role==DeviceRole::MASTER||(transportManager.localMode()&&c.routing_policy!="MASTER_ONLY"&&m.destination==c.device_id))&&m.source=="GATEWAY";
    if(c.mesh_key.length()<32)return false;
    if(remote.startsWith("RADIO:")){
        String hop=remote.substring(6);JsonDocument body;if(deserializeJson(body,m.payload))return false;
        if(m.type==PacketType::HELLO){
            if(body["device_id"].as<String>()!=m.source)return false;String role=body["role"]|"";
            if(c.role==DeviceRole::NODE&&role=="MASTER"&&(c.master_id.isEmpty()||m.source==c.master_id)){
                discoveredMaster=m.source;radioRoutes.observe(m.source.c_str(),hop.c_str(),m.radioHopCount,millis());
                if(time(nullptr)<1700000000&&m.timestamp>=1700000000){timeval value{(time_t)m.timestamp,0};settimeofday(&value,nullptr);}return true;
            }
            bool assigned=body["master_id"].as<String>()==(c.role==DeviceRole::MASTER?c.device_id:masterId());
            if(role!="NODE"||(!assigned&&!(c.registration_open&&body["master_id"].as<String>().isEmpty())))return false;
            return c.registration_open||listed(m.source)||peer(m.source);
        }
        if(c.role==DeviceRole::MASTER){if(m.type==PacketType::COMMAND||m.type==PacketType::CONFIG_GET||m.type==PacketType::CONFIG_UPDATE)return false;Peer* node=peer(m.source);return node&&node->host==remote;}
        auto parent=radioHost(masterId());bool fromParent=!parent.isEmpty()&&parent==remote;
        if(fromParent&&(m.source==masterId()||m.source=="GATEWAY")){
            if(time(nullptr)<1700000000&&m.timestamp>=1700000000){timeval value{(time_t)m.timestamp,0};settimeofday(&value,nullptr);}return true;
        }
        return (listed(m.source)||peer(m.source))&&m.type!=PacketType::COMMAND&&m.type!=PacketType::CONFIG_GET&&m.type!=PacketType::CONFIG_UPDATE;
    }
    if(c.role==DeviceRole::NODE)return (remote==c.master_host||remote==transportManager.masterAddress())&&(m.source==c.master_id||m.source=="GATEWAY")&&(m.destination==c.device_id);
    if(m.type==PacketType::COMMAND||m.type==PacketType::CONFIG_GET||m.type==PacketType::CONFIG_UPDATE)return false; // Child nodes cannot administer their Master.
    if(m.destination!=c.device_id&&m.destination!="GATEWAY")return false;
    if(m.type==PacketType::HELLO)return c.registration_open||listed(m.source)||peer(m.source);
    Peer* p=peer(m.source);return p&&p->host==remote;
}
void NetworkService::loop() {
    sensorManager.loop();pollOperation();auto& c=configManager.get();uint32_t now=millis();radioRoutes.expire(now,c.route_timeout_seconds*1000UL);
    // Commands older than this window are rejected before execution; retaining
    // their full result beyond it only consumes the heap needed by TLS.
    for(auto& entry:cache)if(entry.id.length()&&(!operationJob.active||entry.id!=operationJob.request.messageId)&&now-entry.seen>c.message_max_age_ms){
        // Arduino String's assignment from an empty String retains capacity.
        // Assigning a null C string actually releases the cached heap buffers.
        entry.source=(const char*)nullptr;entry.id=(const char*)nullptr;
        entry.request=(const char*)nullptr;entry.response=(const char*)nullptr;entry.responseId=(const char*)nullptr;entry.seen=0;
    }
    // Establish the drain state before queuing ordinary work; the final call below may sleep/restart.
    powerManager.loop(UINT_MAX,false);
    if(powerManager.draining()){
        JsonDocument event;deserializeJson(event,bodyStatus());JsonDocument power;deserializeJson(power,powerManager.statusJson());String phase=power["state"]|"DRAINING";
        if(lastPowerIntent!=phase){event["event"]="POWER_TRANSITION_INTENT";String raw;serializeJson(event,raw);if(upstream(make(PacketType::STATUS,c.role==DeviceRole::MASTER?"GATEWAY":c.master_id,raw)))lastPowerIntent=phase;}
    }else lastPowerIntent="";
    String delivered;bool ok,viaGateway;int code=0;
    static int lastLoggedError=0;
    while(transportManager.delivery(delivered,ok,viaGateway,code)) {
        for(auto& p:pending)if(ok&&p.used&&p.packet.messageId==delivered&&p.packet.type==PacketType::ACK&&p.packet.correlationId==operationJob.request.messageId&&operationJob.type=="WIFI_RECONNECT")operationJob.acceptance=true;
        for(auto& p:pending)if(p.used&&p.packet.messageId==delivered&&p.gateway==viaGateway){p.inFlight=false;if(!ok)p.next=DeliveryCore::retryAt(p.command,code,now,p.next);}
        if(!ok&&(code<0||code>=400)&&code!=lastLoggedError){lastLoggedError=code;logger.warn("NETWORK",code==-1001?"DNS resolution failed":code==-11?"HTTP server read timeout":"HTTP status/error="+String(code));}
        if(ok)lastLoggedError=0;
        if(DeliveryCore::registrationRetry(c.role==DeviceRole::NODE,viaGateway,code,now-recoveryHelloAt)){recoveryHelloAt=now;helloAt=now-30000;helloJitter=0;}
        if(LocalIpCore::fallback(c.role==DeviceRole::NODE,transportManager.localMode(),c.routing_policy.c_str(),!ok,viaGateway,!c.master_host.isEmpty()&&c.mesh_key.length()>=32))
            for(auto& p:pending)if(p.used&&p.packet.messageId==delivered&&p.gateway){p.gateway=false;p.host=c.master_host;p.next=now+1000;}
        for(auto& p:pending)if(p.used&&p.packet.messageId==delivered&&p.gateway==viaGateway&&ok&&(DeliveryCore::transportReceiptCompletes(p.command,p.gateway,p.host.startsWith("RADIO:"))||(!p.command&&p.packet.type==PacketType::ACK)))p.used=false;
    }
    Message m;String remote;bool gateway;
    // Exactly one dispatcher owns inbound application messages from all transports.
    for(unsigned receiveBudget=0;receiveBudget<4;++receiveBudget) {
        if(!transportManager.receive(m,remote,gateway))break;
        if(!accept(m,remote,gateway))continue;
        JsonDocument incomingBody;deserializeJson(incomingBody,m.payload);
        bool radio=remote.startsWith("RADIO:");
        if(c.role==DeviceRole::NODE&&!gateway&&(m.source==masterId()||m.source=="GATEWAY"))transportManager.masterReceipt();
        if(radio&&m.type==PacketType::HELLO&&m.source!="GATEWAY")radioRoutes.observe(m.source.c_str(),remote.substring(6).c_str(),m.radioHopCount,now);
        if(radio&&m.destination!=c.device_id&&m.destination!="BROADCAST"&&!(c.role==DeviceRole::MASTER&&m.destination=="GATEWAY")){
            String next=radioHost(m.destination);
            if(E220Core::forwardAllowed(m.ttl,m.radioHopCount,m.radioMaxHops,next.c_str(),remote.c_str())&&!forwardDuplicate(m)){Message relay=m;--relay.ttl;if(track(relay,next)){forwardedNow(m);}else ++failed;}else{++failed;static uint32_t routeWarn=0;if(now-routeWarn>=1000){routeWarn=now;logger.warn("ROUTE","Radio route expired, loop, duplicate or hop limit");}}
        }else if(radio&&c.role==DeviceRole::NODE&&m.type==PacketType::HELLO){
            if(m.destination=="BROADCAST"&&c.registration_open&&m.ttl>1&&!forwardDuplicate(m)){Message relay=m;--relay.ttl;if(track(relay,"RADIO:BROADCAST"))forwardedNow(m);}
        }else if(m.type==PacketType::ACK&&!m.correlationId.isEmpty()&&incomingBody["received_message_id"].as<String>()==m.correlationId) {
            if(gateway)for(auto& p:pending)if(p.used&&p.packet.messageId==m.correlationId&&p.packet.type==PacketType::ACK&&p.packet.correlationId==operationJob.request.messageId&&operationJob.type=="WIFI_RECONNECT")operationJob.acceptance=true;
            if(gateway){transportManager.gatewayReceipt();powerManager.resultPersisted(m.correlationId);if(operationJob.active&&operationJob.type=="MQTT_TEST"&&remote=="MQTT"&&operationJob.probe==m.correlationId)operationJob.receipt=true;}
            // The Gateway has durably stored this execution response. Keep the
            // request identity for deduplication, but release its large replay
            // body; it is no longer needed to recover a lost response.
            if(gateway)for(auto& entry:cache)if(entry.responseId==m.correlationId){entry.response=(const char*)nullptr;entry.responseId=(const char*)nullptr;}
            for(auto& p:pending)if(p.used&&p.gateway==gateway&&p.packet.messageId==m.correlationId&&!p.command)p.used=false;
        } else if(gateway&&m.destination!=c.device_id) {
            Peer* target=peer(m.destination);
            bool routed=false;String routeError="ROUTE_NOT_FOUND";
            if(m.ttl<=1)routeError="HOP_LIMIT_EXCEEDED";
            else if(target&&now-target->seen<=c.route_timeout_seconds*1000UL&&(m.type==PacketType::COMMAND||m.type==PacketType::CONFIG_GET||m.type==PacketType::CONFIG_UPDATE)){Message forwarded=m;--forwarded.ttl;routed=track(forwarded,target->host);routeError="ROUTE_QUEUE_FULL";}
            if(!routed){JsonDocument error;error["event"]="COMMAND_ROUTE_FAILED";error["command_id"]=m.messageId;error["target_device_id"]=m.destination;error["error_code"]=routeError;String body;serializeJson(error,body);upstream(make(PacketType::EVENT,"GATEWAY",body));}
        } else if(c.role==DeviceRole::MASTER&&!gateway&&m.type!=PacketType::COMMAND&&m.type!=PacketType::CONFIG_GET&&m.type!=PacketType::CONFIG_UPDATE) {
            JsonDocument body;deserializeJson(body,m.payload);
            if(m.type==PacketType::HELLO) {
                if(body["device_id"].as<String>()!=m.source||((body["master_id"].as<String>()!=c.device_id)&&!(radio&&c.registration_open&&body["master_id"].as<String>().isEmpty()))||body["role"].as<String>()!="NODE"||!body["heartbeat_seconds"].is<uint32_t>()||body["heartbeat_seconds"].as<uint32_t>()<5||body["heartbeat_seconds"].as<uint32_t>()>3600)return;
                Peer* p=peer(m.source);String address=body["node_address"]|"";
                for(auto& other:peers)if(other.id.length()&&other.id!=m.source&&address.length()&&other.address==address){++failed;return;}
                if(p&&p->host!=remote&&now-p->seen<c.route_timeout_seconds*1000){++failed;return;}
                if(!p)for(auto& candidate:peers)if(candidate.id.isEmpty()){p=&candidate;break;}
                if(!p){++failed;return;}p->id=m.source;p->host=remote;p->address=address;p->heartbeat=body["heartbeat_seconds"]|30;p->seen=now;
                if(radio){Message greeting=make(PacketType::HELLO,m.source,bodyStatus());track(greeting,remote);}
            }
            Peer* p=peer(m.source);if(!p)return;p->seen=now;
            if(m.type==PacketType::TELEMETRY) {
                if(!SensorTelemetry::valid(body.as<JsonObjectConst>()))return;
                p->telemetry=m.payload;
            }
            if(m.type==PacketType::ACK){String cid=body["command_id"]|"",state=body["status"]|"";
                for(auto& item:pending)if(item.used&&item.command&&item.packet.messageId==cid&&item.packet.destination==m.source){if(state=="ACCEPTED"||state=="RECEIVED"||state=="DUPLICATE"){item.acknowledged=true;latestState="ACKNOWLEDGED";}else if(state=="REJECTED"||state=="BUSY"){item.used=false;latestState="FAILED";}}
            }
            if(m.type==PacketType::RESPONSE){String cid=body["command_id"]|"";complete(cid,m.source);latestCommand=cid;latestState=body["status"].as<String>()=="SUCCESS"?"COMPLETED":"FAILED";latestResult=m.payload;}
            // Cache only ingestion packets; command retransmissions must replay the cached result.
            bool duplicate=false;
            for(auto& entry:cache)if(entry.source==m.source&&entry.id==m.messageId){if(entry.request!=m.typeString()+m.payload){++failed;return;}duplicate=true;break;}
            if(!duplicate) {
                Message forward=m;forward.destination="GATEWAY";
                if((transportManager.mqttConnected()||(!c.server_host.isEmpty()&&LocalIpCore::gatewayCredential(transportManager.localMode(),!c.gateway_token.isEmpty(),!c.local_ip_token.isEmpty())))&&!upstream(forward)){++failed;return;}
                auto& entry=cache[cacheNext++%32];entry=Cached{};entry.source=m.source;entry.id=m.messageId;entry.request=m.typeString()+m.payload;entry.seen=now;
            }
            else ++duplicates;
            if(m.type!=PacketType::ACK&&DeliveryCore::applicationReceiptRequired(transportManager.localMode(),radio)) {
                Message receipt=make(PacketType::ACK,m.source,"{\"received_message_id\":\""+m.messageId+"\",\"status\":\"RECEIVED\"}");receipt.correlationId=m.messageId;track(receipt,remote);
            }
        } else if(m.type==PacketType::COMMAND||m.type==PacketType::CONFIG_GET||m.type==PacketType::CONFIG_UPDATE)handleCommand(m,remote,gateway);
    }
    Message bootResult;if(powerManager.bootResponse(bootResult)&&upstream(bootResult))powerManager.bootResponseQueued();
    if(c.enabled&&(wifiManager.isConnected()||loraTransport.isInitialized())&&!powerManager.draining()&&!(operationJob.active&&(operationJob.type=="WIFI_RECONNECT"||operationJob.type=="MQTT_CONNECT"||operationJob.type=="MQTT_RECONNECT"||operationJob.type=="MQTT_DISCONNECT"))) {
        if(now-sensorCheckedAt>=1000) {
            sensorCheckedAt=now;JsonDocument sensor;deserializeJson(sensor,sensorManager.telemetryJson());String state=sensor["status"]|"UNKNOWN";
            if(state!=previousSensorStatus) {
                previousSensorStatus=state;
                JsonDocument event;event["sensor_status"]=state;event["sensor_id"]=sensor["sensor_id"];String body;serializeJson(event,body);
                upstream(make(state=="READ_ERROR"?PacketType::ALARM:PacketType::EVENT,c.role==DeviceRole::MASTER?"GATEWAY":c.master_id,body));
                if(state=="READ_ERROR")logger.warn("SENSOR","DHT11 read failed; measurements omitted");
            }
        }
        if(now-diagnosticAt>=60000) {
            diagnosticAt=now;JsonDocument d,history;deserializeJson(d,bodyStatus());deserializeJson(history,logger.getRecentJson());
            auto errors=d["recent_errors"].to<JsonArray>();auto records=history.as<JsonArray>();
            for(int i=(int)records.size()-1;i>=0&&errors.size()<3;--i)if(records[i]["level"].as<String>()!="INFO")errors.add(records[i]);
            String body;serializeJson(d,body);upstream(make(PacketType::DIAGNOSTIC,c.role==DeviceRole::MASTER?"GATEWAY":c.master_id,body));
        }
        if(c.role==DeviceRole::MASTER&&loraTransport.isInitialized()&&(int32_t)(now-radioAnnounceAt)>=0){radioAnnounceAt=now+30000+(esp_random()%2000);track(make(PacketType::HELLO,"BROADCAST",bodyStatus()),"RADIO:BROADCAST");}
        if(now-helloAt>=30000+helloJitter){helloAt=now;helloJitter=esp_random()%2000;upstream(make(PacketType::HELLO,c.role==DeviceRole::MASTER?"GATEWAY":c.master_id,bodyStatus()));}
        if(now-heartbeatAt>=c.heartbeat_seconds*1000UL){heartbeatAt=now;upstream(make(PacketType::HEARTBEAT,c.role==DeviceRole::MASTER?"GATEWAY":c.master_id,bodyStatus()));}
        if(now-telemetryAt>=c.telemetry_seconds*1000UL){telemetryAt=now;for(unsigned i=0;i<3;++i)upstream(make(PacketType::TELEMETRY,c.role==DeviceRole::MASTER?"GATEWAY":c.master_id,sensorManager.telemetryJson(i)));}
        // HTTP polling works behind NAT and remains available when MQTT is disabled or disconnected.
        bool pendingWork=false;for(const auto& item:pending)if(item.used){pendingWork=true;break;}
        if(DeliveryCore::pollGateway(transportManager.localMode(),CanonicalMqtt::configured())&&!pendingWork&&transportManager.idle()&&(c.role==DeviceRole::MASTER||(transportManager.localMode()&&c.routing_policy!="MASTER_ONLY"))&&now-pollAt>=2000&&(!c.gateway_token.isEmpty()||!c.local_ip_token.isEmpty())&&!c.server_host.isEmpty()) {pollAt=now;Message poll=make(PacketType::STATUS,"GATEWAY",bodyStatus());track(poll,c.server_host,true);}
    }
    unsigned work=(operationJob.active||mqttRestoreArmed)?1:0;for(auto& p:pending)if(p.used){
        if(DeliveryCore::discardForPower(powerManager.draining(),p.packet.source==c.device_id,p.command,p.packet.type==PacketType::TELEMETRY,p.packet.type==PacketType::HEARTBEAT,p.packet.type==PacketType::HELLO,p.packet.type==PacketType::STATUS)){p=Pending{};++failed;}else ++work;
    }powerManager.loop(work,transportManager.idle());
    now=millis(); // Packets queued above may have a newer timestamp than the loop entry.
    for(auto& p:pending)if(p.used) {
        if(now-p.created>c.message_max_age_ms){p.used=false;++failed;latestState=p.command?"TIMEOUT":latestState;continue;}
        if(!DeliveryCore::eligible(p.inFlight,p.command,p.acknowledged,now,p.next))continue;
        if(p.attempts>c.retry_count){p.used=false;++failed;if(p.command)latestState="TIMEOUT";continue;}
        if(transportManager.send(p.packet,p.host,p.gateway)){p.inFlight=true;++p.attempts;p.next=now+c.command_timeout_ms*min(8,1<<min(3,(int)p.attempts-1));if(p.command){latestCommand=p.packet.messageId;latestState="SENT";}}
        else p.next=now+1000;
    }
}
String NetworkService::command(const String& destination,const String& type,const String& payload) {
    auto& c=configManager.get();if(c.role!=DeviceRole::MASTER||!c.enabled)return "";
    Peer* p=peer(destination);if(!p)return "";
    JsonDocument args;if(deserializeJson(args,payload)||!args.is<JsonObject>())return "";
    Message m=make(PacketType::COMMAND,destination,"{}");JsonDocument d;d["command_id"]=m.messageId;d["command"]=type;d["arguments"]=args.as<JsonVariant>();m.payload="";serializeJson(d,m.payload);
    if(!track(m,p->host))return "";latestCommand=m.messageId;latestState="QUEUED";return m.messageId;
}
String NetworkService::statusJson() const {
    JsonDocument d;d["operational_job"]["active"]=operationJob.active;if(operationJob.active){d["operational_job"]["command_id"]=operationJob.request.messageId;d["operational_job"]["command"]=operationJob.type;d["operational_job"]["elapsed_ms"]=millis()-operationJob.started;}d["mqtt_runtime_restore_armed"]=mqttRestoreArmed;d["last_command_id"]=latestCommand;d["command_state"]=latestState;d["command_result"]=latestResult;d["delivery_failures"]=failed;d["duplicates"]=duplicates;d["radio_ready"]=loraTransport.isInitialized();d["discovered_master_id"]=discoveredMaster;auto routes=d["radio_routes"].to<JsonArray>();for(const auto& row:radioRoutes.rows)if(row.destination[0]){auto route=routes.add<JsonObject>();route["destination"]=row.destination;route["next_hop"]=row.nextHop;route["hop_count"]=row.hops;route["age_ms"]=millis()-row.seen;}
    int count=0;for(auto& p:pending)if(p.used)++count;d["pending"]=count;
    auto a=d["nodes"].to<JsonArray>();for(auto& p:peers)if(p.id.length()){auto n=a.add<JsonObject>();n["device_id"]=p.id;n["ip"]=p.host;n["node_address"]=p.address;if(p.telemetry.length())n["telemetry"]=serialized(p.telemetry);n["age_ms"]=millis()-p.seen;uint32_t age=(millis()-p.seen)/1000;n["status"]=age<=p.heartbeat*2?"ONLINE":age<=max(configManager.get().route_timeout_seconds,p.heartbeat*3)?"STALE":"OFFLINE";}
    String s;serializeJson(d,s);return s;
}
