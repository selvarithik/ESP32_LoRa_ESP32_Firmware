#include "storage_manager.h"

bool StorageManager::begin(const char* ns) {
    _ready = _prefs.begin(ns, false);
    return _ready;
}

bool StorageManager::putString(const char* key, const String& value) {
    return _ready && _prefs.putString(key, value) > 0;
}

String StorageManager::getString(const char* key, const String& fallback) {
    return _ready ? _prefs.getString(key, fallback) : fallback;
}

bool StorageManager::putUInt(const char* key, uint32_t value) {
    return _ready && _prefs.putUInt(key, value) > 0;
}

uint32_t StorageManager::getUInt(const char* key, uint32_t fallback) {
    return _ready ? _prefs.getUInt(key, fallback) : fallback;
}

bool StorageManager::putBool(const char* key, bool value) {
    return _ready && _prefs.putBool(key, value) > 0;
}

bool StorageManager::getBool(const char* key, bool fallback) {
    return _ready ? _prefs.getBool(key, fallback) : fallback;
}

bool StorageManager::clear() {
    return _ready && _prefs.clear();
}

void StorageManager::end() {
    _prefs.end();
    _ready = false;
}
