#include "canonical_mqtt.h"
#include "../config/hardware_profile.h"
#include "transport_manager.h"
#include "../core/config_manager.h"
#include "../config/mqtt_contract.h"
#include "../config/mqtt_digest.h"
#include "../config/strict_json.h"
#include "../config/mqtt_wire.h"
#include "../config/mqtt_staging.h"
#include "../config/mqtt_failure.h"
#include "../config/mqtt_retry_stamp.h"
#include <mqtt_client.h>
#include <WiFi.h>
#include <atomic>
#include <time.h>
#include <memory>
#include <new>
#include <esp_heap_caps.h>
#include "mqtt_recovery_core.h"

namespace CanonicalMqtt {
namespace {
JsonDocument profile;
esp_mqtt_client_handle_t client=nullptr;
std::atomic<bool> requestedEnabled{!HardwareProfile::ExplicitRole},clientRunning{false};
std::atomic<bool> up{false};std::atomic<uint32_t> reconnects{0},dropped{0};std::atomic<int> failure{0};
struct Received{String topic;String raw;};QueueHandle_t queue=nullptr;
Received assembly{};size_t offset=0;bool assembling=false;
SemaphoreHandle_t runtimeMutex=nullptr;
String inbox[8];unsigned inboxCount=0;
String comparison="UNKNOWN";uint32_t comparedAt=0;String mismatchFields="[]";
struct Receipt{String id,hash,reply;};Receipt receipts[32];unsigned receiptIndex=0;
std::atomic<uint32_t> publishAttempts{0},enqueued{0},pubacks{0};std::atomic<int> publishError{0};
std::atomic<int> subscriptions[6];std::atomic<unsigned> subscriptionMask{0};
std::atomic<bool> subscriptionRejected{false};
std::atomic<uint32_t> lastConnected{0},lastQueued{0},lastPuback{0},lastReceived{0};
String device;std::atomic<uint32_t> sequence{0};uint32_t sharedRevision=0;
std::atomic<bool> recycleRequested{false};
std::atomic<int> lastTlsError{0};std::atomic<uint32_t> lastCertFlags{0},recycles{0},unconfirmedBytesReleased{0};
MqttRecoveryCore::Policy recovery;
bool startClient();
bool send(const String& type,JsonObjectConst data,const String& id="",const String& correlation=""){
 ++publishAttempts;if(!client||!up){publishError=1;return false;}if(esp_mqtt_client_get_outbox_size(client)>16384){publishError=2;return false;}String topic;if(!MqttContract::topic(profile.as<JsonObjectConst>(),type,topic))return false;
 JsonDocument d;d["schema_version"]=1;d["message_id"]=id.isEmpty()?device+":"+String(esp_random(),HEX)+":"+String(++sequence):id;
 d["gateway_id"]=profile["gateway_id"];d["master_id"]=device;d["device_id"]=device;d["message_type"]=type;
 auto now=time(nullptr);uint32_t stamp=MqttRetryStamp::select(now>1700000000?uint32_t(now):0,data["packet"].is<JsonObjectConst>(),data["packet"]["timestamp"]|0U);d["timestamp"]=stamp;if(type!="status"&&type!="telemetry"){if(stamp<1700000000||uint64_t(stamp)+120<uint64_t(now))return false;d["expires_at"]=stamp+120;}
 if(correlation.length())d["correlation_id"]=correlation;d["data"]=data;String raw;serializeJson(d,raw);
 if(d.overflowed()||raw.length()>6144){publishError=3;return false;}
 bool ok=esp_mqtt_client_enqueue(client,topic.c_str(),raw.c_str(),raw.length(),1,0,true)>=0;if(ok){++enqueued;publishError=0;lastQueued=time(nullptr);}else publishError=4;return ok;
}
void event(void*,esp_event_base_t,int32_t,void* data){
 auto e=(esp_mqtt_event_handle_t)data;
 switch(e->event_id){
 case MQTT_EVENT_CONNECTED:{up=true;failure=0;subscriptionMask=0;subscriptionRejected=false;lastConnected=time(nullptr);unsigned i=0;for(auto type:{"command","config","chat/message","chat/ack","chat/status","chat/command"}){String topic;MqttContract::topic(profile.as<JsonObjectConst>(),type,topic);subscriptions[i++]=esp_mqtt_client_subscribe(client,topic.c_str(),1);}break;}
 case MQTT_EVENT_SUBSCRIBED:{for(unsigned i=0;i<6;i++)if(subscriptions[i]==e->msg_id){if(e->data&&e->data_len==1&&uint8_t(e->data[0])==1)subscriptionMask.fetch_or(1U<<i);else if(e->data&&e->data_len&&uint8_t(e->data[0])>=128)subscriptionRejected=true;}break;}
 case MQTT_EVENT_PUBLISHED:++pubacks;lastPuback=time(nullptr);break;
 case MQTT_EVENT_DISCONNECTED:up=false;subscriptionMask=0;++reconnects;break;
 case MQTT_EVENT_ERROR:up=false;
  lastTlsError=e->error_handle?e->error_handle->esp_tls_stack_err:0;lastCertFlags=e->error_handle?e->error_handle->esp_tls_cert_verify_flags:0;
  failure=e->error_handle?classifyMqttFailure(e->error_handle->error_type==MQTT_ERROR_TYPE_CONNECTION_REFUSED,e->error_handle->connect_return_code,lastTlsError.load(),lastCertFlags.load()):3;
  // Event callbacks only request cleanup. The network worker owns stop/destroy.
  if(MqttRecoveryCore::allocationFailure(lastTlsError.load()))recycleRequested=true;
  break;
 case MQTT_EVENT_DATA:
   lastReceived=time(nullptr);
   if(e->current_data_offset==0){
     offset=0;assembly=Received{};assembling=e->total_data_len>0&&e->total_data_len<=6144&&e->topic_len>0&&e->topic_len<192;
     if(assembling){assembly.topic=String(e->topic,e->topic_len);assembling=assembly.raw.reserve(e->total_data_len);}
   }
   if(!assembling||e->current_data_offset!=(int)offset||e->data_len<0||offset+e->data_len>6144){assembling=false;assembly=Received{};break;}
   if(!assembly.raw.concat(e->data,e->data_len)){assembling=false;assembly=Received{};++dropped;break;}offset+=e->data_len;
   if(offset==(size_t)e->total_data_len){
     if(e->qos!=1||e->retain||strlen(assembly.raw.c_str())!=offset)++dropped;
     else {Received* pending=new(std::nothrow) Received(std::move(assembly));if(!pending)++dropped;else if(xQueueSend(queue,&pending,0)!=pdTRUE){delete pending;++dropped;}}
     assembly=Received{};assembling=false;
   }break;
 default:break;
 }
}
bool id(JsonVariantConst v){if(!v.is<const char*>())return false;String s=v.as<String>();if(s.isEmpty()||s.length()>48)return false;for(auto c:s)if(!isalnum(c)&&c!='_'&&c!='.'&&c!=':'&&c!='-')return false;return true;}
void receive(const Received& r){
 const String& raw=r.raw;if(!StrictJson::valid(raw))return;JsonDocument d;if(!MqttWire::incoming(raw,r.topic,profile.as<JsonObjectConst>(),device,d))return;
 for(JsonPair p:d.as<JsonObject>()){String k=p.key().c_str();if(k!="schema_version"&&k!="message_id"&&k!="gateway_id"&&k!="master_id"&&k!="device_id"&&k!="message_type"&&k!="timestamp"&&k!="data"&&k!="correlation_id"&&k!="configuration_revision"&&k!="expires_at")return;}
 if(!d["schema_version"].is<unsigned>()||d["schema_version"].as<unsigned>()!=1||!id(d["message_id"])||d["gateway_id"]!=profile["gateway_id"]||d["master_id"].as<String>()!=device||!id(d["device_id"])||!d["data"].is<JsonObject>())return;
 String type=d["message_type"]|"",topic;if(!MqttContract::topic(profile.as<JsonObjectConst>(),type,topic)||topic!=r.topic)return;
 if(d["device_id"].as<String>()==device)return; // Ignore this Master's own config/chat echoes.
 if(d["device_id"].as<String>()!="GATEWAY")return;
 auto now=time(nullptr);if(now<1700000000||!d["timestamp"].is<uint32_t>()||!d["expires_at"].is<uint32_t>())return;
 uint32_t stamp=d["timestamp"],expires=d["expires_at"];if(!stamp||stamp>now+30||expires<=stamp||expires>stamp+300||expires<now)return;
 String mid=d["message_id"].as<String>(),hash=MqttDigest::hash(raw),replyCache;
 for(auto& old:receipts)if(old.id==mid){if(old.hash!=hash){++dropped;return;}if(old.reply.length()){JsonDocument reply;deserializeJson(reply,old.reply);send("chat/ack",reply.as<JsonObjectConst>(),"",mid);return;}if(type!="command")return;}
 auto data=d["data"].as<JsonObjectConst>();bool accepted=false;
 if(type=="command"&&data.size()==1&&data["packet"].is<JsonObjectConst>()){
   String packet;serializeJson(data["packet"],packet);Message m;if(Message::fromJson(packet,m)&&m.source=="GATEWAY"&&(m.destination==device||m.destination.length()>0)&&(m.type==PacketType::COMMAND||m.type==PacketType::CONFIG_GET||m.type==PacketType::CONFIG_UPDATE))accepted=transportManager.enqueue(packet,"GATEWAY",true);
 }else if(type=="config"){
   String op=data["op"]|"";
   if((op=="SWITCH_PREPARE"&&data.size()==6)||((op=="SWITCH_CONFIRM"||op=="SWITCH_ROLLBACK")&&data.size()==5)){String control;serializeJson(data,control);mqttStaging.control(control);accepted=true;}
   else if(op=="PERSISTED"&&data.size()==3&&id(data["received_message_id"])&&data["status"].as<String>()=="PERSISTED"){
     Message ack;ack.messageId=mid;ack.source="GATEWAY";ack.destination=device;ack.nextHop=device;ack.origin="GATEWAY";ack.type=PacketType::ACK;ack.ttl=1;ack.timestamp=time(nullptr);ack.correlationId=data["received_message_id"].as<String>();
     JsonDocument payload;payload["received_message_id"]=data["received_message_id"];payload["status"]="RECEIVED";serializeJson(payload,ack.payload);accepted=transportManager.enqueue(ack.toJson(),"GATEWAY",true);
   }else if(op=="COMPARE"&&data.size()==3){
     JsonDocument own; if(MqttDigest::snapshot(profile.as<JsonObjectConst>(),sharedRevision,own)){
       auto peer=data["snapshot"].as<JsonObjectConst>();String peerRaw;serializeJson(peer,peerRaw);
       if(peer["schema_version"].as<int>()==1&&peer["values"]["gateway_id"]==profile["gateway_id"]&&MqttDigest::hash(peerRaw)==data["digest"].as<String>()){
         JsonDocument fields;auto a=fields.to<JsonArray>();for(JsonPairConst p:own["values"].as<JsonObjectConst>())if(p.value()!=peer["values"][p.key().c_str()])a.add(String("profile.")+p.key().c_str());if(peer["shared_revision"]!=own["shared_revision"])a.add("shared_revision");comparison=a.size()?"MISMATCH":"MATCH";mismatchFields="";serializeJson(fields,mismatchFields);comparedAt=millis();accepted=true;
       }
     }
   }else if(op=="PROBE"&&data.size()==4&&data["participant"].as<String>()=="GATEWAY"&&data["nonce"].as<String>().length()==32&&data["transaction_id"].as<String>().length()==36){JsonDocument reply;reply.set(data);reply["op"]="PROBE_ACK";reply["participant"]="MASTER";accepted=send("config",reply.as<JsonObjectConst>(),"",mid);}
 }else if(type=="chat/message"&&data.size()==4&&id(data["sender_id"])&&data["sender_id"].as<String>()=="GATEWAY"&&id(data["recipient_id"])&&data["text"].is<const char*>()&&data["text"].as<String>().length()<=2000){
   JsonDocument ack;ack["received_message_id"]=mid;ack["conversation_id"]=data["conversation_id"];
   String content=data["text"].as<String>(),conversation=data["conversation_id"]|"";
   unsigned characters=0;for(size_t i=0;i<content.length();i++)if((content[i]&0xc0)!=0x80)characters++;
   bool valid=characters>0&&characters<=500&&conversation.length()>0&&conversation.length()<=128;
   for(size_t i=0;i<conversation.length();i++)if((unsigned char)conversation[i]<32||(unsigned char)conversation[i]>126)valid=false;
   if(!valid)return;
   if(data["recipient_id"].as<String>()==device&&inboxCount<8){inbox[inboxCount++]=content;ack["state"]="DELIVERED";}
   else {ack["state"]="FAILED";ack["error_code"]=data["recipient_id"].as<String>()==device?"QUEUE_FULL":"NOT_AVAILABLE";}
   serializeJson(ack,replyCache);send("chat/ack",ack.as<JsonObjectConst>(),"",mid);accepted=true;
 }
 if(accepted){receipts[receiptIndex]={mid,hash,replyCache};receiptIndex=(receiptIndex+1)%32;}
}
bool startClient(){ esp_mqtt_client_config_t cfg={};cfg.host=profile["host"];cfg.port=profile["port"];cfg.client_id=profile["client_id"];cfg.username=profile["username"];cfg.password=profile["password"];cfg.cert_pem=profile["ca_cert"];
 cfg.transport=MQTT_TRANSPORT_OVER_SSL;cfg.protocol_ver=MQTT_PROTOCOL_V_3_1_1;cfg.keepalive=profile["keepalive_s"];cfg.network_timeout_ms=profile["timeout_s"].as<int>()*1000;cfg.reconnect_timeout_ms=60000;cfg.buffer_size=2048;cfg.task_stack=6144;cfg.skip_cert_common_name_check=false;
 client=esp_mqtt_client_init(&cfg);if(!client)return false;
 esp_mqtt_client_register_event(client,MQTT_EVENT_ANY,event,nullptr);
 if(esp_mqtt_client_start(client)!=ESP_OK){esp_mqtt_client_destroy(client);client=nullptr;return false;}
 clientRunning=true;return true;
}

}
void begin(const String& raw,uint32_t revision){
 sharedRevision=revision;
 if(!runtimeMutex)runtimeMutex=xSemaphoreCreateMutex();
 if((HardwareProfile::ExplicitRole&&!HardwareProfile::Master)||configManager.get().role!=DeviceRole::MASTER||deserializeJson(profile,raw))return;String field;if(!MqttContract::validate(profile.as<JsonObjectConst>(),field,true)){profile.clear();return;}
 device=configManager.get().device_id;queue=xQueueCreate(3,sizeof(Received*));if(!queue)return;
 if(requestedEnabled.load()&&!startClient())recovery.failed(millis());
}
void requestEnabled(bool enabled){requestedEnabled=enabled;}
void requestReconnect(){recycleRequested=true;}
void loop(){
 if(runtimeMutex&&xSemaphoreTake(runtimeMutex,0)==pdTRUE){
  if(recycleRequested.exchange(false)){
   // Release SDK task, TLS context and unconfirmed broker outbox together.
   // NetworkService keeps application-level retries until PERSISTED/expiry.
   up=false;subscriptionMask=0;
   if(client){unconfirmedBytesReleased+=esp_mqtt_client_get_outbox_size(client);if(clientRunning)esp_mqtt_client_stop(client);esp_mqtt_client_destroy(client);client=nullptr;}
   clientRunning=false;assembly=Received{};assembling=false;offset=0;++recycles;recovery.failed(millis());
  }
  if(!client&&requestedEnabled&&profile.size()&&WiFi.isConnected()&&time(nullptr)>1700000000&&recovery.due(millis(),heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT))){if(startClient())recovery.started();else recovery.failed(millis());}
  xSemaphoreGive(runtimeMutex);
 }
 if(client&&requestedEnabled.load()!=clientRunning.load()){if(requestedEnabled){if(esp_mqtt_client_start(client)==ESP_OK)clientRunning=true;}else{if(esp_mqtt_client_stop(client)==ESP_OK){clientRunning=false;up=false;subscriptionMask=0;}}}if(!requestedEnabled)return;if(up)recovery.connected();if(!queue||!runtimeMutex||xSemaphoreTake(runtimeMutex,0)!=pdTRUE)return;Received* pending=nullptr;if(xQueueReceive(queue,&pending,0)==pdTRUE){std::unique_ptr<Received> owner(pending);receive(*pending);}static uint32_t sent=0;if(up&&millis()-sent>30000){sent=millis();JsonDocument snapshot,data;if(MqttDigest::snapshot(profile.as<JsonObjectConst>(),sharedRevision,snapshot)){data["op"]="COMPARE";data["snapshot"]=snapshot;data["digest"]=MqttDigest::digest(snapshot);send("config",data.as<JsonObjectConst>());}}xSemaphoreGive(runtimeMutex);}
bool controlReply(const String& raw){if(!runtimeMutex||xSemaphoreTake(runtimeMutex,pdMS_TO_TICKS(20))!=pdTRUE)return false;JsonDocument d;bool ok=!deserializeJson(d,raw)&&send("config",d.as<JsonObjectConst>());xSemaphoreGive(runtimeMutex);return ok;}
bool connected(){return up;}
bool configured(){return client!=nullptr;}
bool publish(const Message& packet){if(!runtimeMutex||xSemaphoreTake(runtimeMutex,0)!=pdTRUE)return false;JsonDocument parsed,data;if(deserializeJson(parsed,packet.toJson())){xSemaphoreGive(runtimeMutex);return false;}data["packet"]=parsed;String type=packet.type==PacketType::TELEMETRY?"telemetry":packet.type==PacketType::ACK||packet.type==PacketType::RESPONSE?"command_ack":packet.type==PacketType::EVENT||packet.type==PacketType::ALARM||packet.type==PacketType::DIAGNOSTIC?"events":"status";bool ok=send(type,data.as<JsonObjectConst>(),packet.messageId);xSemaphoreGive(runtimeMutex);return ok;}
String status(){JsonDocument d;if(!runtimeMutex||xSemaphoreTake(runtimeMutex,pdMS_TO_TICKS(20))!=pdTRUE){d["state"]="unknown";d["status_label"]=!WiFi.isConnected()?"WIFI_DISCONNECTED":time(nullptr)<1700000000?"TIME_NOT_SYNCED":"UNKNOWN";d["station_ip"]=WiFi.localIP().toString();d["comparison"]["status"]="UNKNOWN";d["subscription_state"]="UNKNOWN";String raw;serializeJson(d,raw);return raw;}d["tls_stack_error"]=lastTlsError.load();d["certificate_verify_flags"]=lastCertFlags.load();d["tls_client_recycles"]=recycles.load();d["unconfirmed_outbox_bytes_released"]=unconfirmedBytesReleased.load();d["tls_recovery_pending"]=recovery.pending;d["runtime_enabled"]=requestedEnabled.load();d["device_role"]=configManager.roleString();d["wifi_state"]=WiFi.isConnected()?"CONNECTED":"WIFI_DISCONNECTED";d["station_ip"]=WiFi.localIP().toString();d["time_state"]=time(nullptr)>1700000000?"CLOCK_VALID":"TIME_NOT_SYNCED";d["unix_time"]=time(nullptr);d["ntp_status"]="UNKNOWN";d["status_label"]=!WiFi.isConnected()?"WIFI_DISCONNECTED":time(nullptr)<1700000000?"TIME_NOT_SYNCED":up?"CONNECTED":failure==1?"TLS_FAILED":failure==2?"AUTH_FAILED":failure==3?"CONNECTION_FAILED":client?"CONNECTING":"UNKNOWN";d["tls_verification_required"]=true;d["tls_state"]=up?"VERIFIED":failure==1?"TLS_FAILED":"UNKNOWN";d["authentication_state"]=up?"ACCEPTED":failure==2?"AUTH_FAILED":"UNKNOWN";d["subscription_state"]=!up?"UNKNOWN":subscriptionRejected?"SUBSCRIPTION_REJECTED":subscriptionMask==63?"READY":"SUBSCRIPTION_PENDING";d["last_connected_at"]=lastConnected.load();d["last_outbound_queued_at"]=lastQueued.load();d["last_broker_puback_at"]=lastPuback.load();d["last_broker_frame_received_at"]=lastReceived.load();d["chat_inbox_count"]=inboxCount;d["outbox_bytes"]=client?esp_mqtt_client_get_outbox_size(client):0;d["publish_attempts"]=publishAttempts.load();d["enqueued"]=enqueued.load();d["pubacks"]=pubacks.load();d["publish_error"]=publishError.load();d["state"]=!requestedEnabled?(clientRunning?"pause_requested":"paused"):up?"connected":failure==1?"TLS failure":failure==2?"authentication failure":failure==3?"connection failure":client?"connecting":"disconnected";d["reconnect_count"]=reconnects.load();d["dropped"]=dropped.load();d["comparison"]["status"]=comparedAt&&millis()-comparedAt<120000?comparison:"UNKNOWN";d["comparison"]["message"]=comparison=="MISMATCH"?"Master config differs":"";JsonDocument f;deserializeJson(f,mismatchFields);d["comparison"]["fields"]=f;String s;serializeJson(d,s);xSemaphoreGive(runtimeMutex);return s;}
}
