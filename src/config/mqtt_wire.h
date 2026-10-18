#pragma once
#include "mqtt_contract.h"
#include "strict_json.h"
#include <time.h>
namespace MqttWire {
inline bool id(JsonVariantConst v,size_t max=48){if(!v.is<const char*>())return false;auto text=v.as<JsonString>();if(text.size()==0||text.size()>max||strlen(text.c_str())!=text.size())return false;for(size_t i=0;i<text.size();i++){char c=text.c_str()[i];if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='.'||c==':'||c=='-'))return false;}return true;}
inline bool number(JsonVariantConst v){return !v.is<bool>()&&v.is<uint32_t>();}
inline bool hex(JsonVariantConst v,size_t n){if(!v.is<const char*>()||v.as<JsonString>().size()!=n)return false;for(size_t i=0;i<n;i++){char c=v.as<const char*>()[i];if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;}return true;}
inline bool uuid(JsonVariantConst v){if(!v.is<const char*>()||v.as<JsonString>().size()!=36)return false;String s=v.as<String>();if(s[14]!='4'||String("89ab").indexOf(s[19])<0)return false;for(size_t i=0;i<36;i++){if(i==8||i==13||i==18||i==23){if(s[i]!='-')return false;}else if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return false;}return true;}
inline bool control(JsonObjectConst d){
 String op=d["op"]|"";
 if(op=="PROBE"||op=="PROBE_ACK")return d.size()==4&&uuid(d["transaction_id"])&&hex(d["nonce"],32)&&(d["participant"].as<String>()=="MASTER"||d["participant"].as<String>()=="GATEWAY");
 if(op=="SWITCH_PREPARE"||op=="SWITCH_READY"||op=="SWITCH_CONFIRM"||op=="SWITCH_COMMITTED"||op=="SWITCH_ROLLBACK")return d.size()==(op=="SWITCH_PREPARE"?6:5)&&uuid(d["transaction_id"])&&hex(d["digest"],64)&&number(d["shared_revision"])&&(d["participant"].as<String>()=="MASTER"||d["participant"].as<String>()=="GATEWAY")&&(op!="SWITCH_PREPARE"||number(d["deadline"]));
 if(op=="PERSISTED")return d.size()==3&&id(d["received_message_id"])&&d["status"].as<String>()=="PERSISTED";
 if(op=="COMPARE")return d.size()==3&&d["snapshot"].is<JsonObject>()&&hex(d["digest"],64);
 return false;
}
inline bool incoming(const String& raw,const String& topic,JsonObjectConst profile,const String& master,JsonDocument& d){
 if(!StrictJson::valid(raw)||deserializeJson(d,raw)||!d.is<JsonObject>())return false;
 for(JsonPair p:d.as<JsonObject>()){String k=p.key().c_str();if(k!="schema_version"&&k!="message_id"&&k!="gateway_id"&&k!="master_id"&&k!="device_id"&&k!="message_type"&&k!="timestamp"&&k!="data"&&k!="correlation_id"&&k!="configuration_revision"&&k!="expires_at")return false;}
 if(!number(d["schema_version"])||d["schema_version"].as<uint32_t>()!=1||!id(d["message_id"])||d["gateway_id"]!=profile["gateway_id"]||d["master_id"].as<String>()!=master||d["device_id"].as<String>()!="GATEWAY"||!d["data"].is<JsonObject>())return false;
 if(!d["correlation_id"].isNull()&&!id(d["correlation_id"]))return false;if(!d["configuration_revision"].isNull()&&!number(d["configuration_revision"]))return false;
 String expected;if(!MqttContract::topic(profile,d["message_type"]|"",expected)||topic!=expected)return false;
 auto now=time(nullptr);if(now<1700000000||!number(d["timestamp"])||!number(d["expires_at"]))return false;
 uint32_t stamp=d["timestamp"],expires=d["expires_at"];if(!stamp||stamp>now+30||expires<=stamp||uint64_t(expires)>uint64_t(stamp)+300||expires<now)return false;
 return d["message_type"].as<String>()!="config"||control(d["data"]);
}
}
