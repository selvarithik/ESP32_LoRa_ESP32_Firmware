#include "mqtt_candidate.h"
#include "../config/mqtt_failure.h"
#include "../core/config_manager.h"
#include "../config/mqtt_contract.h"
#include "../config/strict_json.h"
#include "../config/mqtt_wire.h"
#include <mqtt_client.h>
#include <atomic>
#include <time.h>
#include <new>

namespace MqttCandidate {
namespace {
struct Control{char raw[1025];};QueueHandle_t queue=nullptr;
std::atomic<bool> running{false},stop{false},commit{false},readyPending{false},connected{false},subscribed{false},puback{false},peerProof{false};
std::atomic<int> tlsError{0},failureCode{0};
JsonDocument profile;String transaction,digest,nonce,device;uint32_t revision,deadline;
esp_mqtt_client_handle_t client=nullptr;String incoming;String receivedTopic;
bool send(JsonObjectConst data,const String& correlation=""){
 if(!client||!connected||esp_mqtt_client_get_outbox_size(client)>16384)return false;
 String topic;MqttContract::topic(profile.as<JsonObjectConst>(),"config",topic);JsonDocument d;d["schema_version"]=1;d["message_id"]=device+":"+String(esp_random(),HEX);d["gateway_id"]=profile["gateway_id"];d["master_id"]=device;d["device_id"]=device;d["message_type"]="config";d["timestamp"]=time(nullptr);d["expires_at"]=time(nullptr)+120;if(correlation.length())d["correlation_id"]=correlation;d["data"]=data;
 String raw;serializeJson(d,raw);if(d.overflowed()||raw.length()>6144)return false;return esp_mqtt_client_enqueue(client,topic.c_str(),raw.c_str(),raw.length(),1,0,true)>=0;
}
void emit(const String& raw){JsonDocument d;if(deserializeJson(d,raw))return;d["transaction_id"]=transaction;String enriched;serializeJson(d,enriched);Control* c=new(std::nothrow) Control{};if(!c)return;strlcpy(c->raw,enriched.c_str(),sizeof(c->raw));if(xQueueSend(queue,&c,0)!=pdTRUE)delete c;}
void event(void*,esp_event_base_t,int32_t,void* raw){auto e=(esp_mqtt_event_handle_t)raw;
 if(e->event_id==MQTT_EVENT_CONNECTED){connected=true;String topic;MqttContract::topic(profile.as<JsonObjectConst>(),"config",topic);esp_mqtt_client_subscribe(client,topic.c_str(),1);}
 else if(e->event_id==MQTT_EVENT_SUBSCRIBED)subscribed=true;
 else if(e->event_id==MQTT_EVENT_PUBLISHED)puback=true;
 else if(e->event_id==MQTT_EVENT_DISCONNECTED)connected=false;
 else if(e->event_id==MQTT_EVENT_ERROR){tlsError=e->error_handle?e->error_handle->esp_tls_stack_err:0;failureCode=e->error_handle?classifyMqttFailure(e->error_handle->error_type==MQTT_ERROR_TYPE_CONNECTION_REFUSED,e->error_handle->connect_return_code,tlsError.load(),e->error_handle->esp_tls_cert_verify_flags):3;stop=true;}
 else if(e->event_id==MQTT_EVENT_DATA){
  if(e->current_data_offset==0){incoming="";receivedTopic="";if(e->topic_len>0&&e->topic_len<192)receivedTopic=String(e->topic,e->topic_len);}
  if(e->total_data_len>6144||e->data_len<0||e->current_data_offset!=(int)incoming.length()||e->qos!=1||e->retain)return;
  incoming.concat(e->data,e->data_len);if(incoming.length()!=(size_t)e->total_data_len)return;
  String topic;MqttContract::topic(profile.as<JsonObjectConst>(),"config",topic);if(topic!=receivedTopic||!StrictJson::valid(incoming))return;
  JsonDocument d;if(!MqttWire::incoming(incoming,receivedTopic,profile.as<JsonObjectConst>(),device,d)||d["message_type"].as<String>()!="config")return;
  auto data=d["data"].as<JsonObjectConst>();auto now=time(nullptr);uint32_t stamp=d["timestamp"]|0,expiry=d["expires_at"]|0;
  if(!stamp||stamp>now+30||expiry<now||expiry<=stamp||expiry>stamp+300||data["transaction_id"].as<String>()!=transaction||data["participant"].as<String>()!="GATEWAY")return;
  String op=data["op"]|"";
  if(op=="PROBE"&&data.size()==4&&data["nonce"].as<String>().length()==32){JsonDocument reply;reply.set(data);reply["op"]="PROBE_ACK";reply["participant"]="MASTER";send(reply.as<JsonObjectConst>(),d["message_id"].as<String>());}
  else if(op=="PROBE_ACK"&&data.size()==4&&data["nonce"].as<String>()==nonce){peerProof=true;emit("{\"op\":\"LOCAL_PEER_PROBE\"}");}
  else if((op=="SWITCH_CONFIRM"||op=="SWITCH_ROLLBACK")&&data.size()==5&&data["digest"].as<String>()==digest&&data["shared_revision"].as<uint32_t>()==revision) {String s;serializeJson(data,s);emit(s);}
 }
}
void worker(void*){
 esp_mqtt_client_config_t cfg={};cfg.host=profile["host"];cfg.port=profile["port"];String id=profile["client_id"].as<String>().substring(0,40)+":test:"+transaction.substring(0,8);cfg.client_id=id.c_str();cfg.username=profile["username"];cfg.password=profile["password"];cfg.cert_pem=profile["ca_cert"];cfg.transport=MQTT_TRANSPORT_OVER_SSL;cfg.protocol_ver=MQTT_PROTOCOL_V_3_1_1;cfg.keepalive=profile["keepalive_s"];cfg.network_timeout_ms=profile["timeout_s"].as<int>()*1000;cfg.disable_auto_reconnect=true;cfg.task_stack=6144;cfg.buffer_size=2048;
 client=esp_mqtt_client_init(&cfg);if(client){esp_mqtt_client_register_event(client,MQTT_EVENT_ANY,event,nullptr);esp_mqtt_client_start(client);}
 unsigned attempts=0;uint32_t last=millis()-5000;bool announced=false;
 while(client&&!stop&&time(nullptr)<=deadline){
  if(subscribed&&connected&&attempts<3&&millis()-last>=5000){last=millis();++attempts;JsonDocument d;d["op"]="PROBE";d["transaction_id"]=transaction;d["nonce"]=nonce;d["participant"]="MASTER";send(d.as<JsonObjectConst>());}
  if(subscribed&&connected&&puback&&peerProof&&!announced){announced=true;emit("{\"op\":\"LOCAL_BROKER_READY\"}");}
  if(readyPending){JsonDocument d;d["op"]="SWITCH_READY";d["transaction_id"]=transaction;d["digest"]=digest;d["shared_revision"]=revision;d["participant"]="MASTER";if(send(d.as<JsonObjectConst>()))readyPending=false;}
  if(commit){JsonDocument d;d["op"]="SWITCH_COMMITTED";d["transaction_id"]=transaction;d["digest"]=digest;d["shared_revision"]=revision;d["participant"]="MASTER";send(d.as<JsonObjectConst>());commit=false;}
  vTaskDelay(pdMS_TO_TICKS(100));
 }
 if(client){esp_mqtt_client_stop(client);esp_mqtt_client_destroy(client);client=nullptr;}if(failureCode){JsonDocument d;d["op"]="LOCAL_BROKER_FAILED";d["reason"]=failureCode==2?"AUTH_FAILED":failureCode==1?"TLS_FAILED":"CONNECTION_FAILED";String raw;serializeJson(d,raw);emit(raw);}running=false;vTaskDelete(nullptr);
}
}
bool start(const String& raw,const String& tx,const String& hash,uint32_t sharedRevision,uint32_t until){
 if(running||time(nullptr)<1700000000)return false;String field;if(deserializeJson(profile,raw)||!MqttContract::validate(profile.as<JsonObjectConst>(),field,true))return false;
 if(!queue)queue=xQueueCreate(6,sizeof(Control*));if(!queue)return false;Control* stale=nullptr;while(xQueueReceive(queue,&stale,0)==pdTRUE)delete stale;transaction=tx;digest=hash;revision=sharedRevision;deadline=until;device=configManager.get().device_id;nonce="";for(int i=0;i<4;i++){char b[9];snprintf(b,9,"%08x",esp_random());nonce+=b;}
 failureCode=0;tlsError=0;stop=false;commit=false;readyPending=false;connected=false;subscribed=false;puback=false;peerProof=false;running=true;
 if(xTaskCreate(worker,"mqtt-candidate",6144,nullptr,1,nullptr)!=pdPASS){running=false;return false;}return true;
}
String status(){JsonDocument d;d["running"]=running.load();d["connected"]=connected.load();d["subscribed"]=subscribed.load();d["puback"]=puback.load();d["peer_probe"]=peerProof.load();d["tls_error"]=tlsError.load();d["failure_code"]=failureCode.load();String raw;serializeJson(d,raw);return raw;}
void cancel(){stop=true;}
void committed(){commit=true;}
void ready(){readyPending=true;}
bool readyFor(const String& tx){return running&&connected&&subscribed&&puback&&peerProof&&tx==transaction;}
bool receive(String& raw){Control* c=nullptr;if(!queue||xQueueReceive(queue,&c,0)!=pdTRUE)return false;raw=c->raw;delete c;return true;}
}
