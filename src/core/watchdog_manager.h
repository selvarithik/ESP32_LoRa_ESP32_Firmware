#pragma once
#include <Arduino.h>

class WatchdogManager {
public:
    bool begin(uint32_t timeoutSeconds = 10);
    void feed();
    bool isEnabled() const;

private:
    bool _enabled = false;
};

extern WatchdogManager watchdogManager;
