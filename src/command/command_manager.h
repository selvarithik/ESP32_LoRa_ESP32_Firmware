#pragma once
#include <Arduino.h>

class CommandManager {
public:
    void begin();
    void loop();

    String createLocalCommand(const String& commandType, const String& payload);
    bool processIncoming(const class Message& message);

    String stateJson() const;
};

extern CommandManager commandManager;
