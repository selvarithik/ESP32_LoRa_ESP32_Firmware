#pragma once
#include <stdint.h>

namespace RadioRssi {
constexpr int dbm(uint8_t value){return int(value)-256;}
constexpr bool available(uint8_t value){return dbm(value)>=-141&&dbm(value)<=-11;}
}
