#pragma once
#include "../protocol/protocol.h"
namespace CanonicalMqtt {
void begin(const String& raw,uint32_t sharedRevision=0);
bool controlReply(const String& raw);
void loop();
void requestEnabled(bool enabled);
bool connected();
bool configured();
bool publish(const Message& packet);
String status();
}
