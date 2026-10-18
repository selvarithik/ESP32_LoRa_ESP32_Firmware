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
DiagnosticsManager diagnosticsManager;
void DiagnosticsManager::begin(){}
String DiagnosticsManager::statusJson()const {
    JsonDocument d;auto& c=configManager.get();auto dev=d["device"].to<JsonObject>();
    dev["id"]=c.device_id;dev["name"]=c.device_name;dev["role"]=configManager.roleString();dev["master_id"]=c.master_id;dev["enabled"]=c.enabled;
    dev["firmware_version"]="1.0.0-part1";dev["configuration_revision"]=c.configuration_revision;dev["heap_free"]=ESP.getFreeHeap();dev["heap_min"]=ESP.getMinFreeHeap();dev["uptime_s"]=systemManager.uptimeSeconds();dev["reset_reason"]=systemManager.resetReason();dev["watchdog"]=watchdogManager.isEnabled();
    JsonDocument w,s,n,l;deserializeJson(w,wifiManager.statusJson());deserializeJson(s,sensorManager.telemetryJson());deserializeJson(n,networkService.statusJson());deserializeJson(l,logger.getRecentJson());
    d["wifi"]=w.as<JsonVariant>();d["sensor"]=s.as<JsonVariant>();d["network"]=n.as<JsonVariant>();d["recent_logs"]=l.as<JsonVariant>();
    d["gateway_reachable"]=transportManager.gatewayConnected();d["mqtt_connected"]=transportManager.mqttConnected();d["internet_status"]=transportManager.internetStatus();d["transport"]=transportManager.mqttConnected()?"MQTT":"HTTP";d["last_success_uptime_ms"]=transportManager.lastSuccess();d["queue_overflows"]=transportManager.dropped();
    d["ethernet_status"]="NOT_AVAILABLE";d["ble_status"]="NOT_AVAILABLE";d["lora_status"]="NOT_AVAILABLE";
    d["last_http_status_or_error"]=transportManager.lastError();
    String out;serializeJson(d,out);return out;
}
