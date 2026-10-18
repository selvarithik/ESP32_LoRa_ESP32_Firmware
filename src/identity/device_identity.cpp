#include "device_identity.h"
#include "../core/config_manager.h"
#include <esp_system.h>

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
    return true;
}

const String& DeviceIdentity::deviceId() const {
    return _deviceId;
}

String DeviceIdentity::chipMac() const {
    uint64_t mac = ESP.getEfuseMac();
    char buffer[24];
    snprintf(buffer, sizeof(buffer), "%04X%08X",
             static_cast<uint16_t>(mac >> 32),
             static_cast<uint32_t>(mac & 0xFFFFFFFFULL));
    return String(buffer);
}
