#include "../config/firmware_version.h"
#include "config_manager.h"
#include "../config/default_config.h"
#include "../config/config_fields.h"
#include <Preferences.h>
#include <ArduinoJson.h>
#include <esp_system.h>
#include <WiFi.h>
#include <type_traits>
#include <limits>
#include "feature_core.h"
#include "indicator_core.h"
#include "../config/strict_json.h"

ConfigManager configManager;
namespace {
Preferences prefs;
String readStoredString(const char* key, const String& fallback) {
    // Missing keys are normal on first boot and during legacy migration.
    // Preferences::getString logs an error for absent keys even with a fallback.
    return prefs.isKey(key) ? prefs.getString(key, fallback) : fallback;
}
String secret() { char s[33]; for (int i=0;i<4;++i) snprintf(s+i*8,9,"%08lx",(unsigned long)esp_random()); return String(s); }
void encode(const DeviceConfig& c, JsonDocument& d, bool secrets) {
#define WRITE(f) d[#f] = c.f;
    CONFIG_FIELDS(WRITE)
#undef WRITE
    d["role"] = c.role == DeviceRole::MASTER ? "MASTER" : "NODE";
    d["device_id"] = c.device_id;
    d["configuration_version"] = c.configuration_version;
    d["configuration_revision"] = c.configuration_revision;
    d["firmware_version"] = FIRMWARE_VERSION;
    if (!secrets) {
        const char* keys[] = {"wifi_password","admin_password","setup_password","mesh_key","gateway_token","local_ip_token","mqtt_password","gateway_ca","mqtt_ca","internet_probe_ca"};
        for (auto k: keys) d.remove(k);
    }
}
bool assign(JsonVariantConst v, String& out) { if (!v.is<const char*>() || strlen(v.as<const char*>())>4096) return false; out=v.as<String>(); return true; }
bool assign(JsonVariantConst v, bool& out) { if (!v.is<bool>()) return false; out=v.as<bool>(); return true; }
template<typename T> bool assign(JsonVariantConst v, T& out) {
    if (!v.is<int64_t>() || v.is<bool>()) return false;
    int64_t n=v.as<int64_t>();
    if (n < static_cast<int64_t>(std::numeric_limits<T>::min()) || n > static_cast<int64_t>(std::numeric_limits<T>::max())) return false;
    out=static_cast<T>(n); return true;
}
bool safeId(const String& s, bool empty=false) {
    if ((!s.length()&&!empty)||s.length()>48) return false;
    for (size_t i=0;i<s.length();++i) if (!isalnum(s[i]) && s[i]!='-' && s[i]!='_' && s[i]!='.' && s[i]!=':') return false;
    return true;
}
bool host(const String& s) {
    if (s.length()>253 || s=="localhost" || s=="127.0.0.1" || s=="0.0.0.0") return false;
    for (size_t i=0;i<s.length();++i) if (!isalnum(s[i])&&s[i]!='.'&&s[i]!='-') return false;
    return true;
}
}

bool ConfigManager::begin() {
    _config=makeDefaultConfig();
    char id[32]; snprintf(id,sizeof(id),"ESP32-%012llX",(unsigned long long)ESP.getEfuseMac());
    _config.device_id=id;
    if (!prefs.begin("device",false)) {
        _config.admin_password=secret();_config.setup_password=secret();
        Serial.println("[PROVISION] NVS unavailable; protected commissioning required");
        return false;
    }
    _ready=true;
    // Migrate the original per-key schema without changing an established identity.
    _config.device_id=readStoredString("device_id",_config.device_id);
    _config.device_name=readStoredString("device_name",_config.device_name);
    _config.role=readStoredString("role",_config.role==DeviceRole::MASTER?"MASTER":"NODE")=="MASTER"?DeviceRole::MASTER:DeviceRole::NODE;
    _config.master_id=readStoredString("master_id","");
    _config.wifi_ssid=readStoredString("wifi_ssid","");
    _config.wifi_password=readStoredString("wifi_pass","");
    if(_config.device_id.isEmpty())_config.device_id=id;
    _config.mesh_id=readStoredString("mesh_id",_config.mesh_id);
    _config.preferred_master=readStoredString("pref_master","");
    _config.backup_master=readStoredString("backup_master","");
    _config.node_address=readStoredString("node_addr",_config.node_address);
    _config.allowed_peers_csv=readStoredString("peers","");
    _config.heartbeat_seconds=prefs.getUInt("hb_sec",_config.heartbeat_seconds);
    _config.route_timeout_seconds=prefs.getUInt("route_to",_config.route_timeout_seconds);
    _config.retry_count=prefs.getUInt("retries",_config.retry_count);
    _config.max_hops=prefs.getUInt("max_hops",_config.max_hops);
    _config.wifi_enabled=prefs.getBool("wifi_en",true);
    _config.ethernet_enabled=prefs.getBool("eth_en",false);
    _config.server_host=readStoredString("srv_host","");_config.server_port=prefs.getUInt("srv_port",5000);_config.server_https=prefs.getBool("srv_https",false);
#define LEGACY(field,key) _config.field=prefs.getUInt(key,_config.field);
    LEGACY(lora_baud,"lora_baud") LEGACY(lora_rx_pin,"lora_rx") LEGACY(lora_tx_pin,"lora_tx")
    LEGACY(lora_aux_pin,"lora_aux") LEGACY(lora_m0_pin,"lora_m0") LEGACY(lora_m1_pin,"lora_m1")
    LEGACY(lora_channel,"lora_ch") LEGACY(lora_air_rate,"lora_air") LEGACY(lora_tx_power,"lora_pwr")
#undef LEGACY
    bool storedInvalid=false;
    String stored=readStoredString("config_v1","");
    if (stored.length()) {
        String error;
        if (!update(stored,error,false)) { storedInvalid=true;Serial.println("[CONFIG] Invalid saved configuration; recovery defaults active"); _config=makeDefaultConfig(); _config.device_id=id; }
    }
    applyTemporaryWiFiDefaults(_config);
    bool generated=false;
    // Login migration and initial salted verifier are owned by Commissioning.
    if (_config.setup_password.length()<10) { _config.setup_password=secret(); generated=true; }
    String validationError;
    if(!validate(_config,validationError)) {
        String admin=_config.admin_password,setup=_config.setup_password,identity=_config.device_id;
        _config=makeDefaultConfig();_config.device_id=identity;_config.admin_password=admin;_config.setup_password=setup;
        Serial.println("[CONFIG] Invalid legacy fields; safe defaults active");
        generated=true;
    }
    if (generated&&!storedInvalid) {
        if (!save()) Serial.println("[CONFIG] WARNING: credentials could not be persisted");
    }
    if(generated||(_config.wifi_ssid.isEmpty()&&_config.mesh_key.isEmpty())) {
        // Credentials must not be emitted, even to the physical serial console.
        Serial.println("[PROVISION] Credentials generated; secret output disabled");
    }
    return true;
}

bool ConfigManager::validate(const DeviceConfig& c, String& error) const {
#define REQUIRE(test,msg) if (!(test)) { error=msg; return false; }
    REQUIRE(safeId(c.device_id)&&safeId(c.master_id,true)&&safeId(c.node_address,true),"Invalid device/master/node identity")
    REQUIRE(c.device_name.length()>0&&c.device_name.length()<=64,"Device name must be 1..64 characters")
    for(size_t i=0;i<c.device_name.length();++i)REQUIRE((uint8_t)c.device_name[i]>=32,"Device name contains control characters")
    REQUIRE(c.heartbeat_seconds>=5&&c.heartbeat_seconds<=3600&&c.telemetry_seconds>=2&&c.telemetry_seconds<=3600,"Intervals out of range")
    REQUIRE(c.retry_count<=5&&c.command_timeout_ms>=1000&&c.command_timeout_ms<=30000,"Retry/timeout out of range")
    REQUIRE(c.routing_policy=="MASTER_ONLY"||c.routing_policy=="DIRECT_ONLY"||c.routing_policy=="DIRECT_PREFERRED","Invalid local routing policy")
    REQUIRE(c.local_ip_token.isEmpty()||(c.local_ip_token.length()>=32&&c.local_ip_token.length()<=64),"Invalid local IP credential length")
    REQUIRE(!c.local_ip_enabled||(c.role==DeviceRole::NODE&&c.routing_policy=="MASTER_ONLY")||(!c.server_host.isEmpty()&&c.server_port&&(c.gateway_token.length()>=32||c.local_ip_token.length()>=32)),"Local Gateway address and provisioned credential required")
    REQUIRE(c.message_max_age_ms>=c.command_timeout_ms&&c.message_max_age_ms<=300000,"Message age out of range")
    REQUIRE(c.max_hops>=1&&c.max_hops<=15&&c.route_timeout_seconds>=10&&c.route_timeout_seconds<=86400,"Route settings out of range")
    REQUIRE(c.wifi_ssid.length()<=32&&(c.wifi_password.isEmpty()||(c.wifi_password.length()>=8&&c.wifi_password.length()<=63)),"Invalid Wi-Fi credentials")
    REQUIRE(host(c.server_host)&&host(c.master_host)&&host(c.mqtt_host)&&c.server_port&&c.mqtt_port,"Use a LAN IP/hostname, without scheme or path")
    REQUIRE(c.mesh_key.isEmpty()||(c.mesh_key.length()>=32&&c.mesh_key.length()<=64),"Mesh key must be 32..64 characters")
    REQUIRE((c.admin_password.isEmpty()||c.admin_password=="admin"||c.admin_password.length()>=10)&&c.admin_password.length()<=64&&c.setup_password.length()>=10&&c.setup_password.length()<=63,"Provisioning passwords must be 10..64 characters (setup AP maximum 63)")
    REQUIRE(!c.mqtt_enabled||(!c.mqtt_host.isEmpty()&&c.mqtt_keepalive>=15&&c.mqtt_keepalive<=300),"MQTT host/keepalive required")
    REQUIRE(!c.mqtt_tls||c.mqtt_ca.length()>0,"MQTT TLS requires a CA certificate")
    REQUIRE(!c.server_https||c.gateway_ca.length()>0,"HTTPS requires a CA certificate")
    REQUIRE(host(c.internet_probe_host)&&c.internet_probe_port,"Invalid Internet probe host/port")
    REQUIRE(c.internet_probe_host.isEmpty()||!c.internet_probe_tls||c.internet_probe_ca.length()>0,"Internet probe TLS requires a CA certificate")
    REQUIRE(c.sleep_mode=="DEEP"||c.sleep_mode=="LIGHT","Invalid sleep mode")
    REQUIRE(c.lora_sleep_mode=="KEEP"||c.lora_sleep_mode=="DEEP","Invalid E220 sleep mode")
    REQUIRE(c.lora_work_mode=="NORMAL"||c.lora_work_mode=="WOR_TX"||c.lora_work_mode=="WOR_RX","Invalid E220 work mode")
    REQUIRE(c.lora_wor_cycle_ms>=500&&c.lora_wor_cycle_ms<=4000&&c.lora_wor_cycle_ms%500==0,"Invalid WOR cycle")
    REQUIRE(c.lora_packet_bytes==64||c.lora_packet_bytes==128||c.lora_packet_bytes==200,"Invalid E220 packet size")
    REQUIRE(c.lora_tx_power==22||c.lora_tx_power==17||c.lora_tx_power==13||c.lora_tx_power==10,"Invalid E220 power")
    REQUIRE(c.lora_uart<=2&&c.lora_channel<=80&&c.lora_air_rate<=7&&c.lora_tx_power<=30,"E220 settings out of range")
    REQUIRE(c.lora_baud==1200||c.lora_baud==2400||c.lora_baud==4800||c.lora_baud==9600||c.lora_baud==19200||c.lora_baud==38400||c.lora_baud==57600||c.lora_baud==115200,"Invalid reserved E220 UART baud rate")
    REQUIRE(c.lora_rx_pin<=39&&c.lora_tx_pin<=33&&c.lora_aux_pin<=39&&c.lora_m0_pin<=33&&c.lora_m1_pin<=33,"E220 pin out of range")
    REQUIRE(c.dht_pin==4||c.dht_pin==13||c.dht_pin==14||c.dht_pin==18||c.dht_pin==19||c.dht_pin==21||c.dht_pin==22||c.dht_pin==23||c.dht_pin==32||c.dht_pin==33,"Unsupported DHT GPIO")
    REQUIRE(!IndicatorCore::pinConflict(c.role==DeviceRole::MASTER,c.tm1637_enabled,c.dht_enabled,c.dht_pin,c.server_led_enabled,c.lora_enabled,c.lora_rx_pin,c.lora_tx_pin,c.lora_aux_pin,c.lora_m0_pin,c.lora_m1_pin),"Indicator pins conflict with DHT or radio")
    REQUIRE(c.ina219_address>=64&&c.ina219_address<=79,"INA219 address must be 64..79")
    REQUIRE(c.ina219_shunt_milliohms<=1000,"INA219 shunt must be 0 (unknown) or 1..1000 milliohms")
    REQUIRE(c.mq135_warmup_seconds>=60&&c.mq135_warmup_seconds<=604800,"MQ135 warmup must be 60..604800 seconds")
    REQUIRE(!HardwareProfile::inaAllowed(c.role==DeviceRole::MASTER)||!c.ina219_enabled||!c.dht_enabled||(c.dht_pin!=21&&c.dht_pin!=22),"DHT pin conflicts with INA219 I2C")
    if (!c.wifi_dhcp) REQUIRE(FeatureCore::staticNetwork(c.wifi_ip.c_str(),c.wifi_subnet.c_str(),c.wifi_gateway.c_str(),c.wifi_dns.c_str(),c.wifi_dns2.c_str()),"Invalid static IPv4 mask, host, gateway or DNS")
    if(!c.wifi_dns2.isEmpty()){uint32_t dns;REQUIRE(FeatureCore::ipv4(c.wifi_dns2.c_str(),dns)&&FeatureCore::unicast(dns),"Invalid secondary DNS")}
    FeatureCore::Schedule schedule;schedule.enabled=c.sleep_enabled;schedule.start=c.sleep_start_minute;schedule.duration=c.sleep_duration_seconds;schedule.days=c.sleep_days_mask;schedule.offset=c.utc_offset_minutes;
    REQUIRE(FeatureCore::valid(schedule),"Invalid recurring sleep schedule")
    REQUIRE(c.lora_rx_pin>=0&&c.lora_tx_pin>=0&&c.lora_aux_pin>=0&&c.lora_m0_pin>=0&&c.lora_m1_pin>=0,"Radio pins must be nonnegative")
    if (c.role==DeviceRole::NODE) REQUIRE(c.master_id!=c.device_id,"Node cannot assign itself as Master")
    if (c.allowed_peers_csv.length()>1024) { error="Too many peer IDs"; return false; }
    String peers=","+c.allowed_peers_csv+",";
    for (int start=1;start<(int)peers.length(); ) { int end=peers.indexOf(',',start); if(end<0)break; String p=peers.substring(start,end); p.trim(); REQUIRE(safeId(p,true),"Invalid allowed peer ID") start=end+1; }
#undef REQUIRE
    return true;
}

bool ConfigManager::update(const String& json, String& error, bool persist) {
    if(!StrictJson::valid(json,12288)){error="Invalid JSON configuration";return false;}
    if(json.length()>12288) { error="Configuration too large"; return false; }
    JsonDocument d;
    if(deserializeJson(d,json)||!d.is<JsonObject>()) { error="JSON object required"; return false; }
    DeviceConfig c=_config;
    for(JsonPair p:d.as<JsonObject>()) {
        String key=p.key().c_str(); bool known=false;
#define READ(f) if(key==#f) { known=true; if(!assign(p.value(),c.f)) { error="Invalid type: "+key; return false; } }
        // Retired keys migrate on NVS load only; public writes reject them.
        if(!persist&&(key=="ble_enabled"||key=="ble_name"||key=="ble_scan_seconds"||key=="ble_expected_address"))continue;
        CONFIG_FIELDS(READ)
#undef READ
        if(key=="role"||key=="device_role") { known=true; String r=p.value().as<String>(); if(r!="MASTER"&&r!="NODE") {error="Invalid role";return false;} c.role=r=="MASTER"?DeviceRole::MASTER:DeviceRole::NODE; }
        if(key=="device_id"||key=="configuration_version"||key=="configuration_revision"||key=="firmware_version") {
            known=true;
            if(key=="device_id"&&p.value().as<String>()!=c.device_id) { error="Hardware identity is immutable; use node_address";return false; }
            if(key=="configuration_version"&&p.value().as<int>()!=1) { error="Unsupported configuration version";return false; }
            if(!persist&&key=="configuration_revision") c.configuration_revision=p.value().as<uint32_t>();
        }
        if(!known) { error="Unknown configuration field: "+key; return false; }
    }
    // Old configurations predate indicator fields: preserve peripherals instead of rejecting NVS.
    if(!persist){
        if(d["tm1637_enabled"].isUnbound()&&IndicatorCore::pinConflict(c.role==DeviceRole::MASTER,true,c.dht_enabled,c.dht_pin,false,c.lora_enabled,c.lora_rx_pin,c.lora_tx_pin,c.lora_aux_pin,c.lora_m0_pin,c.lora_m1_pin))c.tm1637_enabled=false;
        if(d["server_led_enabled"].isUnbound()&&IndicatorCore::pinConflict(false,false,false,4,true,c.lora_enabled,c.lora_rx_pin,c.lora_tx_pin,c.lora_aux_pin,c.lora_m0_pin,c.lora_m1_pin))c.server_led_enabled=false;
    }
    if(c.ethernet_enabled&&!_config.ethernet_enabled){error="Ethernet driver NOT_AVAILABLE; external module and pins required";return false;}
    if(c.role==DeviceRole::MASTER)c.master_id=c.device_id;
    if(!validate(c,error)) return false;
    DeviceConfig old=_config;
    if(persist) {
        JsonDocument before,after;encode(old,before,true);encode(c,after,true);
        String previous,next;serializeJson(before,previous);serializeJson(after,next);
        if(previous==next)return true;
    }
    if(persist) c.configuration_revision=old.configuration_revision+1;
    _config=c;
    if(persist&&!save()) { _config=old; error="NVS write failed"; return false; }
    return true;
}
bool ConfigManager::save() {
    if(!_ready) return false;
    JsonDocument d; encode(_config,d,true); String data; serializeJson(d,data);
    if(readStoredString("config_v1","")==data) return true;
    return prefs.putString("config_v1",data)==data.length();
}
bool ConfigManager::reset() { return _ready&&prefs.clear(); }
DeviceConfig& ConfigManager::get() { return _config; }
const DeviceConfig& ConfigManager::get() const { return _config; }
bool ConfigManager::setRole(const String& role) { if(role!="MASTER"&&role!="NODE")return false; _config.role=role=="MASTER"?DeviceRole::MASTER:DeviceRole::NODE; return true; }
String ConfigManager::roleString() const { return _config.role==DeviceRole::MASTER?"MASTER":"NODE"; }
String ConfigManager::toJson() const { JsonDocument d;encode(_config,d,false);String s;serializeJson(d,s);return s; }
