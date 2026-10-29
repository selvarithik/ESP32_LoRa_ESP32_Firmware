#include "wifi_manager.h"
#include "../identity/device_identity.h"
#include "../core/logger.h"
#include "../core/config_manager.h"
#include "../core/feature_core.h"
#include <atomic>
WiFiManagerESP32 wifiManager;
namespace { uint32_t lastAttempt=0, connectedSince=0; wl_status_t previous=WL_IDLE_STATUS;unsigned attempts=0;std::atomic<unsigned> disconnectReason{0};uint32_t lastConnected=0;bool appliedDhcp=true;String appliedIp,appliedMask,appliedGateway,appliedDns,appliedDns2; }
bool WiFiManagerESP32::begin(const DeviceConfig& c) {
    _enabled=c.wifi_enabled;_ssid=c.wifi_ssid;_password=c.wifi_password;appliedDhcp=c.wifi_dhcp;appliedIp=c.wifi_ip;appliedMask=c.wifi_subnet;appliedGateway=c.wifi_gateway;appliedDns=c.wifi_dns;appliedDns2=c.wifi_dns2;
    WiFi.onEvent([](WiFiEvent_t,WiFiEventInfo_t info){disconnectReason=info.wifi_sta_disconnected.reason;},ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    WiFi.persistent(false);WiFi.setAutoReconnect(false);WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(IPAddress(192,168,100,1),IPAddress(192,168,100,1),IPAddress(255,255,255,0));
    _apSsid="Setup-"+deviceIdentity.deviceId();
    _apStarted=WiFi.softAP(_apSsid.c_str(),c.setup_password.c_str());
    if(_apStarted)logger.info("WIFI","Recovery AP started: "+_apSsid);
    if(!c.wifi_dhcp) { IPAddress ip,sub,g,d,d2;ip.fromString(c.wifi_ip);sub.fromString(c.wifi_subnet);g.fromString(c.wifi_gateway);d.fromString(c.wifi_dns);if(!c.wifi_dns2.isEmpty())d2.fromString(c.wifi_dns2);if(!WiFi.config(ip,g,sub,d,d2))logger.warn("WIFI","Static network rejected; recovery AP remains available"); }
    if(_enabled&&_ssid.length())connect(_ssid,_password,0);
    return true;
}
void WiFiManagerESP32::suspend(bool value){if(value==suspended)return;suspended=value;if(value){connectedSince=0;_apStarted=false;WiFi.softAPdisconnect(true);WiFi.disconnect(true);}else if(_enabled){WiFi.mode(WIFI_STA);connect(_ssid,_password,0);}}
void WiFiManagerESP32::loop() {
    if(suspended)return;
    uint32_t now=millis();auto status=WiFi.status();
    if(status!=previous) { previous=status;logger.info("WIFI",status==WL_CONNECTED?"Station connected IP="+WiFi.localIP().toString():"Station disconnected; recovery active"); }
    if(status==WL_CONNECTED) {
        if(!connectedSince){connectedSince=now;lastConnected=now;attempts=0;disconnectReason=0;
            uint32_t mask=static_cast<uint32_t>(WiFi.subnetMask()),sta=static_cast<uint32_t>(WiFi.localIP()),ap=static_cast<uint32_t>(WiFi.softAPIP());
            if(_apStarted&&(sta&mask)==(ap&mask)){
                // Recovery access resumes after station loss; do not install overlapping routes.
                WiFi.softAPdisconnect(true);_apStarted=false;logger.warn("WIFI","Recovery AP suspended: station subnet overlaps recovery subnet");
            }
        }
        if(_apStarted&&now-connectedSince>60000) {WiFi.softAPdisconnect(true);_apStarted=false;logger.info("WIFI","Setup AP stopped");}
    } else {
        connectedSince=0;
        if(!_apStarted&&now-lastAttempt>30000) {WiFi.mode(WIFI_AP_STA);_apStarted=WiFi.softAP(_apSsid.c_str(),configManager.get().setup_password.c_str());}
        if(_enabled&&_ssid.length()&&now-lastAttempt>FeatureCore::reconnectDelay(attempts))connect(_ssid,_password,0);
    }
}
bool WiFiManagerESP32::connect(const String& ssid,const String& password,uint32_t) { if(!ssid.length())return false;_ssid=ssid;_password=password;_enabled=true;lastAttempt=millis();++attempts;WiFi.begin(ssid.c_str(),password.c_str());return true; }
bool WiFiManagerESP32::disconnect() { _enabled=false;return WiFi.disconnect(false); }
String WiFiManagerESP32::scanJson() {
    int count=WiFi.scanComplete();JsonDocument d;JsonArray a=d.to<JsonArray>();
    if(count==WIFI_SCAN_FAILED)WiFi.scanNetworks(true,true);
    if(count>=0) {for(int i=0;i<min(count,24);++i) {auto o=a.add<JsonObject>();o["ssid"]=WiFi.SSID(i);o["rssi"]=WiFi.RSSI(i);o["encryption"]=(int)WiFi.encryptionType(i);}WiFi.scanDelete();}
    String s;serializeJson(d,s);return s;
}
String WiFiManagerESP32::statusJson() const { JsonDocument d;d["enabled"]=_enabled;d["connected"]=isConnected();d["ip"]=ip().toString();d["rssi"]=isConnected()?rssi():0;d["ap_enabled"]=_apStarted;d["ap_ssid"]=_apSsid;d["ap_ip"]=WiFi.softAPIP().toString();d["dhcp"]=appliedDhcp;auto& saved=configManager.get();d["settings_pending_restart"]=saved.wifi_dhcp!=appliedDhcp||saved.wifi_ip!=appliedIp||saved.wifi_subnet!=appliedMask||saved.wifi_gateway!=appliedGateway||saved.wifi_dns!=appliedDns||saved.wifi_dns2!=appliedDns2;d["subnet"]=WiFi.subnetMask().toString();d["gateway"]=WiFi.gatewayIP().toString();d["dns1"]=WiFi.dnsIP(0).toString();d["dns2"]=WiFi.dnsIP(1).toString();d["disconnect_reason"]=disconnectReason.load();d["reconnect_attempts"]=attempts;d["last_connected_uptime_ms"]=lastConnected;d["scan_state"]=WiFi.scanComplete()==WIFI_SCAN_RUNNING?"SCANNING":"IDLE";String s;serializeJson(d,s);return s; }
bool WiFiManagerESP32::isConnected() const {return WiFi.status()==WL_CONNECTED;}
int WiFiManagerESP32::rssi() const {return WiFi.RSSI();}
IPAddress WiFiManagerESP32::ip() const {return WiFi.localIP();}
