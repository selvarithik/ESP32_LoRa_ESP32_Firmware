#pragma once
#include <Arduino.h>

class CommandExecutor {
public:
    void begin();
    String execute(const String& command, const String& payload);
};

extern CommandExecutor commandExecutor;
