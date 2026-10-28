#include "device_identity.h"
#include "../core/config_manager.h"
#include <esp_system.h>
#include <esp_mac.h>
#include "../config/hardware_profile.h"

DeviceIdentity deviceIdentity;

bool DeviceIdentity::begin() {
    DeviceConfig& c = configManager.get();

    if (c.device_id.length() == 0) {
        uint64_t mac = ESP.getEfuseMac();
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "ESP32-%012llX",
                 static_cast<unsigned long long>(mac));
        c.device_id = buffer;
        configManager.save();
    }

    _deviceId = c.device_id;
    char legacy[32];snprintf(legacy,sizeof(legacy),"ESP32-%012llX",static_cast<unsigned long long>(ESP.getEfuseMac()));
    return !HardwareProfile::ExplicitRole||c.device_id.equalsIgnoreCase(legacy)||c.device_id.equalsIgnoreCase(chipMac());
}

const String& DeviceIdentity::deviceId() const {
    return _deviceId;
}

String DeviceIdentity::chipMac() const {
    uint8_t mac[6];esp_read_mac(mac,ESP_MAC_WIFI_STA);char buffer[18];
    snprintf(buffer,sizeof(buffer),"%02X:%02X:%02X:%02X:%02X:%02X",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    return String(buffer);
}
