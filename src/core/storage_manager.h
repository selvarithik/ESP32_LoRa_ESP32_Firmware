#pragma once
#include <Arduino.h>
#include <Preferences.h>

class StorageManager {
public:
    bool begin(const char* ns = "system");
    bool putString(const char* key, const String& value);
    String getString(const char* key, const String& fallback = "");
    bool putUInt(const char* key, uint32_t value);
    uint32_t getUInt(const char* key, uint32_t fallback = 0);
    bool putBool(const char* key, bool value);
    bool getBool(const char* key, bool fallback = false);
    bool clear();
    void end();

private:
    Preferences _prefs;
    bool _ready = false;
};
