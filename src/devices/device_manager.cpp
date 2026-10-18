#include "device_manager.h"
#include <ArduinoJson.h>

DeviceManager deviceManager;

void DeviceManager::begin() {
}

String DeviceManager::capabilitiesJson() const {
    JsonDocument doc;
    JsonArray caps = doc["capabilities"].to<JsonArray>();
    caps.add("ESP32");
    caps.add("WIFI");
    caps.add("COMMAND");
    caps.add("ACK");
    caps.add("RESPONSE");
    caps.add("DIAGNOSTICS");
    caps.add("LOCAL_WEB");
    caps.add("DHT11_OPTIONAL");
    doc["lora_status"]="NOT_AVAILABLE";
    doc["ethernet_status"]="NOT_AVAILABLE";
    doc["ble_status"]="NOT_AVAILABLE";

    String out;
    serializeJson(doc, out);
    return out;
}
