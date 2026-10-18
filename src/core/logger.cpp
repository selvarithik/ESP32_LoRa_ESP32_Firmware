#include "logger.h"
#include <ArduinoJson.h>

Logger logger;

void Logger::begin(uint32_t baud) {
    Serial.begin(baud);
    delay(50);
    info("SYSTEM", "Logger initialized");
}

void Logger::write(const String& level, const String& source, const String& message) {
    Entry& e = _entries[_next];
    e.level = level;
    e.source = source;
    e.message = message.substring(0, MAX_TEXT - 1);
    e.ms = millis();

    _next = (_next + 1) % MAX_ENTRIES;
    if (_count < MAX_ENTRIES) {
        ++_count;
    }

    Serial.printf("[%s][%s][%lu] %s\n",
                  level.c_str(),
                  source.c_str(),
                  static_cast<unsigned long>(e.ms),
                  e.message.c_str());
}

void Logger::info(const String& source, const String& message) {
    write("INFO", source, message);
}

void Logger::warn(const String& source, const String& message) {
    write("WARN", source, message);
}

void Logger::error(const String& source, const String& message) {
    write("ERROR", source, message);
}

String Logger::getRecentJson() const {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    size_t start = (_count < MAX_ENTRIES) ? 0 : _next;
    for (size_t i = 0; i < _count; ++i) {
        size_t index = (_count < MAX_ENTRIES)
            ? i
            : (start + i) % MAX_ENTRIES;

        JsonObject item = arr.add<JsonObject>();
        item["level"] = _entries[index].level;
        item["source"] = _entries[index].source;
        item["message"] = _entries[index].message;
        item["ms"] = _entries[index].ms;
    }

    String out;
    serializeJson(arr, out);
    return out;
}
