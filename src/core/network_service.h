#pragma once
#include <Arduino.h>
#include "../protocol/protocol.h"
class NetworkService {
public:
    void begin();
    void loop();
    String command(const String& destination,const String& type,const String& payload);
    String statusJson() const;
    bool accept(const Message& message,const String& remote,bool gateway=false);
};
extern NetworkService networkService;
