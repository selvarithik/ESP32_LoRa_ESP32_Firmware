#pragma once
#include <Arduino.h>

class DeviceIdentity {
public:
    bool begin();
    const String& deviceId() const;
    String chipMac() const;

private:
    String _deviceId;
};

extern DeviceIdentity deviceIdentity;
