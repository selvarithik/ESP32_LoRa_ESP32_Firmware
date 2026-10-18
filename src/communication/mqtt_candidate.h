#pragma once
#include <Arduino.h>
namespace MqttCandidate {
bool start(const String& profile,const String& transaction,const String& digest,uint32_t revision,uint32_t deadline);
void cancel();
bool receive(String& control);
void committed();
void ready();
bool readyFor(const String& transaction);
String status();
}
