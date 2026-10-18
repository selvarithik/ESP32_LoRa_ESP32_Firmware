#include "watchdog_manager.h"
#include <esp_task_wdt.h>
#include "../core/logger.h"

WatchdogManager watchdogManager;

bool WatchdogManager::begin(uint32_t timeoutSeconds)
{
    if (timeoutSeconds == 0) {
        timeoutSeconds = 10;
    }

    esp_err_t result = esp_task_wdt_init(
        timeoutSeconds,
        true
    );

    if (result == ESP_OK || result == ESP_ERR_INVALID_STATE) {

        esp_err_t addResult = esp_task_wdt_add(NULL);

        _enabled =
            (addResult == ESP_OK ||
             addResult == ESP_ERR_INVALID_STATE);

    } else {

        _enabled = false;
    }

    if (_enabled) {
        logger.info(
            "WATCHDOG",
            "Task watchdog initialized"
        );
    } else {
        logger.warn(
            "WATCHDOG",
            "Task watchdog unavailable"
        );
    }

    return _enabled;
}

void WatchdogManager::feed()
{
    if (_enabled) {
        esp_task_wdt_reset();
    }
}

bool WatchdogManager::isEnabled() const
{
    return _enabled;
}
