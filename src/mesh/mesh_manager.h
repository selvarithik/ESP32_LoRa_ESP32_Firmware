#pragma once
#include <Arduino.h>
#include "../config/config_schema.h"

class MeshManager {
public:
    void begin(const DeviceConfig& config);
    void loop();

    String selectNextHop(const String& destination) const;
    bool isPeerAllowed(const String& peer) const;
    String statusJson() const;

private:
    String _deviceId;
    String _meshId;
    String _masterId;
    String _preferredMaster;
    String _backupMaster;
    String _nodeAddress;
    uint8_t _maxHops = 5;
};

extern MeshManager meshManager;
