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

void setup() {
    logger.begin(115200);
    if(!configManager.begin())logger.error("CONFIG","NVS unavailable; restart required before commissioning");
    mqttStaging.begin();Commissioning::begin();
    deviceIdentity.begin();systemManager.begin();watchdogManager.begin(10);
    logger.info("IDENTITY","Device ID: "+deviceIdentity.deviceId()+" Role: "+configManager.roleString());
    wifiManager.begin(configManager.get());meshManager.begin(configManager.get());
    transportManager.begin();sensorManager.begin();
    if(configManager.get().role==DeviceRole::MASTER)masterRole.begin();else nodeRole.begin();
    localWebServer.begin();
    logger.info("SYSTEM","Part 1 Wi-Fi services ready; E220/Ethernet/BLE drivers NOT_AVAILABLE");
}
void loop() {
    watchdogManager.feed();Commissioning::loop();mqttStaging.loop();wifiManager.loop();
    if(configManager.get().role==DeviceRole::MASTER)masterRole.loop();else nodeRole.loop();
    localWebServer.loop();delay(2);
}
