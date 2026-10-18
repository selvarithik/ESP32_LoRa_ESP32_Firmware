#include "network_service.h"
#include "config_manager.h"
#include "logger.h"
#include "system_manager.h"
#include "../communication/transport_manager.h"
#include "../communication/wifi_manager.h"
#include "../diagnostics/diagnostics_manager.h"
#include "../sensors/sensor_manager.h"
#include "../protocol/packet.h"
#include <ArduinoJson.h>
#include <time.h>
#include <cmath>

NetworkService networkService;
namespace {
struct Peer {String id,host,address,telemetry;uint32_t seen=0,heartbeat=30;};
struct Pending {bool used=false,gateway=false,command=false;Message packet;String host;uint8_t attempts=0;uint32_t next=0,created=0;};
struct Cached {String source,id,request,response;uint32_t seen=0;};
Peer peers[12];Pending pending[12];Cached cache[32];uint8_t cacheNext=0;
uint32_t heartbeatAt=0,telemetryAt=0,helloAt=0,pollAt=0,sequence=0,failed=0,duplicates=0;
uint32_t sensorCheckedAt=0,diagnosticAt=0;
String previousSensorStatus;
String latestCommand,latestState="IDLE",latestResult="{}";
Peer* peer(const String& id){for(auto& p:peers)if(p.id==id)return &p;return nullptr;}
bool listed(const String& id){String csv=","+configManager.get().allowed_peers_csv+",";csv.replace(" ","");return csv.indexOf(","+id+",")>=0;}
Message make(PacketType type,const String& destination,const String& body) {Message m=Packet::make(type,configManager.get().device_id,destination,destination,body,configManager.get().max_hops);m.origin=m.source;m.sequence=++sequence;return m;}
bool track(const Message& m,const String& host,bool gateway=false) {
    if(host.isEmpty()&&!gateway)return false;
    if(m.payload.length()>MAX_PAYLOAD_BYTES||m.toJson().length()>MAX_FRAME_BYTES){++failed;return false;}
    for(auto& p:pending)if(p.used&&p.packet.messageId==m.messageId&&p.gateway==gateway)return true;
    for(auto& p:pending)if(!p.used){p=Pending{};p.used=true;p.packet=m;p.host=host;p.gateway=gateway;p.command=m.type==PacketType::COMMAND||m.type==PacketType::CONFIG_GET||m.type==PacketType::CONFIG_UPDATE;p.created=millis();return true;}
    ++failed;logger.warn("DELIVERY","Pending queue full; new message rejected");return false;
}
bool upstream(const Message& m) {auto& c=configManager.get();return c.role==DeviceRole::MASTER?((transportManager.mqttConnected()||!c.gateway_token.isEmpty())&&track(m,c.server_host,true)):(!c.mesh_key.isEmpty()&&!c.master_id.isEmpty()&&track(m,c.master_host));}
void complete(const String& id,const String& source) {for(auto& p:pending)if(p.used&&p.command&&p.packet.messageId==id&&p.packet.destination==source)p.used=false;}
String bodyStatus() {
    JsonDocument d;auto& c=configManager.get();d["device_id"]=c.device_id;d["device_name"]=c.device_name;d["role"]=configManager.roleString();d["master_id"]=c.role==DeviceRole::MASTER?c.device_id:c.master_id;d["node_address"]=c.node_address;
    d["firmware_version"]="1.0.0-part1";d["configuration_revision"]=c.configuration_revision;d["heartbeat_seconds"]=c.heartbeat_seconds;d["uptime_s"]=millis()/1000;d["free_heap"]=ESP.getFreeHeap();d["wifi_connected"]=wifiManager.isConnected();if(wifiManager.isConnected())d["wifi_rssi"]=wifiManager.rssi();
    d["gateway_reachable"]=transportManager.gatewayConnected();d["transport"]=transportManager.mqttConnected()?"MQTT":"HTTP";
    d["reset_reason"]=systemManager.resetReason();d["internet_status"]=transportManager.internetStatus();
    JsonDocument sensor;deserializeJson(sensor,sensorManager.telemetryJson());d["sensor_status"]=sensor["status"];d["enabled"]=c.enabled;
    String s;serializeJson(d,s);return s;
}
bool execute(const Message& m,String& result) {
    JsonDocument d;deserializeJson(d,m.payload);String type=d["command"]|"";
    if(m.type==PacketType::CONFIG_GET)type="GET_CONFIG";
    if(m.type==PacketType::CONFIG_UPDATE)type="CONFIG_UPDATE";
    if(type=="PING")result="{\"ok\":true}";
    else if(type=="GET_STATUS"||type=="STATUS")result=bodyStatus();
    else if(type=="DIAGNOSTICS")result=diagnosticsManager.statusJson();
    else if(type=="GET_TELEMETRY")result=sensorManager.telemetryJson();
    else if(type=="GET_CONFIG")result=configManager.toJson();
    else if(type=="SET_TELEMETRY_INTERVAL"||type=="SET_HEARTBEAT_INTERVAL"||type=="CONFIG_UPDATE") {
        JsonDocument update;auto args=d["arguments"].as<JsonObject>();
        if(type=="CONFIG_UPDATE") {
            if(args.isNull()||args.size()==0){result="{\"error\":\"EMPTY_UPDATE\"}";return false;}
            for(JsonPair p:args){String k=p.key().c_str();if(k!="telemetry_seconds"&&k!="heartbeat_seconds"&&k!="enabled"){result="{\"error\":\"LOCAL_CONFIGURATION_REQUIRED\"}";return false;}update[k]=p.value();}
        } else {
            if(!args["seconds"].is<uint32_t>()){result="{\"error\":\"INTEGER_SECONDS_REQUIRED\"}";return false;}
            update[type=="SET_TELEMETRY_INTERVAL"?"telemetry_seconds":"heartbeat_seconds"]=args["seconds"];
        }
        String raw,error;serializeJson(update,raw);
        if(!configManager.update(raw,error)){JsonDocument r;r["error"]=error;result="";serializeJson(r,result);return false;}
        result="{\"saved\":true}";
    } else {result="{\"error\":\"UNSUPPORTED_COMMAND\"}";return false;}
    return true;
}
void handleCommand(const Message& m,const String& remote,bool gateway) {
    JsonDocument d;deserializeJson(d,m.payload);String cid=d["command_id"]|m.messageId;
    if(cid!=m.messageId){++failed;return;}
    time_t utc=time(nullptr);
    if(m.timestamp&&utc>1600000000&&(m.timestamp>utc+30||utc-m.timestamp>configManager.get().message_max_age_ms/1000)){++failed;return;}
    for(auto& c:cache)if(c.source==m.source&&c.id==cid&&c.request!=m.payload){++failed;return;}
    latestCommand=cid;latestState="RECEIVED";
    Message ack=make(PacketType::ACK,m.source,"{\"command_id\":\""+cid+"\",\"status\":\"RECEIVED\"}");ack.correlationId=cid;
    if(gateway)track(ack,configManager.get().server_host,true);else track(ack,remote);
    for(auto& c:cache)if(c.source==m.source&&c.id==cid) {
        ++duplicates;
        if(c.request!=m.payload){++failed;return;}
        if(c.response.length()){Message r;if(Message::fromJson(c.response,r)){if(gateway)track(r,configManager.get().server_host,true);else track(r,remote);}}
        return;
    }
    auto& entry=cache[cacheNext++%32];entry=Cached{};entry.source=m.source;entry.id=cid;entry.request=m.payload;entry.seen=millis();
    latestState="EXECUTING";String data;bool ok=execute(m,data);
    JsonDocument response;response["command_id"]=cid;response["status"]=ok?"SUCCESS":"FAILED";
    JsonDocument parsed;if(!deserializeJson(parsed,data))response["data"]=parsed.as<JsonVariant>();
    String body;serializeJson(response,body);Message r=make(PacketType::RESPONSE,m.source,body);r.correlationId=cid;
    if(r.toJson().length()>MAX_FRAME_BYTES||r.payload.length()>MAX_PAYLOAD_BYTES){r.payload="{\"command_id\":\""+cid+"\",\"status\":\"FAILED\",\"data\":{\"error\":\"RESULT_TOO_LARGE\"}}";ok=false;}
    entry.response=r.toJson();latestResult=data;latestState=ok?"COMPLETED":"FAILED";
    if(gateway)track(r,configManager.get().server_host,true);else track(r,remote);
}
}
void NetworkService::begin(){helloAt=millis()-30000;heartbeatAt=millis();telemetryAt=millis();pollAt=millis();}
bool NetworkService::accept(const Message& m,const String& remote,bool gateway) {
    auto& c=configManager.get();
    if(m.source==c.device_id)return false;
    if(!c.enabled&&m.type!=PacketType::COMMAND&&m.type!=PacketType::CONFIG_GET&&m.type!=PacketType::CONFIG_UPDATE)return false;
    if(gateway)return c.role==DeviceRole::MASTER&&m.source=="GATEWAY";
    if(c.mesh_key.length()<32)return false;
    if(c.role==DeviceRole::NODE)return (remote==c.master_host||remote==transportManager.masterAddress())&&(m.source==c.master_id||m.source=="GATEWAY")&&(m.destination==c.device_id);
    if(m.destination!=c.device_id&&m.destination!="GATEWAY")return false;
    if(m.type==PacketType::HELLO)return c.registration_open||listed(m.source)||peer(m.source);
    Peer* p=peer(m.source);return p&&p->host==remote;
}
void NetworkService::loop() {
    sensorManager.loop();auto& c=configManager.get();uint32_t now=millis();
    String delivered;bool ok,viaGateway;
    static int lastLoggedError=0;
    while(transportManager.delivery(delivered,ok,viaGateway)) {
        int code=transportManager.lastError();
        if(!ok&&(code<0||code>=400)&&code!=lastLoggedError){lastLoggedError=code;logger.warn("NETWORK",code==-1001?"DNS resolution failed":code==-11?"HTTP server read timeout":"HTTP status/error="+String(code));}
        if(ok)lastLoggedError=0;
        for(auto& p:pending)if(p.used&&p.packet.messageId==delivered&&p.gateway==viaGateway&&ok&&!p.command&&(p.gateway||p.packet.type==PacketType::ACK))p.used=false;
    }
    Message m;String remote;bool gateway;
    // Exactly one dispatcher owns inbound application messages from all transports.
    if(transportManager.receive(m,remote,gateway)&&accept(m,remote,gateway)) {
        JsonDocument incomingBody;deserializeJson(incomingBody,m.payload);
        if(m.type==PacketType::ACK&&!m.correlationId.isEmpty()&&incomingBody["received_message_id"].as<String>()==m.correlationId) {
            if(gateway)transportManager.gatewayReceipt();
            for(auto& p:pending)if(p.used&&p.gateway==gateway&&p.packet.messageId==m.correlationId&&!p.command)p.used=false;
        } else if(gateway&&m.destination!=c.device_id) {
            Peer* target=peer(m.destination);
            if(target&&(m.type==PacketType::COMMAND||m.type==PacketType::CONFIG_GET||m.type==PacketType::CONFIG_UPDATE))track(m,target->host);
        } else if(c.role==DeviceRole::MASTER&&!gateway&&m.type!=PacketType::COMMAND&&m.type!=PacketType::CONFIG_GET&&m.type!=PacketType::CONFIG_UPDATE) {
            JsonDocument body;deserializeJson(body,m.payload);
            if(m.type==PacketType::HELLO) {
                if(body["device_id"].as<String>()!=m.source||body["master_id"].as<String>()!=c.device_id||body["role"].as<String>()!="NODE"||!body["heartbeat_seconds"].is<uint32_t>()||body["heartbeat_seconds"].as<uint32_t>()<5||body["heartbeat_seconds"].as<uint32_t>()>3600)return;
                Peer* p=peer(m.source);String address=body["node_address"]|"";
                for(auto& other:peers)if(other.id.length()&&other.id!=m.source&&address.length()&&other.address==address){++failed;return;}
                if(p&&p->host!=remote&&now-p->seen<c.route_timeout_seconds*1000){++failed;return;}
                if(!p)for(auto& candidate:peers)if(candidate.id.isEmpty()){p=&candidate;break;}
                if(!p){++failed;return;}p->id=m.source;p->host=remote;p->address=address;p->heartbeat=body["heartbeat_seconds"]|30;p->seen=now;
            }
            Peer* p=peer(m.source);if(!p)return;p->seen=now;
            if(m.type==PacketType::TELEMETRY) {
                if(!body["valid"].is<bool>()||!body["parameters"].is<JsonObject>())return;
                auto values=body["parameters"].as<JsonObject>();
                if((!body["valid"].as<bool>()&&values.size())||(body["valid"].as<bool>()&&values.size()!=2))return;
                for(JsonPair pair:values) {String key=pair.key().c_str();float value=pair.value().as<float>();if((key!="temperature"&&key!="humidity")||!pair.value().is<float>()||!std::isfinite(value)||value<0||value>(key=="temperature"?50:100))return;}
                p->telemetry=m.payload;
            }
            if(m.type==PacketType::ACK){latestState="ACKNOWLEDGED";}
            if(m.type==PacketType::RESPONSE){String cid=body["command_id"]|"";complete(cid,m.source);latestCommand=cid;latestState=body["status"].as<String>()=="SUCCESS"?"COMPLETED":"FAILED";latestResult=m.payload;}
            // Cache only ingestion packets; command retransmissions must replay the cached result.
            bool duplicate=false;
            for(auto& entry:cache)if(entry.source==m.source&&entry.id==m.messageId){if(entry.request!=m.typeString()+m.payload){++failed;return;}duplicate=true;break;}
            if(!duplicate) {
                Message forward=m;forward.destination="GATEWAY";
                if((transportManager.mqttConnected()||(!c.server_host.isEmpty()&&!c.gateway_token.isEmpty()))&&!upstream(forward)){++failed;return;}
                auto& entry=cache[cacheNext++%32];entry=Cached{};entry.source=m.source;entry.id=m.messageId;entry.request=m.typeString()+m.payload;entry.seen=now;
            }
            else ++duplicates;
            if(m.type!=PacketType::ACK) {
                Message receipt=make(PacketType::ACK,m.source,"{\"received_message_id\":\""+m.messageId+"\",\"status\":\"RECEIVED\"}");receipt.correlationId=m.messageId;track(receipt,remote);
            }
        } else if(m.type==PacketType::COMMAND||m.type==PacketType::CONFIG_GET||m.type==PacketType::CONFIG_UPDATE)handleCommand(m,remote,gateway);
    }
    if(c.enabled&&wifiManager.isConnected()) {
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
        if(now-helloAt>=30000){helloAt=now;upstream(make(PacketType::HELLO,c.role==DeviceRole::MASTER?"GATEWAY":c.master_id,bodyStatus()));}
        if(now-heartbeatAt>=c.heartbeat_seconds*1000UL){heartbeatAt=now;upstream(make(PacketType::HEARTBEAT,c.role==DeviceRole::MASTER?"GATEWAY":c.master_id,bodyStatus()));}
        if(c.role==DeviceRole::NODE&&now-telemetryAt>=c.telemetry_seconds*1000UL){telemetryAt=now;upstream(make(PacketType::TELEMETRY,c.master_id,sensorManager.telemetryJson()));}
        // HTTP polling works behind NAT and remains available when MQTT is disabled or disconnected.
        if(c.role==DeviceRole::MASTER&&now-pollAt>=2000&&!c.gateway_token.isEmpty()&&!c.server_host.isEmpty()) {pollAt=now;Message poll=make(PacketType::STATUS,"GATEWAY",bodyStatus());track(poll,c.server_host,true);}
    }
    now=millis(); // Packets queued above may have a newer timestamp than the loop entry.
    for(auto& p:pending)if(p.used) {
        if(now-p.created>c.message_max_age_ms){p.used=false;++failed;latestState=p.command?"TIMED_OUT":latestState;continue;}
        if((int32_t)(now-p.next)<0)continue;
        if(p.attempts>c.retry_count){p.used=false;++failed;if(p.command)latestState="TIMED_OUT";continue;}
        if(transportManager.send(p.packet,p.host,p.gateway)){++p.attempts;p.next=now+c.command_timeout_ms*min(4,(int)p.attempts);if(p.command){latestCommand=p.packet.messageId;latestState="SENT";}}
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
    JsonDocument d;d["last_command_id"]=latestCommand;d["command_state"]=latestState;d["command_result"]=latestResult;d["delivery_failures"]=failed;d["duplicates"]=duplicates;
    int count=0;for(auto& p:pending)if(p.used)++count;d["pending"]=count;
    auto a=d["nodes"].to<JsonArray>();for(auto& p:peers)if(p.id.length()){auto n=a.add<JsonObject>();n["device_id"]=p.id;n["ip"]=p.host;n["node_address"]=p.address;if(p.telemetry.length())n["telemetry"]=serialized(p.telemetry);n["age_ms"]=millis()-p.seen;uint32_t age=(millis()-p.seen)/1000;n["status"]=age<=p.heartbeat*2?"ONLINE":age<=max(configManager.get().route_timeout_seconds,p.heartbeat*3)?"STALE":"OFFLINE";}
    String s;serializeJson(d,s);return s;
}
