#pragma once
#include "e220_core.h"

namespace CompactRadio {
constexpr uint8_t Version=3;
enum Kind:uint8_t {Register=1, Poll, Ping, Ack, Result, Ina219, Idle, Config};
constexpr size_t Header=22;
inline size_t payloadSize(uint8_t kind){
 switch(kind){
 case Register:return 2;
 case Poll:return 1;
 case Ping:return 10;
 case Ack:return 7;
 case Result:return 9;
 case Ina219:return 10;
 case Idle:return 0;
 case Config:return 6;
 default:return SIZE_MAX;
 }
}
struct Frame {
 uint8_t kind=0;
 uint8_t source[6]{},destination[6]{},data[10]{};
 uint32_t token=0;
};
inline size_t pack(const Frame& f,uint8_t* out){
 const size_t n=payloadSize(f.kind);
 if(n==SIZE_MAX||!f.token)return 0;
 out[0]=0xe2;out[1]=0x7a;out[2]=Version;out[3]=0;
 out[4]=f.kind;out[5]=0;
 memcpy(out+6,f.source,6);memcpy(out+12,f.destination,6);
 E220Core::put32(out+18,f.token);
 memcpy(out+22,f.data,n);
 return 22+n;
}
inline bool unpack(const uint8_t* raw,size_t len,Frame& f){
 if(len<22||raw[0]!=0xe2||raw[1]!=0x7a||raw[2]!=Version||raw[3]||raw[5])return false;
 const size_t n=payloadSize(raw[4]);
 if(n==SIZE_MAX||len!=22+n)return false;
 f.token=E220Core::get32(raw+18);if(!f.token)return false;
 f.kind=raw[4];memcpy(f.source,raw+6,6);memcpy(f.destination,raw+12,6);
 memcpy(f.data,raw+22,n);
 return true;
}
inline bool validTag(const uint8_t* actual,const uint8_t* expected){
 uint8_t different=0;
 for(size_t i=0;i<E220Core::Tag;++i)different|=actual[i]^expected[i];
 return different==0;
}
inline bool addressed(const Frame& frame,const uint8_t* source,const uint8_t* destination){
 return !memcmp(frame.source,source,6)&&!memcmp(frame.destination,destination,6);
}
}

