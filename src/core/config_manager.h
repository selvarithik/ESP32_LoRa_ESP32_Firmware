#pragma once
#include "../config/config_schema.h"

class ConfigManager {
public:
    bool begin();
    bool save();
    bool reset();

    DeviceConfig& get();
    const DeviceConfig& get() const;

    bool setRole(const String& role);
    String roleString() const;
    String toJson() const;
    bool update(const String& json, String& error, bool persist = true);
    bool validate(const DeviceConfig& config, String& error) const;

private:
    DeviceConfig _config;
    bool _ready = false;

    void loadString(const char* key, String& target, const String& fallback);
    void loadUInt(const char* key, uint32_t& target, uint32_t fallback);
    void loadBool(const char* key, bool& target, bool fallback);
};

extern ConfigManager configManager;
