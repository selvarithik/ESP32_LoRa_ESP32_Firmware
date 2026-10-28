#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "../protocol/packet_validation_core.h"
namespace E220Core {
constexpr size_t MaxMessage=3072,Header=30,Tag=16,MaxFrame=200;
constexpr uint32_t AssemblyLifetime=120000;
inline void put16(uint8_t* p,uint16_t v){p[0]=v;p[1]=v>>8;}
inline uint16_t get16(const uint8_t* p){return uint16_t(p[0])|uint16_t(p[1])<<8;}
inline void put32(uint8_t* p,uint32_t v){for(unsigned i=0;i<4;++i)p[i]=v>>(i*8);}
inline uint32_t get32(const uint8_t* p){uint32_t v=0;for(unsigned i=0;i<4;++i)v|=uint32_t(p[i])<<(i*8);return v;}
inline size_t cobsEncode(const uint8_t* in,size_t n,uint8_t* out,size_t cap){if(!out||cap<n+n/254+2)return 0;size_t codeAt=0,write=1;uint8_t code=1;for(size_t i=0;i<n;++i){if(!in[i]){out[codeAt]=code;code=1;codeAt=write++;}else{out[write++]=in[i];if(++code==255){out[codeAt]=code;code=1;codeAt=write++;}}}out[codeAt]=code;out[write++]=0;return write;}
inline size_t cobsDecode(const uint8_t* in,size_t n,uint8_t* out,size_t cap){size_t r=0,w=0;while(r<n){uint8_t code=in[r++];if(!code||r+code-1>n)return 0;for(unsigned i=1;i<code;++i){if(w>=cap||!in[r])return 0;out[w++]=in[r++];}if(code!=255&&r<n){if(w>=cap)return 0;out[w++]=0;}}return w;}
struct Settings {uint16_t address=0;uint32_t baud=0;uint8_t airRate=0,channel=0,power=0;uint16_t packet=0;bool fixed=false,rssi=false,lbt=false;};
inline bool settings(const uint8_t* bytes,size_t n,Settings& out){if(n!=9||bytes[0]!=0xc1||bytes[1]!=0||bytes[2]!=6)return false;const uint8_t* p=bytes+3;static const uint32_t baud[]={1200,2400,4800,9600,19200,38400,57600,115200};static const uint16_t lengths[]={200,128,64,32};static const uint8_t power[]={22,17,13,10};if((((p[2]>>3)&3)==1||((p[2]>>3)&3)==2)||p[4]>80)return false;out.address=uint16_t(p[0])<<8|p[1];out.baud=baud[p[2]>>5];out.airRate=p[2]&7;out.packet=lengths[p[3]>>6];out.power=power[p[3]&3];out.channel=p[4];out.rssi=p[5]&128;out.fixed=p[5]&64;out.lbt=p[5]&16;return true;}
inline uint8_t span(uint16_t packet){return packet>=64&&packet<=200?uint8_t(packet-2-Header-Tag):0;}
struct Fragment {uint32_t transfer=0,crc=0;uint8_t sender[6]{},target[6]{},index=0,count=0,step=0,length=0;uint16_t total=0;const uint8_t* bytes=nullptr;};
inline size_t fragment(const uint8_t* message,size_t total,uint32_t transfer,const uint8_t* sender,const uint8_t* target,uint8_t step,uint8_t index,uint8_t* out){if(!step||total==0||total>MaxMessage)return 0;size_t count=(total+step-1)/step;if(count>255||index>=count)return 0;size_t size=total-size_t(index)*step;if(size>step)size=step;out[0]=0xe2;out[1]=0x7a;out[2]=1;out[3]=0;put32(out+4,transfer);memcpy(out+8,sender,6);memcpy(out+14,target,6);out[20]=index;out[21]=count;put16(out+22,total);out[24]=size;out[25]=step;put32(out+26,PacketValidationCore::crc32((const char*)message,total));memcpy(out+Header,message+size_t(index)*step,size);return Header+size;}
// HMAC must be verified by the caller before admitting the parsed fragment.
inline bool parse(const uint8_t* in,size_t n,Fragment& f){if(n<Header+1||in[0]!=0xe2||in[1]!=0x7a||in[2]!=1||in[3])return false;f.transfer=get32(in+4);memcpy(f.sender,in+8,6);memcpy(f.target,in+14,6);f.index=in[20];f.count=in[21];f.total=get16(in+22);f.length=in[24];f.step=in[25];f.crc=get32(in+26);f.bytes=in+Header;if(!f.step||f.total==0||f.total>MaxMessage||!f.count||f.index>=f.count||n!=Header+f.length||size_t(f.index)*f.step>=f.total||f.count!=(f.total+f.step-1)/f.step)return false;size_t wanted=f.total-size_t(f.index)*f.step;if(wanted>f.step)wanted=f.step;return f.length==wanted;}
struct Assembly {bool used=false;uint8_t sender[6]{},target[6]{},count=0,step=0,received[32]{};uint16_t total=0,parts=0;uint32_t transfer=0,crc=0,started=0;uint8_t data[MaxMessage]{};};
class Assembler {
public:
 Assembly entries[2];uint32_t rejected=0,expired=0,duplicates=0;
 void expire(uint32_t now){for(auto& e:entries)if(e.used&&uint32_t(now-e.started)>AssemblyLifetime){e.used=false;++expired;}}
 Assembly* add(const Fragment& f,uint32_t now){expire(now);Assembly* item=nullptr;for(auto& e:entries)if(e.used&&e.transfer==f.transfer&&!memcmp(e.sender,f.sender,6)){item=&e;break;}if(!item)for(auto& e:entries)if(!e.used){item=&e;break;}if(!item){++rejected;return nullptr;}auto& e=*item;
  if(!e.used){e.used=true;e.transfer=f.transfer;e.crc=f.crc;e.total=f.total;e.count=f.count;e.step=f.step;e.parts=0;e.started=now;memset(e.received,0,sizeof(e.received));memcpy(e.sender,f.sender,6);memcpy(e.target,f.target,6);}
  if(e.crc!=f.crc||e.total!=f.total||e.count!=f.count||e.step!=f.step||memcmp(e.target,f.target,6)){e.used=false;++rejected;return nullptr;}
  uint8_t mask=1<<(f.index%8);size_t offset=size_t(f.index)*f.step;if(e.received[f.index/8]&mask){if(memcmp(e.data+offset,f.bytes,f.length)){e.used=false;++rejected;}else ++duplicates;return nullptr;}
  memcpy(e.data+offset,f.bytes,f.length);e.received[f.index/8]|=mask;++e.parts;
  if(e.parts!=e.count)return nullptr;
  if(PacketValidationCore::crc32((const char*)e.data,e.total)!=e.crc){e.used=false;++rejected;return nullptr;}return &e;
 }
};
inline bool forwardAllowed(uint8_t ttl,uint8_t hops,uint8_t maximum,const char* next,const char* incoming){return ttl>1&&(!maximum||hops<maximum)&&next&&next[0]&&incoming&&strcmp(next,incoming);}
struct Route {char destination[49]{},nextHop[49]{};uint32_t seen=0;uint8_t hops=0;};
class Routes {
public:Route rows[12];
 bool observe(const char* destination,const char* via,uint8_t hops,uint32_t now,uint32_t age=AssemblyLifetime){if(!PacketValidationCore::identity(destination,strlen(destination))||!PacketValidationCore::identity(via,strlen(via))||hops>15)return false;Route* selected=nullptr;for(auto& r:rows)if(!strcmp(r.destination,destination)){if(strcmp(r.nextHop,via)&&r.hops<=hops&&uint32_t(now-r.seen)<=age)return true;selected=&r;break;}if(!selected)for(auto& r:rows)if(!r.destination[0]){selected=&r;break;}if(!selected)return false;strcpy(selected->destination,destination);strcpy(selected->nextHop,via);selected->hops=hops;selected->seen=now;return true;}
 const Route* find(const char* destination,uint32_t now,uint32_t age)const{for(auto& r:rows)if(r.destination[0]&&!strcmp(r.destination,destination)&&uint32_t(now-r.seen)<=age)return &r;return nullptr;}
 void expire(uint32_t now,uint32_t age){for(auto& r:rows)if(r.destination[0]&&uint32_t(now-r.seen)>age)r=Route{};}
};
}
