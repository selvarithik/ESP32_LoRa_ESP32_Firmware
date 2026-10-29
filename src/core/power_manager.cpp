#include "power_manager.h"
#include "feature_core.h"
#include "connection_indicators.h"
#include "config_manager.h"
#include "system_manager.h"
#include "../config/mqtt_staging.h"
#include <Preferences.h>
#include <esp_sleep.h>
#include <WiFi.h>
#include "../communication/wifi_manager.h"
#include "../communication/transport_manager.h"
#include "../communication/lora_transport.h"
#include "../config/transport_trial.h"
#include <driver/gpio.h>
#include <time.h>
PowerManager powerManager;
namespace {bool persist(const String& raw){Preferences p;if(!p.begin("operations",false))return false;bool ok=p.putString("restart",raw)==raw.length();p.end();return ok;}}
void PowerManager::begin(){Preferences p;if(!p.begin("operations",false)){lastError="STORAGE_UNAVAILABLE";return;}ready=true;if(p.isKey("restart"))marker=p.getString("restart");p.end();if(marker.length()){JsonDocument d;if(deserializeJson(d,marker)||!d["command_id"].is<const char*>()||!d["source"].is<const char*>()){lastError="RESTART_RECOVERY_REQUIRED";marker="";ready=false;}}}
bool PowerManager::requestRestart(const Message& m,String& error){if(!ready||restarting){error=!ready?"STORAGE_UNAVAILABLE":"RESTART_BUSY";return false;}JsonDocument previous;if(marker.length()&&!deserializeJson(previous,marker)&&previous["command_id"].as<String>()==m.messageId&&previous["source"].as<String>()==m.source){if(previous["request"].as<String>()!=m.payload){error="COMMAND_ID_CONFLICT";return false;}return true;}JsonDocument d;d["command_id"]=m.messageId;d["source"]=m.source;d["request"]=m.payload;String raw;serializeJson(d,raw);if(!persist(raw)){error="STORAGE_FAILED";return false;}marker=raw;restarting=true;requested=millis();state="RESTART_DRAINING";return true;}
bool PowerManager::replayRestart(const Message& m,Message& out)const{JsonDocument d;if(!marker.length()||deserializeJson(d,marker)||d["command_id"].as<String>()!=m.messageId||d["source"].as<String>()!=m.source||d["request"].as<String>()!=m.payload)return false;return d["response"].is<const char*>()&&Message::fromJson(d["response"],out);}
bool PowerManager::bootResponse(Message& out){if(restarting||bootQueued||!marker.length()||time(nullptr)<1700000000)return false;JsonDocument d;if(deserializeJson(d,marker))return false;if(d["delivered"]==true)return false;if(d["response"].is<const char*>()){if(!Message::fromJson(d["response"],out))return false;return out.timestamp&&time(nullptr)-out.timestamp<=configManager.get().message_max_age_ms/1000;}
 JsonDocument body;body["command_id"]=d["command_id"];bool confirmed=systemManager.resetReason()=="SOFTWARE";body["status"]=confirmed?"SUCCESS":"FAILED";body["data"]["restarted"]=true;body["data"]["operation_confirmed"]=confirmed;if(!confirmed)body["data"]["error"]="RESET_CAUSE_NOT_REQUESTED_SOFTWARE_RESTART";body["data"]["reset_reason"]=systemManager.resetReason();String data;serializeJson(body,data);out=Message{};out.messageId=generateMessageId();out.source=configManager.get().device_id;out.origin=out.source;out.destination=d["source"].as<String>();out.nextHop=out.destination;out.type=PacketType::RESPONSE;out.correlationId=d["command_id"].as<String>();out.timestamp=time(nullptr);out.payload=data;d["response"]=out.toJson();String raw;serializeJson(d,raw);if(!persist(raw)){lastError="RESULT_STORAGE_FAILED";return false;}marker=raw;return true;}
void PowerManager::bootResponseQueued(){bootQueued=true;}
void PowerManager::resultPersisted(const String& id){if(!marker.length())return;JsonDocument d;Message response;if(deserializeJson(d,marker)||d["delivered"]==true||!d["response"].is<const char*>()||!Message::fromJson(d["response"],response)||response.messageId!=id)return;d["delivered"]=true;String raw;serializeJson(d,raw);if(persist(raw))marker=raw;else lastError="RESULT_RECEIPT_STORAGE_FAILED";}
bool PowerManager::draining()const{return sleeping||restarting||state=="DRAINING";}
bool PowerManager::requestSleep(uint32_t seconds,const String& mode,String& error){if(seconds<60||seconds>86400||(mode!="DEEP"&&mode!="LIGHT")){error="INVALID_SLEEP_REQUEST";return false;}if(sleeping||restarting||manualSleep||TransportSwitch::busy()){error="POWER_BUSY";return false;}manualSleep=true;manualAt=millis();manualDuration=seconds;manualMode=mode;state="DRAINING";return true;}
bool PowerManager::cancelPendingSleep(String& error){if(!manualSleep||sleeping||restarting){error="NO_CANCELLABLE_PENDING_SLEEP";return false;}manualSleep=false;remaining=0;state="ACTIVE";return true;}
void PowerManager::loop(unsigned pending,bool idle){auto& c=configManager.get();FeatureCore::Schedule schedule;schedule.enabled=c.sleep_enabled;schedule.start=c.sleep_start_minute;schedule.duration=c.sleep_duration_seconds;schedule.days=c.sleep_days_mask;schedule.offset=c.utc_offset_minutes;remaining=FeatureCore::sleepRemaining(schedule,time(nullptr));if(manualSleep){remaining=FeatureCore::manualSleepRemaining(millis(),manualAt,manualDuration);if(!remaining)manualSleep=false;}bool effectiveEnabled=c.sleep_enabled||manualSleep;String effectiveMode=manualSleep?manualMode:c.sleep_mode;
 if(sleeping){
  if(!remaining||!effectiveEnabled){sleeping=false;loraTransport.powerSleep(false);wifiManager.suspend(false);transportManager.pause(false);connectionIndicators.begin();state="WAKING";return;}
  state="SLEEPING";if(esp_sleep_enable_timer_wakeup(1000000ULL)!=ESP_OK||esp_light_sleep_start()!=ESP_OK){lastError="LIGHT_SLEEP_FAILED";connectionIndicators.begin();sleeping=false;wifiManager.suspend(false);transportManager.pause(false);loraTransport.powerSleep(false);}return;
 }
 if(!remaining&&!restarting){transportManager.pause(false);if(state=="DRAINING"){loraTransport.powerSleep(false);wifiManager.suspend(false);}}
 if(!restarting)state=!effectiveEnabled?"ACTIVE":time(nullptr)<1700000000?"CLOCK_REQUIRED":remaining?"DRAINING":"ACTIVE";
 if(!draining())return;
 // Configuration coordinator must be quiescent; never interrupt a broker commit.
 JsonDocument tx;deserializeJson(tx,mqttStaging.status(false));String transaction=tx["transaction"]["state"]|"UNKNOWN";
 bool quiescent=!TransportSwitch::busy()&&(transaction=="IDLE"||transaction=="CANCELLED"||transaction=="ROLLED_BACK"||transaction=="COMMITTED"||transaction=="FAILED");
 if(manualSleep&&millis()-manualAt<5000){lastError="WAITING_FOR_FINAL_RESPONSE";return;}
 auto decision=FeatureCore::powerDecision(restarting,millis()-requested,effectiveEnabled,time(nullptr)>=1700000000,remaining,pending,idle,quiescent);
 if(decision==FeatureCore::PowerDecision::DRAINING){lastError="WAITING_FOR_PENDING_WORK";return;}
 lastError="";if(decision==FeatureCore::PowerDecision::RESTART){ESP.restart();return;}
 if(decision==FeatureCore::PowerDecision::SLEEP){
 transportManager.pause(true);if(!transportManager.paused()){lastError="WAITING_FOR_NETWORK_WORKER";return;}
 if(c.lora_sleep_mode=="DEEP"&&!loraTransport.powerSleep(true)){lastError="WAITING_FOR_RADIO_SLEEP";return;}
 connectionIndicators.stop();wifiManager.suspend(true);state="SLEEPING";
 if(effectiveMode=="LIGHT"){sleeping=true;return;}
 if(esp_sleep_enable_timer_wakeup((uint64_t)remaining*1000000ULL)!=ESP_OK){lastError="TIMER_WAKE_FAILED";connectionIndicators.begin();loraTransport.powerSleep(false);wifiManager.suspend(false);transportManager.pause(false);return;}
 if(c.lora_enabled){gpio_hold_en((gpio_num_t)c.lora_m0_pin);gpio_hold_en((gpio_num_t)c.lora_m1_pin);gpio_deep_sleep_hold_en();}esp_deep_sleep_start();}
}
String PowerManager::statusJson()const{JsonDocument d;JsonDocument tx;deserializeJson(tx,TransportSwitch::status());d["transport_switch"]=tx.as<JsonVariant>();d["state"]=state;d["last_error"]=lastError;d["clock_valid"]=time(nullptr)>=1700000000;d["sleep_remaining_seconds"]=remaining;d["remote_wake_supported"]=false;d["manual_sleep_pending"]=manualSleep;d["effective_sleep_mode"]=manualSleep?manualMode:configManager.get().sleep_mode;d["restart_pending"]=restarting;d["restart_storage_ready"]=ready;d["reset_reason"]=systemManager.resetReason();auto& c=configManager.get();d["sleep_mode"]=c.sleep_mode;d["lora_sleep_mode"]=c.lora_sleep_mode;d["sleep_enabled"]=c.sleep_enabled;d["sleep_start_minute"]=c.sleep_start_minute;d["sleep_duration_seconds"]=c.sleep_duration_seconds;d["sleep_days_mask"]=c.sleep_days_mask;d["utc_offset_minutes"]=c.utc_offset_minutes;d["wake_minute"]=(c.sleep_start_minute+(c.sleep_duration_seconds+59)/60)%1440;d["active_seconds_per_day"]=86400-c.sleep_duration_seconds;String raw;serializeJson(d,raw);return raw;}
