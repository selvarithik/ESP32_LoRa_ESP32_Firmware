#pragma once
#include <Arduino.h>

class DeviceManager {
public:
    void begin();
    String capabilitiesJson() const;
};

extern DeviceManager deviceManager;
