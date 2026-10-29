#ifdef TRANSPORT_TRIAL_HOST_TEST
#include "../../test/transport_trial_host_port.h"
#else
#include "transport_trial.h"
#include "mqtt_staging.h"
#include "../core/config_manager.h"
#include "../communication/transport_manager.h"
#include <Preferences.h>
#endif
#include "transport_trial_core.h"
#include <time.h>
namespace TransportSwitch {
namespace {
using TransportTrial::Phase;Phase phase=Phase::IDLE;String tx;bool oldMode=false,target=false,storage=false,applied=false,pingSeen=false;
uint32_t deadline=0,applyAt=0,receiptAt=0,trialStarted=0;unsigned timeout=90;
bool save(){Preferences p;if(!p.begin("transport_tx",false))return false;JsonDocument d;d["id"]=tx;d["phase"]=TransportTrial::name(phase);d["old"]=oldMode;d["target"]=target;d["deadline"]=deadline;d["timeout"]=timeout;String raw;serializeJson(d,raw);bool ok=p.putString("journal",raw)==raw.length();p.end();return ok;}
bool restore(){transportManager.requestLocalMode(oldMode);if(configManager.get().local_ip_enabled!=oldMode){JsonDocument d;d["local_ip_enabled"]=oldMode;String raw,error;serializeJson(d,raw);if(!configManager.update(raw,error))return false;}phase=Phase::ROLLED_BACK;applied=false;return save();}
bool fail(String& result,const char* error){JsonDocument d;d["error"]=error;serializeJson(d,result);return false;}
}
bool ready(){return storage;}
bool busy(){return !storage||TransportTrial::busy(phase);}
void begin(){phase=Phase::IDLE;storage=false;applied=false;pingSeen=false;tx="";Preferences p;if(!p.begin("transport_tx",false))return;String raw=p.isKey("journal")?p.getString("journal"):"";p.end();storage=true;if(raw.isEmpty())return;JsonDocument d;
 if(deserializeJson(d,raw)||!d["old"].is<bool>()||!d["target"].is<bool>()||!d["deadline"].is<uint32_t>()||!TransportTrial::id(d["id"]|"")){storage=false;return;}
 tx=d["id"].as<String>();oldMode=d["old"];target=d["target"];deadline=d["deadline"];timeout=d["timeout"]|90;String s=d["phase"]|"";
 if(s=="TRIAL"){phase=Phase::TRIAL;storage=restore();}else if(s=="PREPARED"){phase=Phase::CANCELLED;storage=save();}else if(s=="COMMITTED")phase=Phase::COMMITTED;else if(s=="CANCELLED")phase=Phase::CANCELLED;else if(s=="ROLLED_BACK")phase=Phase::ROLLED_BACK;else storage=false;
}
String status(){JsonDocument d;d["transaction_id"]=tx;d["state"]=storage?TransportTrial::name(phase):"RECOVERY_REQUIRED";d["requested_transport"]=target?"IP":"MQTT";d["active_transport"]=transportManager.localMode()?"IP":"MQTT";d["deadline"]=deadline;d["ping_verified"]=pingSeen;d["configuration_revision"]=configManager.get().configuration_revision;String raw;serializeJson(d,raw);return raw;}
bool action(JsonObjectConst args,bool gateway,String& result){
 if(!gateway||configManager.get().role!=DeviceRole::MASTER)return fail(result,"GATEWAY_MASTER_REQUIRED");
 if(!storage)return fail(result,"TRANSPORT_RECOVERY_REQUIRED");loop();
 String operation=args["transport_action"]|"",id=args["transaction_id"]|"";
 if(!TransportTrial::id(id.c_str())||!args["expected_revision"].is<uint32_t>()||args["expected_revision"].is<bool>()||args["expected_revision"].as<uint32_t>()!=configManager.get().configuration_revision)return fail(result,"REVISION_OR_TRANSACTION_INVALID");
 if(operation=="prepare"){
  if(args.size()!=5||!args["local_ip_enabled"].is<bool>()||!args["timeout_seconds"].is<unsigned>()||args["timeout_seconds"].is<bool>()||args["timeout_seconds"].as<unsigned>()<30||args["timeout_seconds"].as<unsigned>()>180)return fail(result,"INVALID_TRANSPORT_REQUEST");
  bool desired=args["local_ip_enabled"];unsigned seconds=args["timeout_seconds"];
  if(tx==id){if(target!=desired||timeout!=seconds)return fail(result,"TRANSACTION_ID_CONFLICT");result=status();return true;}
  if(busy()||time(nullptr)<1700000000)return fail(result,"TRANSACTION_BUSY_OR_CLOCK_REQUIRED");
  JsonDocument broker;deserializeJson(broker,mqttStaging.status(false));String b=broker["transaction"]["state"]|"UNKNOWN";if(b!="IDLE"&&b!="CANCELLED"&&b!="ROLLED_BACK"&&b!="COMMITTED"&&b!="FAILED")return fail(result,"BROKER_TRANSACTION_BUSY");
  DeviceConfig candidate=configManager.get();candidate.local_ip_enabled=desired;String error;if(!configManager.validate(candidate,error)||(!desired&&mqttStaging.active().isEmpty()))return fail(result,"TARGET_TRANSPORT_NOT_PROVISIONED");
  if(desired==transportManager.localMode())return fail(result,"TRANSPORT_ALREADY_ACTIVE");
  tx=id;oldMode=transportManager.localMode();target=desired;timeout=seconds;phase=Phase::PREPARED;deadline=time(nullptr)+600;pingSeen=false;
 }else{
  if(args.size()!=3||id!=tx)return fail(result,"TRANSACTION_NOT_FOUND");
  if(operation=="apply"){if(phase==Phase::TRIAL){result=status();return true;}if(phase!=Phase::PREPARED)return fail(result,"PREPARE_REQUIRED");phase=Phase::TRIAL;deadline=time(nullptr)+timeout;trialStarted=millis();applyAt=millis()+3000;applied=false;}
  else if(operation=="confirm"){
   if(phase==Phase::COMMITTED){result=status();return true;}
   if(!TransportTrial::canConfirm(phase,applied&&transportManager.localMode()==target,pingSeen,transportManager.receiptCounter()!=receiptAt,time(nullptr),deadline))return fail(result,"NEW_PATH_PROOF_REQUIRED");
   JsonDocument d;d["local_ip_enabled"]=target;String raw,error;serializeJson(d,raw);if(!configManager.update(raw,error))return fail(result,"COMMIT_STORAGE_FAILED");phase=Phase::COMMITTED;
  }else if(operation=="cancel"){if(phase==Phase::TRIAL){if(!restore()){storage=false;return fail(result,"ROLLBACK_STORAGE_FAILED");}result=status();return true;}if(phase==Phase::PREPARED)phase=Phase::CANCELLED;else if(phase!=Phase::CANCELLED&&phase!=Phase::ROLLED_BACK)return fail(result,"NOT_CANCELLABLE");}
  else return fail(result,"INVALID_TRANSPORT_ACTION");
 }
 if(!save()){storage=false;transportManager.requestLocalMode(oldMode);return fail(result,"JOURNAL_STORAGE_FAILED");}result=status();return true;
}
void loop(){if(!storage)return;if(TransportTrial::expired(phase,time(nullptr),deadline)||(phase==Phase::TRIAL&&uint32_t(millis()-trialStarted)>=timeout*1000U)){if(phase==Phase::TRIAL)storage=restore();else{phase=Phase::CANCELLED;storage=save();}return;}if(phase==Phase::TRIAL&&!applied&&(int32_t)(millis()-applyAt)>=0){receiptAt=transportManager.receiptCounter();transportManager.requestLocalMode(target);applied=true;}}
void ping(const String& message,const String& channel,JsonDocument& response){bool path=channel==(target?"IP":"MQTT");if(phase==Phase::TRIAL&&applied&&transportManager.localMode()==target&&path&&message=="TRANSPORT:"+tx&&time(nullptr)<deadline)pingSeen=true;response["transport"]=channel;response["transaction_id"]=phase==Phase::TRIAL?tx:"";response["path_verified"]=pingSeen&&path;}
}
