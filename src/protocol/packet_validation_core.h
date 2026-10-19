#pragma once
#include <stdint.h>
#include <stddef.h>
namespace PacketValidationCore {
inline uint32_t crc32(const char* data,size_t length){uint32_t crc=~0u;for(size_t i=0;i<length;i++){crc^=(uint8_t)data[i];for(unsigned j=0;j<8;j++)crc=(crc>>1)^(0xedb88320u&-(crc&1));}return ~crc;}
inline bool identity(const char* s,size_t length){if(!length||length>48)return false;for(size_t i=0;i<length;i++){char c=s[i];if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c==':'))return false;}return true;}
}
