#pragma once
#include <Arduino.h>
#include "../protocol/protocol.h"
class PowerManager {
public:bool requestSleep(uint32_t seconds,const String& mode,String& error);bool cancelPendingSleep(String& error);void begin();void loop(unsigned pending,bool transportIdle);bool draining()const;bool requestRestart(const Message&,String& error);bool replayRestart(const Message&,Message& response)const;bool bootResponse(Message&);void bootResponseQueued();void resultPersisted(const String& responseId);String statusJson()const;
private:bool manualSleep=false;uint32_t manualAt=0,manualDuration=0;String manualMode="DEEP";String marker;bool sleeping=false,ready=false,restarting=false,bootQueued=false;uint32_t requested=0,remaining=0;String state="ACTIVE",lastError;
};
extern PowerManager powerManager;
