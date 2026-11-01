#pragma once
#include <stdint.h>
namespace RuntimeIpBackupCore {
inline bool duration(uint32_t seconds){return seconds>=30&&seconds<=180;}
inline uint32_t remaining(uint32_t now,uint32_t deadline,bool armed){int32_t ms=(int32_t)(deadline-now);return armed&&ms>0?(uint32_t(ms)+999)/1000:0;}
}
