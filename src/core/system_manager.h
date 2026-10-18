#pragma once
#include <Arduino.h>

class SystemManager {
public:
    void begin();
    uint32_t uptimeSeconds() const;
    String resetReason() const;
    bool restart();
};

extern SystemManager systemManager;
