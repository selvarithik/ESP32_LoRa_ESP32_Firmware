#include "commissioning.h"
#include "config_manager.h"
#include "../config/mqtt_staging.h"
#include <Preferences.h>
#include <ArduinoJson.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>
#include <esp_system.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include <esp_heap_caps.h>
#include <sys/time.h>
#include "../communication/transport_manager.h"

namespace Commissioning {
namespace {
String salt, verifier, line;
String accountName="admin";
uint32_t generation=0;
bool mustChange=true, available=false, overflow=false;
uint32_t lastByte=0;
String hex(const unsigned char* b,size_t n){String s;for(size_t i=0;i<n;i++){char t[3];snprintf(t,3,"%02x",b[i]);s+=t;}return s;}
String derive(const String& password,const String& s){
    // PBKDF2 block 1 (32-byte SHA-256 output), yielding between bounded batches.
    unsigned char u[32],out[32];mbedtls_md_context_t c;mbedtls_md_init(&c);
    if(mbedtls_md_setup(&c,mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),1)){mbedtls_md_free(&c);return "";}
    const unsigned char block[4]={0,0,0,1};
    int rc=mbedtls_md_hmac_starts(&c,(const unsigned char*)password.c_str(),password.length());
    rc|=mbedtls_md_hmac_update(&c,(const unsigned char*)s.c_str(),s.length());rc|=mbedtls_md_hmac_update(&c,block,4);rc|=mbedtls_md_hmac_finish(&c,u);memcpy(out,u,32);
    for(unsigned round=1;!rc&&round<100000;round++){
        rc=mbedtls_md_hmac_reset(&c);rc|=mbedtls_md_hmac_update(&c,u,32);rc|=mbedtls_md_hmac_finish(&c,u);
        for(unsigned j=0;j<32;j++)out[j]^=u[j];
        if((round&255)==0){if(esp_task_wdt_status(nullptr)==ESP_OK)esp_task_wdt_reset();delay(1);}
    }
    mbedtls_md_free(&c);String result=rc?String():hex(out,32);memset(out,0,32);memset(u,0,32);return result;
}
bool equal(const String& a,const String& b){unsigned diff=a.length()^b.length();for(size_t i=0;i<a.length()&&i<b.length();++i)diff|=a[i]^b[i];return diff==0;}
bool setPassword(const String& p,bool initial,const String& user=""){
    unsigned char random[16];esp_fill_random(random,16);String s=hex(random,16),v=derive(p,s);if(v.isEmpty())return false;
    JsonDocument d;d["salt"]=s;d["verifier"]=v;d["must_change"]=initial;d["username"]=user.length()?user:accountName;
    if(d.overflowed())return false;String raw;size_t length=measureJson(d);if(!raw.reserve(length))return false;serializeJson(d,raw);if(raw.length()!=length)return false;
    Preferences n;if(!n.begin("device",false))return false;bool ok=n.putString("login_v1",raw)==raw.length();n.end();
    if(ok){if(user.length())accountName=user;salt=s;verifier=v;mustChange=initial;available=true;++generation;}return ok;
}
void reply(bool ok,const char* code){Serial.printf("[COMMISSION] {\"ok\":%s,\"code\":\"%s\"}\n",ok?"true":"false",code);}
void dispatch(){
    JsonDocument d;if(deserializeJson(d,line)||!d.is<JsonObject>()){reply(false,"INVALID_REQUEST");return;}
    String op=d["op"]|""; // Physical USB possession authorizes commissioning. Never echo input.
    if(op=="status"&&d.size()==1){
        JsonDocument out;out["ok"]=true;out["device_id"]=configManager.get().device_id;out["username"]=accountName;out["password_change_required"]=required();out["station_ip"]=WiFi.localIP().toString();out["ap_ip"]=WiFi.softAPIP().toString();out["free_heap"]=ESP.getFreeHeap();out["loop_stack_free_min_bytes"]=uxTaskGetStackHighWaterMark(nullptr);out["network_stack_free_min_bytes"]=transportManager.minimumStackFreeBytes();out["free_8bit_heap"]=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);out["largest_8bit_block"]=heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);out["largest_free_block"]=ESP.getMaxAllocHeap();out["minimum_free_heap"]=ESP.getMinFreeHeap();out["uptime_ms"]=millis();out["unix_time"]=time(nullptr);
        JsonDocument mqtt;deserializeJson(mqtt,mqttStaging.status(false));out["mqtt"]=mqtt;
        String raw;serializeJson(out,raw);Serial.print("[COMMISSION] ");Serial.println(raw);
    }else if(op=="self_test"&&d.size()==1){
        uint32_t started=millis();bool ok=derive("public-pbkdf2-fixture","00112233445566778899aabbccddeeff")=="71c9db0332994401d7d514fe7cd8fc4ca25d7286a222e83ea86d32600f7410e0";
        JsonDocument out;out["ok"]=ok;out["elapsed_ms"]=millis()-started;String raw;serializeJson(out,raw);Serial.print("[COMMISSION] ");Serial.println(raw);
    }else if(op=="time"){
        if(d.size()!=2||d["unix_time"].is<bool>()||!d["unix_time"].is<uint32_t>()||d["unix_time"].as<uint32_t>()<1700000000||d["unix_time"].as<uint32_t>()>4102444800UL){reply(false,"INVALID_TIME");return;}
        timeval now{};now.tv_sec=d["unix_time"].as<uint32_t>();reply(settimeofday(&now,nullptr)==0,"USB_CLOCK_RESULT");
    }else if(op=="recover_account"){
        if(d.size()!=2||d["confirm_device_id"].as<String>()!=configManager.get().device_id){reply(false,"IDENTITY_CONFIRMATION_REQUIRED");return;}
        reply(setPassword("admin",true,"admin"),"ACCOUNT_RECOVERY_PASSWORD_CHANGE_REQUIRED");
    }else if(op=="password"){
        String p=d["password"]|"";
        if(d.size()!=2||p.length()<12||p.length()>64||p=="admin"){reply(false,"INVALID_PASSWORD");return;}
        reply(setPassword(p,false),"PASSWORD_RESULT");
    }else if(op=="config"){
        if(mustChange){reply(false,"PASSWORD_CHANGE_REQUIRED");return;}
        auto v=d["config"].as<JsonObjectConst>();bool valid=d.size()==2&&!v.isNull();
        for(JsonPairConst p:v){String k=p.key().c_str();if(k!="wifi_ssid"&&k!="wifi_password"&&k!="setup_password"&&k!="mesh_key"&&k!="gateway_token"&&k!="role")valid=false;}
        if(!valid){reply(false,"INVALID_REQUEST");return;}
        String raw,error;serializeJson(v,raw);reply(configManager.update(raw,error),"CONFIG_RESULT_RESTART_REQUIRED");
    }else if(op=="mqtt_action"){
        if(mustChange){reply(false,"PASSWORD_CHANGE_REQUIRED");return;}
        String action=d["action"]|"";
        if(configManager.get().role!=DeviceRole::MASTER||d.size()!=3||(action!="candidate"&&action!="test"&&action!="cancel")){reply(false,"INVALID_REQUEST");return;}
        String raw,response;serializeJson(d["request"],raw);mqttStaging.action(action,raw,response);
        Serial.print("[COMMISSION] ");Serial.println(response);
    }else if(op=="mqtt"||op=="mqtt_restore"){
        if(mustChange){reply(false,"PASSWORD_CHANGE_REQUIRED");return;}
        String raw,response;serializeJson(d["request"],raw);int code=mqttStaging.provision(raw,response,op=="mqtt_restore");reply(code==200,"MQTT_RESULT");
    }else reply(false,"UNKNOWN_OPERATION");
}
}
bool begin(){
    Preferences n;if(!n.begin("device",false))return false;String raw=n.isKey("login_v1")?n.getString("login_v1"):"";n.end();
    if(raw.length()){JsonDocument d;if(deserializeJson(d,raw)||!d["must_change"].is<bool>())return false;salt=d["salt"]|"";verifier=d["verifier"]|"";mustChange=d["must_change"];accountName=d["username"]|"admin";available=salt.length()==32&&verifier.length()==64;}
    else {String old=configManager.get().admin_password;bool initial=old.isEmpty()||old=="admin";if(!setPassword(initial?String("admin"):old,initial))return false;}
    // Clear legacy recoverable login password after the independent verifier is durable.
    configManager.get().admin_password="";return available&&configManager.save();
}
bool required(){return !available||mustChange;}
const String& username(){return accountName;}
uint32_t credentialGeneration(){return generation;}
bool verifyCredentials(const String& user,const String& password){
    if(!available||user!=accountName||password.length()>64)return false;
    return equal(derive(password,salt),verifier);
}
bool updateAccount(const String& user,const String& currentPassword,const String& newPassword){
    if(user.length()<3||user.length()>32||newPassword.length()<12||newPassword.length()>64||newPassword=="admin")return false;
    for(unsigned i=0;i<user.length();++i){char c=user[i];if(!isalnum((unsigned char)c)&&c!='_'&&c!='-'&&c!='.')return false;}
    if(!verifyCredentials(accountName,currentPassword))return false;
    return setPassword(newPassword,false,user);
}
void loop(){
    if(line.length()&&millis()-lastByte>5000){line="";overflow=false;}
    for(unsigned i=0;i<128&&Serial.available();++i){char c=Serial.read();lastByte=millis();if(c=='\n'){
        if(!overflow&&line.length())dispatch();else if(overflow)reply(false,"TOO_LARGE");line="";overflow=false;
    }else if(c!='\r'){if(line.length()<8192&&!overflow)line+=c;else overflow=true;}}
}
}
