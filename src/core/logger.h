#pragma once
#include <Arduino.h>

class Logger {
public:
    static constexpr size_t MAX_ENTRIES = 40;
    static constexpr size_t MAX_TEXT = 160;

    void begin(uint32_t baud = 115200);
    void info(const String& source, const String& message);
    void warn(const String& source, const String& message);
    void error(const String& source, const String& message);
    String getRecentJson() const;

private:
    struct Entry {
        String level;
        String source;
        String message;
        uint32_t ms = 0;
    };

    Entry _entries[MAX_ENTRIES];
    size_t _count = 0;
    size_t _next = 0;

    void write(const String& level, const String& source, const String& message);
};

extern Logger logger;
