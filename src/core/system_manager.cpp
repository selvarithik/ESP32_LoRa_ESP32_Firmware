#include "system_manager.h"
#include <esp_system.h>
#include "../core/logger.h"

SystemManager systemManager;

void SystemManager::begin() {
    logger.info("SYSTEM", "System manager initialized");
    logger.info("SYSTEM", String("Reset reason: ") + resetReason());
}

uint32_t SystemManager::uptimeSeconds() const {
    return millis() / 1000UL;
}

String SystemManager::resetReason() const {
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON: return "POWERON";
        case ESP_RST_EXT: return "EXTERNAL";
        case ESP_RST_SW: return "SOFTWARE";
        case ESP_RST_PANIC: return "PANIC";
        case ESP_RST_INT_WDT: return "INT_WDT";
        case ESP_RST_TASK_WDT: return "TASK_WDT";
        case ESP_RST_WDT: return "WDT";
        case ESP_RST_BROWNOUT: return "BROWNOUT";
        case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
        default: return "OTHER";
    }
}

bool SystemManager::restart() {
    logger.warn("SYSTEM", "Restart requested");
    delay(100);
    ESP.restart();
    return true;
}
