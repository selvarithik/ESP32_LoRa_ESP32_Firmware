#pragma once
#include <ArduinoJson.h>
namespace TransportSwitch {
void begin();void loop();bool busy();bool ready();
bool action(JsonObjectConst args,bool gateway,String& result);
void ping(const String& message,const String& channel,JsonDocument& response);
String status();
}
