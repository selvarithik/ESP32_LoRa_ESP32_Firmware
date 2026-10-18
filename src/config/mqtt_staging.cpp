#include "mqtt_staging.h"
#include "mqtt_contract.h"
#include "strict_json.h"
#include "mqtt_digest.h"
#include "../communication/mqtt_candidate.h"
#include <time.h>
#include "../core/config_manager.h"
#include <Preferences.h>
#include <esp_system.h>
#include <memory>
#include <new>
#include "../communication/canonical_mqtt.h"


MqttStaging mqttStaging;
namespace {
struct Control {char raw[1025];};QueueHandle_t controls=nullptr;uint32_t restartAt=0;
int error(int status,const char* code,const char* message,String& response,const String& field=""){
    JsonDocument d;d["ok"]=false;d["error"]["code"]=code;d["error"]["message"]=message;
    if(field.length())d["error"]["field"]=field;response="";serializeJson(d,response);return status;
}
String uuid(){uint8_t bytes[16];esp_fill_random(bytes,sizeof(bytes));bytes[6]=(bytes[6]&15)|64;bytes[8]=(bytes[8]&63)|128;
    char id[37];snprintf(id,sizeof(id),"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",bytes[0],bytes[1],bytes[2],bytes[3],bytes[4],bytes[5],bytes[6],bytes[7],bytes[8],bytes[9],bytes[10],bytes[11],bytes[12],bytes[13],bytes[14],bytes[15]);return id;}
}
void MqttStaging::begin(){
    controls=xQueueCreate(4,sizeof(Control*));
    state.clear();state["schema_version"]=1;state["revision"]=0;state["transaction"]["state"]="IDLE";
    Preferences p;if(!p.begin("device",false))return;ready=true;
    String raw;
    if(p.isKey("mqtt_blob_v1")){
        size_t size=p.getBytesLength("mqtt_blob_v1");
        if(size==0||size>24576){p.end();ready=false;state["transaction"]["state"]="RECOVERY_REQUIRED";return;}
        std::unique_ptr<char[]> bytes(new(std::nothrow) char[size+1]);
        if(!bytes||p.getBytes("mqtt_blob_v1",bytes.get(),size)!=size){p.end();ready=false;state["transaction"]["state"]="RECOVERY_REQUIRED";return;}
        bytes[size]=0;raw=bytes.get();
    }else if(p.isKey("mqtt_v1"))raw=p.getString("mqtt_v1");
    p.end();
    if(raw.length()){
        JsonDocument saved;
        if(raw.length()>24576||deserializeJson(saved,raw)||(!saved["schema_version"].is<uint32_t>()||saved["schema_version"].is<bool>()||saved["schema_version"].as<uint32_t>()!=1)||!saved["revision"].is<uint32_t>()){
            ready=false;state["transaction"]["state"]="RECOVERY_REQUIRED";return;
        }
        state["revision"]=saved["revision"];state["shared_revision"]=saved["shared_revision"]|0U;
        for(auto name:{"active","pending","previous"})if(saved[name].is<JsonObject>()){String field;if(!MqttContract::validate(saved[name],field)){ready=false;state["transaction"]["state"]="RECOVERY_REQUIRED";state["transaction"]["reason"]=String("INVALID_PROFILE:")+name+"."+field;return;}}
        swap(state,saved);
        // An explicit repair keeps quarantine until the restored journal passes a cold read.
        if(state["transaction"]["state"].as<String>()=="IDLE"&&state["transaction"]["reason"].as<String>().startsWith("INVALID_PROFILE:")){
            Preferences cleanup;if(cleanup.begin("device",false)){if(cleanup.isKey("mqtt_bad_v1"))cleanup.remove("mqtt_bad_v1");cleanup.end();}
            state["transaction"]["reason"]="RECOVERED";save(state);
        }
        // Preserve the active profile; interrupted candidate work requires explicit recovery.
        if(!state["pending"].isNull()&&state["transaction"]["state"].as<String>()!="PENDING"&&state["transaction"]["state"].as<String>()!="RECOVERY_REQUIRED"){state["transaction"]["state"]="RECOVERY_REQUIRED";state["revision"]=(state["revision"]|0U)+1;if(!save(state))ready=false;}
    }else{
        const auto& legacy=configManager.get();
        if(legacy.mqtt_host.length()){
            JsonDocument profile;MqttContract::defaults(profile.to<JsonObject>());
            profile["host"]=legacy.mqtt_host;profile["port"]=legacy.mqtt_port;profile["tls"]=legacy.mqtt_tls;profile["ca_cert"]=legacy.mqtt_ca;profile["username"]=legacy.mqtt_user;profile["password"]=legacy.mqtt_password;profile["keepalive_s"]=legacy.mqtt_keepalive;profile["client_id"]=legacy.device_id;
            String field;if(MqttContract::validate(profile.as<JsonObjectConst>(),field)){state["pending"]=profile;state["transaction"]["transaction_id"]=uuid();state["transaction"]["state"]="PENDING";state["transaction"]["deadline"]=nullptr;}
            state["transaction"]["reason"]="MIGRATION_REQUIRED";state["revision"]=1;save(state);
        }
    }
}
bool MqttStaging::save(const JsonDocument& candidate){
    if(!ready||candidate.overflowed())return false;
    if(candidate["schema_version"].is<bool>()||!candidate["schema_version"].is<uint32_t>()||candidate["schema_version"].as<uint32_t>()!=1||candidate["revision"].is<bool>()||!candidate["revision"].is<uint32_t>()||candidate["revision"].as<uint32_t>()<state["revision"].as<uint32_t>())return false;
    if(state["active"].is<JsonObject>()&&!candidate["active"].is<JsonObjectConst>())return false;
    size_t size=measureJson(candidate);if(size>24576)return false;
    String raw;if(!raw.reserve(size))return false;serializeJson(candidate,raw);if(raw.length()!=size)return false;
    Preferences p;if(!p.begin("device",false))return false;
    // Blobs span NVS pages; strings are limited to roughly one page.
    bool ok=p.putBytes("mqtt_blob_v1",raw.c_str(),raw.length())==raw.length();
    if(ok&&p.isKey("mqtt_v1"))p.remove("mqtt_v1"); // Retire legacy only after the new journal is durable.
    p.end();return ok;
}
String MqttStaging::status(bool includeProfiles) const {
    JsonDocument out;
    for(JsonPairConst p:state.as<JsonObjectConst>()){
        String name=p.key().c_str();if(name!="active"&&name!="pending"&&name!="previous"&&name!="last_commit")out[name]=p.value();
    }
    if(includeProfiles){for(auto name:{"active","pending","previous"})if(!state[name].isNull())out[name]=state[name];}
    else {
        JsonDocument active;active.set(state["active"]);active.remove("password");String raw;size_t size=measureJson(active);
        bool complete=!active.overflowed()&&raw.reserve(size);if(complete)serializeJson(active,raw);
        if(complete&&raw.length()==size)out["active_fingerprint"]=MqttDigest::hash(raw);else out["active_fingerprint"]=nullptr;out["active_client_id"]=state["active"]["client_id"];out["active_configured"]=!state["active"].isNull();out["pending_configured"]=!state["pending"].isNull();
    }
    for(auto name:{"active","pending","previous"}){auto profile=out[name].as<JsonObject>();if(!profile.isNull()){profile.remove("password");}}
    if(includeProfiles){MqttContract::defaults(out["defaults"].to<JsonObject>());out["defaults"].remove("password");}
    out["comparison"]["status"]="UNKNOWN";
    out["comparison"]["message"]="No verified Gateway snapshot received";
    JsonDocument runtime;deserializeJson(runtime,CanonicalMqtt::status());out["runtime"]=runtime;JsonDocument candidate;deserializeJson(candidate,MqttCandidate::status());out["candidate_runtime"]=candidate;out["comparison"]=runtime["comparison"];
    out["capabilities"]["protected_provisioning"]=true;
    out["capabilities"]["connection_test"]=true;
    out["capabilities"]["coordinated_commit"]=true;
    out["active_source"]=state["active"].isNull()?"MIGRATION_REQUIRED":"physical commissioning";
    out["secret_state"]["password_configured"]=state["active"]["password"].is<const char*>()&&strlen(state["active"]["password"].as<const char*>())>0;
    out["storage_protection"]="NVS encryption not configured; chip security unverified";
    if(out.overflowed())return "{\"ok\":false,\"error\":{\"code\":\"RESOURCE_LOW\",\"message\":\"Profile status temporarily unavailable\"}}";
    String result;if(!result.reserve(measureJson(out)))return "{\"ok\":false,\"error\":{\"code\":\"RESOURCE_LOW\",\"message\":\"Reload profile status when resources recover\"}}";
    serializeJson(out,result);return result;
}
int MqttStaging::action(const String& op,const String& raw,String& response){
    JsonDocument req;
    if(!StrictJson::valid(raw,MqttContract::maxRequestBytes)||deserializeJson(req,raw)||!req.is<JsonObject>())return error(400,"INVALID_REQUEST","JSON object required",response);
    for(JsonPair entry:req.as<JsonObject>()){
        String key=entry.key().c_str();
        if(key!="expected_revision"&&!(op=="candidate"&&key=="profile")&&!(op=="cancel"&&key=="transaction_id"))return error(400,"INVALID_REQUEST","Unknown request field",response);
    }
    if(!req["expected_revision"].is<uint32_t>()||req["expected_revision"].is<bool>()||req["expected_revision"].as<uint32_t>()!=state["revision"].as<uint32_t>())return error(409,"REVISION_CONFLICT","Reload the current revision",response);
    if(!ready)return error(503,"RECOVERY_REQUIRED","Configuration storage is unavailable",response);
    if(op=="confirm")return error(409,"COORDINATION_REQUIRED","Confirm through the Gateway coordinator; active profile unchanged",response);
    if(op=="test"){
        if(state["shared_revision"].as<uint32_t>()==UINT32_MAX)return error(409,"REVISION_CONFLICT","Shared revision exhausted",response);
        if(state["transaction"]["state"].as<String>()!="PENDING")return error(409,"TRANSACTION_CONFLICT","Stage a candidate first",response);
        if(time(nullptr)<1700000000)return error(409,"CLOCK_REQUIRED","Synchronize clock before testing",response);
        JsonDocument snapshot;if(!MqttDigest::snapshot(state["pending"],(state["shared_revision"]|0U)+1,snapshot))return error(400,"INVALID_PROFILE","Invalid candidate",response);
        String pending;serializeJson(state["pending"],pending);auto until=time(nullptr)+120;
        JsonDocument next;next.set(state);next["transaction"]["state"]="TESTING";next["transaction"]["deadline"]=until;next["transaction"]["test_deadline"]=time(nullptr)+60;next["transaction"]["digest"]=MqttDigest::digest(snapshot);next["revision"]=state["revision"].as<uint32_t>()+1;
        if(!save(next))return error(503,"STORAGE_FAILED","Test not started",response);swap(state,next);
        if(!MqttCandidate::start(pending,state["transaction"]["transaction_id"],state["transaction"]["digest"],(state["shared_revision"]|0U)+1,until)){state["transaction"]["state"]="FAILED";state["transaction"]["reason"]="PROTECTED_PROVISIONING_REQUIRED";save(state);}
        response=status();return 202;
    }
    if(op!="candidate"&&op!="cancel")return error(404,"NOT_FOUND","Unknown operation",response);
    uint32_t revision=state["revision"]|0U;
    if(revision==UINT32_MAX)return error(409,"REVISION_CONFLICT","Revision exhausted",response);
    JsonDocument next;next.set(state);
    if(op=="candidate"){
        if(!state["pending"].isNull())return error(409,"TRANSACTION_BUSY","Cancel the existing candidate before staging another",response);
        if(!req["profile"].is<JsonObject>())return error(400,"INVALID_PROFILE","Complete profile required",response,"profile");
        if(!MqttContract::isPreservedSecret(req["profile"]["password"]))return error(403,"PROTECTED_PROVISIONING_REQUIRED","Do not submit connection passwords over this HTTP interface",response,"password");
        JsonDocument profile;profile.set(req["profile"]);profile["password"]=state["active"]["password"]|"";
        String field;if(!MqttContract::validate(profile.as<JsonObjectConst>(),field))return error(400,"INVALID_PROFILE","Invalid or missing profile field",response,field);
        next["pending"]=profile;
        if(state["transaction"]["state"].as<String>()=="COMMITTED"&&state["transaction"]["decision"].as<String>()=="COMMIT"){
            next["last_commit"]["transaction_id"]=state["transaction"]["transaction_id"];
            next["last_commit"]["digest"]=state["transaction"]["digest"];
            next["last_commit"]["shared_revision"]=state["shared_revision"]|0U;
        }
        // COMMIT is irreversible. Its retired rollback profile is not part of a
        // new draft and need not consume a third certificate-sized NVS copy.
        if(!next["last_commit"].isNull())next.remove("previous");
        next.remove("transaction"); // A new draft must not inherit an older COMMIT decision/proofs.
        next["transaction"]["transaction_id"]=uuid();next["transaction"]["state"]="PENDING";
        next["transaction"]["reason"]="PROTECTED_PROVISIONING_REQUIRED";
        next["transaction"]["deadline"]=nullptr; // No network transaction started.
    }else{
        if(state["pending"].isNull()){response=status();return 200;}
        if(!req["transaction_id"].is<const char*>()||req["transaction_id"].as<String>()!=state["transaction"]["transaction_id"].as<String>())return error(409,"TRANSACTION_CONFLICT","Exact transaction ID required",response);
        if(state["transaction"]["decision"].as<String>()=="COMMIT")return error(409,"RECOVERY_REQUIRED","Committed decision cannot be cancelled",response);
        MqttCandidate::cancel();next.remove("pending");next["transaction"]["state"]="ROLLED_BACK";next["transaction"].remove("reason");
    }
    next["revision"]=revision+1;
    if(!save(next))return error(503,"STORAGE_FAILED","Candidate was not saved; active profile unchanged",response);
    swap(state,next);response=status();return 200;
}

String MqttStaging::active() const {String raw;serializeJson(state["active"],raw);return raw;}
int MqttStaging::provision(const String& raw,String& response,bool restoreInvalid){
    JsonDocument req;if(!StrictJson::valid(raw,8192)||deserializeJson(req,raw)||!req.is<JsonObject>()||req.size()!=2||!req["expected_revision"].is<uint32_t>())return error(400,"INVALID_REQUEST","Invalid commissioning request",response);
    if(req["expected_revision"].as<uint32_t>()!=state["revision"].as<uint32_t>())return error(409,"REVISION_CONFLICT","Reload revision",response);
    String field;if(!MqttContract::validate(req["profile"].as<JsonObjectConst>(),field,true))return error(400,"INVALID_PROFILE","Invalid profile",response,field);
    if(state["revision"].as<uint32_t>()==UINT32_MAX)return error(409,"REVISION_CONFLICT","Revision exhausted",response);
    if(restoreInvalid){
        if(ready||state["transaction"]["state"].as<String>()!="RECOVERY_REQUIRED")return error(409,"REPAIR_NOT_REQUIRED","Preserve the existing configuration",response);
        Preferences p;if(!p.begin("device",false))return error(503,"STORAGE_FAILED","Storage unavailable",response);
        size_t size=p.getBytesLength("mqtt_blob_v1");std::unique_ptr<uint8_t[]> backup(size?new(std::nothrow) uint8_t[size]:nullptr);
        bool backed=size&&backup&&p.getBytes("mqtt_blob_v1",backup.get(),size)==size&&p.putBytes("mqtt_bad_v1",backup.get(),size)==size;p.end();
        if(!backed)return error(503,"STORAGE_FAILED","Recovery backup could not be retained",response);
        ready=true;
    }
    JsonDocument next;next.set(state);
    // Explicit local first commissioning is distinct from replacing an active broker.
    if(state["active"].isNull()){next["active"]=req["profile"];next["transaction"]["state"]="IDLE";}
    else {if(state["pending"].isNull()||state["transaction"]["state"].as<String>()!="PENDING")return error(409,"STAGED_SWITCH_REQUIRED","Stage a candidate first",response);JsonDocument check;check.set(req["profile"]);check["password"]=state["pending"]["password"];if(check.as<JsonVariantConst>()!=state["pending"].as<JsonVariantConst>())return error(409,"REVISION_CONFLICT","Only the staged password may be provisioned",response);next["pending"]=req["profile"];}
    next["revision"]=state["revision"].as<uint32_t>()+1;next["secret_revision"]=(state["secret_revision"]|0U)+1;
    if(!save(next))return error(503,"STORAGE_FAILED","No change applied",response);swap(state,next);response=status();return 200;
}

void MqttStaging::control(const String& raw){if(!controls||raw.length()>1024)return;Control* c=new(std::nothrow) Control{};if(!c)return;strlcpy(c->raw,raw.c_str(),sizeof(c->raw));if(xQueueSend(controls,&c,0)!=pdTRUE)delete c;}
void MqttStaging::loop(){
    if(restartAt&&(int32_t)(millis()-restartAt)>=0)ESP.restart();
    String raw;Control* c=nullptr;if(controls&&xQueueReceive(controls,&c,0)==pdTRUE){raw=c->raw;delete c;}else MqttCandidate::receive(raw);
    if(raw.length()){
        JsonDocument d;if(!deserializeJson(d,raw)){
            String op=d["op"]|"";auto t=state["transaction"].as<JsonObjectConst>();JsonDocument next;next.set(state);bool changed=false;
            if(op=="SWITCH_PREPARE"&&state["shared_revision"].as<uint32_t>()<UINT32_MAX&&d["participant"].as<String>()=="GATEWAY"&&d["transaction_id"].as<String>().length()==36&&d["deadline"].is<uint32_t>()&&time(nullptr)>1700000000&&d["deadline"].as<uint32_t>()>time(nullptr)&&d["deadline"].as<uint32_t>()<=time(nullptr)+120&&!state["pending"].isNull()&&t["state"].as<String>()=="PENDING"){
                JsonDocument snapshot;uint32_t revision=(state["shared_revision"]|0U)+1;
                if(d["shared_revision"].as<uint32_t>()==revision&&MqttDigest::snapshot(state["pending"],revision,snapshot)&&MqttDigest::digest(snapshot)==d["digest"].as<String>()){
                    next["transaction"]["transaction_id"]=d["transaction_id"];next["transaction"]["digest"]=d["digest"];next["transaction"]["deadline"]=d["deadline"];next["transaction"]["test_deadline"]=time(nullptr)+60;next["transaction"]["state"]="TESTING";next["revision"]=(state["revision"]|0U)+1;
                    if(save(next)){swap(state,next);String pending;serializeJson(state["pending"],pending);if(!MqttCandidate::start(pending,d["transaction_id"],d["digest"],revision,d["deadline"])){state["transaction"]["state"]="FAILED";state["transaction"]["reason"]="CONNECTION_FAILED";save(state);}}
                }
            }else if(op=="SWITCH_CONFIRM"&&d["participant"].as<String>()=="GATEWAY"&&d["shared_revision"].as<uint32_t>()==(state["shared_revision"]|0U)&&((d["transaction_id"]==t["transaction_id"]&&d["digest"]==t["digest"]&&t["decision"].as<String>()=="COMMIT")||(d["transaction_id"]==state["last_commit"]["transaction_id"]&&d["digest"]==state["last_commit"]["digest"]&&state["last_commit"]["shared_revision"].as<uint32_t>()==(state["shared_revision"]|0U)))){JsonDocument reply;reply.set(d);reply["op"]="SWITCH_COMMITTED";reply["participant"]="MASTER";String raw;serializeJson(reply,raw);CanonicalMqtt::controlReply(raw);}
            else if(op=="LOCAL_BROKER_FAILED"&&d["transaction_id"]==t["transaction_id"]&&(t["state"].as<String>()=="TESTING"||t["state"].as<String>()=="READY_TO_CONFIRM")&&t["decision"].as<String>()!="COMMIT"){next["transaction"]["state"]="FAILED";next["transaction"]["reason"]=d["reason"];changed=true;}
            else if(op=="LOCAL_PEER_PROBE"&&d["transaction_id"]==t["transaction_id"]&&t["state"].as<String>()=="TESTING"){next["transaction"]["proofs"]["peer_probe"]=true;changed=true;}
            else if(op=="LOCAL_BROKER_READY"&&d["transaction_id"]==t["transaction_id"]&&t["proofs"]["peer_probe"].as<bool>()&&t["state"].as<String>()=="TESTING"){next["transaction"]["proofs"]["broker_ready"]=true;next["transaction"]["state"]="READY_TO_CONFIRM";changed=true;}
            else if(op=="SWITCH_CONFIRM"&&d["participant"].as<String>()=="GATEWAY"&&d["shared_revision"].as<uint32_t>()==(state["shared_revision"]|0U)+1&&d["transaction_id"]==t["transaction_id"]&&d["digest"]==t["digest"]&&t["state"].as<String>()=="READY_TO_CONFIRM"&&time(nullptr)<=t["deadline"].as<uint32_t>()){
                next["previous"]=state["active"];next["active"]=state["pending"];next.remove("pending");next["shared_revision"]=(state["shared_revision"]|0U)+1;next["transaction"]["state"]="COMMITTED";next["transaction"]["decision"]="COMMIT";changed=true;
            }else if(op=="SWITCH_ROLLBACK"&&d["participant"].as<String>()=="GATEWAY"&&d["shared_revision"].as<uint32_t>()==(state["shared_revision"]|0U)+1&&d["digest"]==t["digest"]&&!state["pending"].isNull()&&d["transaction_id"]==t["transaction_id"]&&t["decision"].as<String>()!="COMMIT"){next.remove("pending");next["transaction"]["state"]="ROLLED_BACK";MqttCandidate::cancel();changed=true;}
            if(changed){next["revision"]=(state["revision"]|0U)+1;if(save(next)){swap(state,next);if(op=="LOCAL_BROKER_READY")MqttCandidate::ready();if(op=="SWITCH_CONFIRM"){MqttCandidate::committed();restartAt=millis()+3000;}}}
        }
    }
    auto t=state["transaction"].as<JsonObjectConst>();String phase=t["state"]|"";
    // Proof callbacks can race or a low-memory journal attempt can fail. Retry the
    // durable transition from the verified candidate state, never from a UI claim.
    static uint32_t lastReadyWrite=0;
    if(phase=="TESTING"&&MqttCandidate::readyFor(t["transaction_id"]|"")&&millis()-lastReadyWrite>=250){
        lastReadyWrite=millis();JsonDocument next;next.set(state);next["transaction"]["proofs"]["peer_probe"]=true;next["transaction"]["proofs"]["broker_ready"]=true;next["transaction"]["state"]="READY_TO_CONFIRM";next["revision"]=(state["revision"]|0U)+1;
        if(save(next)){swap(state,next);MqttCandidate::ready();return;}
    }
    if((phase=="TESTING"||phase=="READY_TO_CONFIRM")&&time(nullptr)>1700000000&&time(nullptr)>(phase=="TESTING"?t["test_deadline"].as<uint32_t>():t["deadline"].as<uint32_t>())){
        JsonDocument next;next.set(state);next["transaction"]["state"]="ROLLED_BACK";next["transaction"]["reason"]="TIMEOUT";next.remove("pending");next["revision"]=(state["revision"]|0U)+1;if(save(next)){swap(state,next);MqttCandidate::cancel();}
    }
}

uint32_t MqttStaging::sharedRevision() const {return state["shared_revision"]|0U;}
