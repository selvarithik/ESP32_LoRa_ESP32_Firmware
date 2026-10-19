#pragma once
#include "packet_validation_core.h"
#include <string.h>
namespace RadioFrame {
// Binary datagram v1, explicitly software-only until the E220 SKU is verified.
constexpr size_t MaxPayload=128,MaxBytes=240;
struct Frame {uint8_t type=0,ttl=5;uint32_t sequence=0;char source[49]={0},destination[49]={0};uint8_t payload[MaxPayload];uint16_t length=0;};
inline void write32(uint8_t* p,uint32_t n){for(int i=0;i<4;++i)p[i]=(n>>(8*i))&255;}
inline uint32_t read32(const uint8_t* p){uint32_t n=0;for(int i=0;i<4;++i)n|=uint32_t(p[i])<<(8*i);return n;}
inline size_t encode(const Frame& f,uint8_t* out,size_t max){size_t a=strnlen(f.source,49),b=strnlen(f.destination,49),n=16+a+b+f.length;if(!PacketValidationCore::identity(f.source,a)||!PacketValidationCore::identity(f.destination,b)||!f.type||f.type>10||f.ttl<1||f.ttl>15||f.length>MaxPayload||n>max)return 0;
 out[0]=0xE2;out[1]=1;out[2]=f.type;out[3]=f.ttl;write32(out+4,f.sequence);out[8]=a;out[9]=b;out[10]=f.length&255;out[11]=f.length>>8;memcpy(out+12,f.source,a);memcpy(out+12+a,f.destination,b);memcpy(out+12+a+b,f.payload,f.length);write32(out+n-4,PacketValidationCore::crc32((const char*)out,n-4));return n;}
inline bool decode(const uint8_t* in,size_t n,Frame& f){if(!in||n<18||n>MaxBytes||in[0]!=0xE2||in[1]!=1||!in[2]||in[2]>10||in[3]<1||in[3]>15)return false;size_t a=in[8],b=in[9],length=in[10]|uint16_t(in[11])<<8;if(length>MaxPayload||n!=16+a+b+length||!PacketValidationCore::identity((const char*)in+12,a)||!PacketValidationCore::identity((const char*)in+12+a,b)||read32(in+n-4)!=PacketValidationCore::crc32((const char*)in,n-4))return false;f=Frame{};f.type=in[2];f.ttl=in[3];f.sequence=read32(in+4);memcpy(f.source,in+12,a);memcpy(f.destination,in+12+a,b);f.length=length;memcpy(f.payload,in+12+a+b,length);return true;}
}
