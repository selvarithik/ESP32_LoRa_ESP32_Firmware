#include <Arduino.h>
#include "core/logger.h"
#include "core/commissioning.h"
#include "config/mqtt_staging.h"
#include "core/config_manager.h"
#include "core/system_manager.h"
#include "core/watchdog_manager.h"
#include "core/network_service.h"
#include "identity/device_identity.h"
#include "communication/wifi_manager.h"
#include "communication/transport_manager.h"
#include "mesh/mesh_manager.h"
#include "sensors/sensor_manager.h"
#include "web/local_web_server.h"
#include "roles/master.h"
#include "roles/node.h"
#include "core/power_manager.h"
#include "core/connection_indicators.h"
#include "communication/lora_transport.h"
#include "config/hardware_profile.h"
#include "config/transport_trial.h"
namespace {bool roleReady=false,identityReady=false;}

void setup() {
    logger.begin(115200);
    if(!configManager.begin())logger.error("CONFIG","NVS unavailable; restart required before commissioning");
    mqttStaging.begin();TransportSwitch::begin();Commissioning::begin();
    identityReady=deviceIdentity.begin();systemManager.begin();watchdogManager.begin(10);
    logger.info("IDENTITY","Device ID: "+deviceIdentity.deviceId()+" Role: "+configManager.roleString());
    connectionIndicators.begin();
    wifiManager.begin(configManager.get());meshManager.begin(configManager.get());
    roleReady=identityReady&&HardwareProfile::roleMatches(configManager.get().role==DeviceRole::MASTER);
    loraTransport.begin(configManager.get());sensorManager.begin();
    if(roleReady){transportManager.begin();if(configManager.get().role==DeviceRole::MASTER)masterRole.begin();else nodeRole.begin();}
    else logger.error("CONFIG","Build/stored-role or hardware identity mismatch; local recovery only. NVS preserved.");
    powerManager.begin();localWebServer.begin();
    logger.info("SYSTEM","Device services ready; inspect radio status for enablement/settings (Ethernet unavailable)");
}
void loop() {
    watchdogManager.feed();TransportSwitch::loop();Commissioning::loop();mqttStaging.loop();wifiManager.loop();transportManager.loop();
    if(roleReady){if(configManager.get().role==DeviceRole::MASTER)masterRole.loop();else nodeRole.loop();}
    connectionIndicators.loop();localWebServer.loop();delay(2);
}
