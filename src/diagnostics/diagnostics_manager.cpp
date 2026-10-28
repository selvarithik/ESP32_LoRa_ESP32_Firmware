#include "../config/firmware_version.h"
#include "diagnostics_manager.h"
#include "../identity/device_identity.h"
#include "../core/config_manager.h"
#include "../core/system_manager.h"
#include "../core/watchdog_manager.h"
#include "../core/network_service.h"
#include "../core/logger.h"
#include "../communication/wifi_manager.h"
#include "../communication/transport_manager.h"
#include "../sensors/sensor_manager.h"
#include <ArduinoJson.h>
#include "../core/power_manager.h"
#include "../core/connection_indicators.h"
#include "../communication/lora_transport.h"
#include "../config/hardware_profile.h"
DiagnosticsManager diagnosticsManager;
void DiagnosticsManager::begin(){}
String DiagnosticsManager::statusJson()const {
    JsonDocument d;auto& c=configManager.get();auto dev=d["device"].to<JsonObject>();
    dev["id"]=c.device_id;dev["name"]=c.device_name;dev["role"]=configManager.roleString();dev["master_id"]=c.master_id;dev["enabled"]=c.enabled;
    dev["build_profile"]=HardwareProfile::ExplicitRole?(HardwareProfile::Master?"MASTER":"SLAVE"):"LEGACY_NVS";dev["build_role_matches_stored"]=HardwareProfile::roleMatches(c.role==DeviceRole::MASTER);dev["display_inhibited_by_profile"]=!HardwareProfile::DisplayPresent;dev["firmware_version"]=FIRMWARE_VERSION;dev["configuration_revision"]=c.configuration_revision;dev["heap_free"]=ESP.getFreeHeap();dev["heap_min"]=ESP.getMinFreeHeap();dev["uptime_s"]=systemManager.uptimeSeconds();dev["reset_reason"]=systemManager.resetReason();dev["watchdog"]=watchdogManager.isEnabled();
    JsonDocument w,s,n,l;deserializeJson(w,wifiManager.statusJson());deserializeJson(s,sensorManager.telemetryJson());deserializeJson(n,networkService.statusJson());deserializeJson(l,logger.getRecentJson());
    d["wifi"]=w.as<JsonVariant>();d["sensor"]=s.as<JsonVariant>();JsonDocument health;deserializeJson(health,sensorManager.healthJson());d["sensors"]=health;d["network"]=n.as<JsonVariant>();d["recent_logs"]=l.as<JsonVariant>();
    d["gateway_reachable"]=transportManager.gatewayConnected();d["mqtt_connected"]=transportManager.mqttConnected();d["internet_status"]=transportManager.internetStatus();d["transport"]=transportManager.mqttConnected()?"MQTT":"HTTP";d["last_success_uptime_ms"]=transportManager.lastSuccess();d["queue_overflows"]=transportManager.dropped();
    JsonDocument power;deserializeJson(power,powerManager.statusJson());d["power"]=power;
    JsonDocument indicators;deserializeJson(indicators,connectionIndicators.statusJson());d["indicators"]=indicators;
    d["ethernet_status"]="NOT_AVAILABLE";d["lora_status"]=loraTransport.isInitialized()?"READY_LINK_UNVERIFIED":"DISABLED_OR_ERROR";JsonDocument radio;deserializeJson(radio,loraTransport.statusJson());d["radio"]=radio;
    d["last_http_status_or_error"]=transportManager.lastError();JsonDocument local;deserializeJson(local,transportManager.localStatus());d["local_ip"]=local;
    String out;serializeJson(d,out);return out;
}
