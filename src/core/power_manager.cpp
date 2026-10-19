#include "power_manager.h"
#include "feature_core.h"
#include "connection_indicators.h"
#include "config_manager.h"
#include "system_manager.h"
#include "../config/mqtt_staging.h"
#include <Preferences.h>
#include <esp_sleep.h>
#include <WiFi.h>
#include <time.h>
PowerManager powerManager;
namespace {bool persist(const String& raw){Preferences p;if(!p.begin("operations",false))return false;bool ok=p.putString("restart",raw)==raw.length();p.end();return ok;}}
void PowerManager::begin(){Preferences p;if(!p.begin("operations",false)){lastError="STORAGE_UNAVAILABLE";return;}ready=true;if(p.isKey("restart"))marker=p.getString("restart");p.end();if(marker.length()){JsonDocument d;if(deserializeJson(d,marker)||!d["command_id"].is<const char*>()||!d["source"].is<const char*>()){lastError="RESTART_RECOVERY_REQUIRED";marker="";ready=false;}}}
bool PowerManager::requestRestart(const Message& m,String& error){if(!ready||restarting){error=!ready?"STORAGE_UNAVAILABLE":"RESTART_BUSY";return false;}JsonDocument previous;if(marker.length()&&!deserializeJson(previous,marker)&&previous["command_id"].as<String>()==m.messageId&&previous["source"].as<String>()==m.source){if(previous["request"].as<String>()!=m.payload){error="COMMAND_ID_CONFLICT";return false;}return true;}JsonDocument d;d["command_id"]=m.messageId;d["source"]=m.source;d["request"]=m.payload;String raw;serializeJson(d,raw);if(!persist(raw)){error="STORAGE_FAILED";return false;}marker=raw;restarting=true;requested=millis();state="RESTART_DRAINING";return true;}
bool PowerManager::replayRestart(const Message& m,Message& out)const{JsonDocument d;if(!marker.length()||deserializeJson(d,marker)||d["command_id"].as<String>()!=m.messageId||d["source"].as<String>()!=m.source||d["request"].as<String>()!=m.payload)return false;return d["response"].is<const char*>()&&Message::fromJson(d["response"],out);}
bool PowerManager::bootResponse(Message& out){if(restarting||bootQueued||!marker.length()||time(nullptr)<1700000000)return false;JsonDocument d;if(deserializeJson(d,marker))return false;if(d["response"].is<const char*>())return Message::fromJson(d["response"],out);
 JsonDocument body;body["command_id"]=d["command_id"];bool confirmed=systemManager.resetReason()=="SOFTWARE";body["status"]=confirmed?"SUCCESS":"FAILED";body["data"]["restarted"]=true;body["data"]["operation_confirmed"]=confirmed;if(!confirmed)body["data"]["error"]="RESET_CAUSE_NOT_REQUESTED_SOFTWARE_RESTART";body["data"]["reset_reason"]=systemManager.resetReason();String data;serializeJson(body,data);out=Message{};out.messageId=generateMessageId();out.source=configManager.get().device_id;out.origin=out.source;out.destination=d["source"].as<String>();out.nextHop=out.destination;out.type=PacketType::RESPONSE;out.correlationId=d["command_id"].as<String>();out.timestamp=time(nullptr);out.payload=data;d["response"]=out.toJson();String raw;serializeJson(d,raw);if(!persist(raw)){lastError="RESULT_STORAGE_FAILED";return false;}marker=raw;return true;}
void PowerManager::bootResponseQueued(){bootQueued=true;}
bool PowerManager::draining()const{return restarting||state=="DRAINING";}
void PowerManager::loop(unsigned pending,bool idle){auto& c=configManager.get();FeatureCore::Schedule schedule;schedule.enabled=c.sleep_enabled;schedule.start=c.sleep_start_minute;schedule.duration=c.sleep_duration_seconds;schedule.days=c.sleep_days_mask;schedule.offset=c.utc_offset_minutes;remaining=FeatureCore::sleepRemaining(schedule,time(nullptr));
 if(!restarting)state=!c.sleep_enabled?"ACTIVE":time(nullptr)<1700000000?"CLOCK_REQUIRED":remaining?"DRAINING":"ACTIVE";
 if(!draining())return;
 // Configuration coordinator must be quiescent; never interrupt a broker commit.
 JsonDocument tx;deserializeJson(tx,mqttStaging.status(false));String transaction=tx["transaction"]["state"]|"UNKNOWN";
 bool quiescent=transaction=="IDLE"||transaction=="CANCELLED"||transaction=="ROLLED_BACK";
 auto decision=FeatureCore::powerDecision(restarting,millis()-requested,c.sleep_enabled,time(nullptr)>=1700000000,remaining,pending,idle,quiescent);
 if(decision==FeatureCore::PowerDecision::DRAINING){lastError="WAITING_FOR_PENDING_WORK";return;}
 lastError="";if(decision==FeatureCore::PowerDecision::RESTART){ESP.restart();return;}
 if(decision==FeatureCore::PowerDecision::SLEEP){connectionIndicators.stop();WiFi.disconnect(true);esp_sleep_enable_timer_wakeup((uint64_t)remaining*1000000ULL);state="SLEEPING";esp_deep_sleep_start();}
}
String PowerManager::statusJson()const{JsonDocument d;d["state"]=state;d["last_error"]=lastError;d["clock_valid"]=time(nullptr)>=1700000000;d["sleep_remaining_seconds"]=remaining;d["remote_wake_supported"]=false;d["restart_pending"]=restarting;d["restart_storage_ready"]=ready;d["reset_reason"]=systemManager.resetReason();auto& c=configManager.get();d["sleep_enabled"]=c.sleep_enabled;d["sleep_start_minute"]=c.sleep_start_minute;d["sleep_duration_seconds"]=c.sleep_duration_seconds;d["sleep_days_mask"]=c.sleep_days_mask;d["utc_offset_minutes"]=c.utc_offset_minutes;d["wake_minute"]=(c.sleep_start_minute+(c.sleep_duration_seconds+59)/60)%1440;d["active_seconds_per_day"]=86400-c.sleep_duration_seconds;String raw;serializeJson(d,raw);return raw;}
