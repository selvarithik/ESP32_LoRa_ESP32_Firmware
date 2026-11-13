#pragma once
#include <stdint.h>

namespace RadioRssi {
constexpr bool available(uint8_t value){return value!=0;}
constexpr int dbm(uint8_t value){return int(value)-256;} // Call only for an available sample.
}
