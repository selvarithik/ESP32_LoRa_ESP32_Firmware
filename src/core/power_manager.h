#pragma once
#include <Arduino.h>
#include "../protocol/protocol.h"
class PowerManager {
public:void begin();void loop(unsigned pending,bool transportIdle);bool draining()const;bool requestRestart(const Message&,String& error);bool replayRestart(const Message&,Message& response)const;bool bootResponse(Message&);void bootResponseQueued();String statusJson()const;
private:String marker;bool ready=false,restarting=false,bootQueued=false;uint32_t requested=0,remaining=0;String state="ACTIVE",lastError;
};
extern PowerManager powerManager;
