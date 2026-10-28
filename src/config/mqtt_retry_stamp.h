#pragma once
#include <stdint.h>
namespace MqttRetryStamp {
// Re-sending the same packet ID must preserve its envelope timestamp.
inline uint32_t select(uint32_t now,bool packet,uint32_t original){return packet?original:now;}
}
