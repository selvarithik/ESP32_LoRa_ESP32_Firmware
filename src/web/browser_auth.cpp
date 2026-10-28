#include "local_web_server.h"
#include "../core/commissioning.h"
#include "../config/strict_json.h"
#include "web_assets_gzip.h"
#include <ArduinoJson.h>
#include <esp_system.h>
#include <atomic>

namespace {
struct LoginWork {Commissioning::Verification captured;String session;uint32_t generation=0,started=0;bool used=false,accepted=false;std::atomic<bool> done{false};} loginWork;
void verifyLoginWorker(void*){loginWork.accepted=Commissioning::verifyCaptured(loginWork.captured);loginWork.done.store(true);vTaskDelete(nullptr);}
String randomHex(size_t bytes){uint8_t raw[32];esp_fill_random(raw,bytes);String value;value.reserve(bytes*2);for(size_t i=0;i<bytes;i++){char part[3];snprintf(part,3,"%02x",raw[i]);value+=part;}return value;}
String cookieToken(const String& cookies){int offset=0;while(offset<(int)cookies.length()){int end=cookies.indexOf(';',offset);if(end<0)end=cookies.length();String item=cookies.substring(offset,end);item.trim();if(item.startsWith("esp_session="))return item.substring(12);offset=end+1;}return "";}
}
BrowserSessions::Entry* LocalWebServer::browserSession(bool create){
    String token=cookieToken(_server.header("Cookie"));if(token.length()!=64)token="";auto* entry=sessions.find(token.c_str(),millis(),Commissioning::credentialGeneration());
    if(!entry&&create){String t=randomHex(32),c=randomHex(16);entry=sessions.create(t.c_str(),c.c_str(),millis(),Commissioning::credentialGeneration(),false);if(entry)_server.sendHeader("Set-Cookie","esp_session="+t+"; Path=/; HttpOnly; SameSite=Strict");}
    return entry;
}
void LocalWebServer::sendAccessPage(bool account){if(account&&!authorize())return;_server.sendHeader("Content-Encoding","gzip");_server.sendHeader("Cache-Control","no-store");_server.send_P(200,"text/html",(const char*)accessGzip,sizeof(accessGzip));}
void LocalWebServer::handleSession(){
    auto* e=browserSession(true);if(!e){sendJson(503,"{\"error\":\"SESSION_UNAVAILABLE\"}");return;}
    JsonDocument d;d["authenticated"]=e->authenticated;d["csrf"]=e->csrf;d["idle_timeout_s"]=900;d["transport"]= "HTTP_UNENCRYPTED";
    if(e->authenticated){d["username"]=Commissioning::username();d["password_change_required"]=Commissioning::required();}
    String raw;serializeJson(d,raw);sendJson(200,raw);
}
void LocalWebServer::handleLogin(){
    auto* entry=browserSession();if(!entry||!BrowserSessions::same(entry->csrf,_server.header("X-CSRF-Token").c_str())){sendJson(403,"{\"error\":\"Reload the login page.\"}");return;}
    if(uint32_t(millis()-loginWindow)>=60000){loginWindow=millis();loginFailures=0;}
    if(loginFailures>=5){sendJson(429,"{\"error\":\"Too many attempts; retry in one minute.\"}");return;}
    if(!body(512))return;JsonDocument d;const String& raw=_server.arg("plain");
    if(!StrictJson::valid(raw,512)||deserializeJson(d,raw)||!d.is<JsonObject>()||d.size()!=2||!d["username"].is<String>()||!d["password"].is<String>()){sendJson(400,"{\"error\":\"User ID and password required.\"}");return;}
    if(d["username"].as<String>().length()>32||d["password"].as<String>().isEmpty()||d["password"].as<String>().length()>64){sendJson(400,"{\"error\":\"Invalid credential lengths.\"}");return;}
    ++loginFailures;
    if(!Commissioning::verifyCredentials(d["username"],d["password"])){sendJson(401,"{\"error\":\"Invalid user ID or password.\"}");return;}
    String token=randomHex(32),csrf=randomHex(16);if(token.length()!=64||csrf.length()!=32){sendJson(503,"{\"error\":\"SESSION_UNAVAILABLE\"}");return;}
    sessions.revoke(entry);if(!sessions.create(token.c_str(),csrf.c_str(),millis(),Commissioning::credentialGeneration(),true)){sendJson(503,"{\"error\":\"SESSION_UNAVAILABLE\"}");return;}
    _server.sendHeader("Set-Cookie","esp_session="+token+"; Path=/; HttpOnly; SameSite=Strict");loginFailures=0;
    sendJson(200,Commissioning::required()?"{\"success\":true,\"password_change_required\":true}":"{\"success\":true,\"password_change_required\":false}");
}
void LocalWebServer::handleLogout(){
    if(!authorize(true))return;sessions.revoke(browserSession());_server.sendHeader("Set-Cookie","esp_session=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0");sendJson(200,"{\"success\":true}");
}
void LocalWebServer::handleAccount(){
    if(!authorize(true)||!body(768))return;
    if(uint32_t(millis()-loginWindow)>=60000){loginWindow=millis();loginFailures=0;}
    if(loginFailures>=5){sendJson(429,"{\"error\":\"Too many attempts; retry in one minute.\"}");return;}
    JsonDocument d;const String& raw=_server.arg("plain");
    if(!StrictJson::valid(raw,768)||deserializeJson(d,raw)||!d.is<JsonObject>()||d.size()!=3||!d["username"].is<String>()||!d["password"].is<String>()||!d["new_password"].is<String>()){sendJson(400,"{\"error\":\"Complete account fields required.\"}");return;}
    ++loginFailures;
    if(!Commissioning::updateAccount(d["username"],d["password"],d["new_password"])){sendJson(400,"{\"error\":\"Check the current password, user ID (3–32 letters/digits/._-) and new password (12–64 UTF-8 bytes), or retry local storage.\"}");return;}
    loginFailures=0;_server.sendHeader("Set-Cookie","esp_session=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0");sendJson(200,"{\"success\":true,\"reauthenticate\":true}");
}

void LocalWebServer::handleLoginStart(){
 auto* entry=browserSession();if(!entry||!BrowserSessions::same(entry->csrf,_server.header("X-CSRF-Token").c_str())){sendJson(403,"{\"error\":\"Reload the login page.\"}");return;}
 if(loginWork.used){if(loginWork.done.load()&&uint32_t(millis()-loginWork.started)>60000)loginWork.used=false;else{sendJson(429,"{\"error\":\"Verification busy; wait, then reload.\"}");return;}}
 if(uint32_t(millis()-loginWindow)>=60000){loginWindow=millis();loginFailures=0;}
 if(loginFailures>=5){sendJson(429,"{\"error\":\"Too many attempts; retry in one minute.\"}");return;}
 if(!body(512))return;JsonDocument d;const String raw=_server.arg("plain");
 if(!StrictJson::valid(raw,512)||deserializeJson(d,raw)||!d.is<JsonObject>()||d.size()!=2||!d["username"].is<String>()||!d["password"].is<String>()||d["username"].as<String>().length()>32||d["password"].as<String>().isEmpty()||d["password"].as<String>().length()>64){sendJson(400,"{\"error\":\"User ID and password required.\"}");return;}
 ++loginFailures;loginWork.used=true;loginWork.done.store(false);loginWork.accepted=false;loginWork.session=entry->token;loginWork.generation=Commissioning::credentialGeneration();loginWork.started=millis();loginWork.captured=Commissioning::captureVerification(d["username"],d["password"]);
 if(xTaskCreate(verifyLoginWorker,"login-check",6144,nullptr,1,nullptr)!=pdPASS){loginWork.used=false;loginWork.captured=Commissioning::Verification{};sendJson(503,"{\"error\":\"VERIFICATION_UNAVAILABLE\"}");return;}
 sendJson(202,"{\"pending\":true}");
}
void LocalWebServer::handleLoginResult(){
 auto* entry=browserSession();if(!entry||!BrowserSessions::same(entry->csrf,_server.header("X-CSRF-Token").c_str())||!loginWork.used||loginWork.session!=entry->token){sendJson(403,"{\"error\":\"Reload the login page.\"}");return;}
 if(!loginWork.done.load()){sendJson(202,"{\"pending\":true}");return;}
 loginWork.used=false;loginWork.session="";
 if(loginWork.generation!=Commissioning::credentialGeneration()){sendJson(409,"{\"error\":\"Credentials changed; reload the login page.\"}");return;}
 if(!loginWork.accepted){sendJson(401,"{\"error\":\"Invalid user ID or saved password.\"}");return;}
 String token=randomHex(32),csrf=randomHex(16);sessions.revoke(entry);
 if(!sessions.create(token.c_str(),csrf.c_str(),millis(),Commissioning::credentialGeneration(),true)){sendJson(503,"{\"error\":\"SESSION_UNAVAILABLE\"}");return;}
 _server.sendHeader("Set-Cookie","esp_session="+token+"; Path=/; HttpOnly; SameSite=Strict");loginFailures=0;
 sendJson(200,Commissioning::required()?"{\"success\":true,\"password_change_required\":true}":"{\"success\":true,\"password_change_required\":false}");
}
