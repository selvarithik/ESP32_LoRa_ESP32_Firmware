#pragma once
#include <stdint.h>

namespace E220Readback {
constexpr uint32_t SettleMs=500, AuxTimeoutMs=3000, RetryGapMs=300, ReadTimeoutMs=300;
constexpr unsigned MaxAttempts=4; // Initial read plus three retries.
inline bool settled(uint32_t now,uint32_t rise,uint32_t write,bool sawRise,bool high){
 return sawRise&&high&&uint32_t(rise-write)<AuxTimeoutMs&&uint32_t(now-rise)>=SettleMs;
}
}
