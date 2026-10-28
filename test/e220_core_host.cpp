#include "../src/communication/e220_core.h"
#include "../src/config/hardware_profile.h"
#include <assert.h>
#include <stdio.h>
#include <vector>
#include <algorithm>
#include <random>
int main(){
 assert(!HardwareProfile::DisplayPresent);assert(HardwareProfile::Dht==4&&HardwareProfile::Mq135==34&&HardwareProfile::InaSda==21&&HardwareProfile::InaScl==22);
#ifdef DEVICE_ROLE_MASTER
 assert(HardwareProfile::Master&&HardwareProfile::inaAllowed(true)&&!HardwareProfile::inaAllowed(false));
#else
 assert(!HardwareProfile::Master&&!HardwareProfile::inaAllowed(false));
#endif
 assert(HardwareProfile::roleMatches(HardwareProfile::Master)&&!HardwareProfile::roleMatches(!HardwareProfile::Master));
 uint8_t registers[]={0xc1,0,6,0x12,0x34,0x62,0,23,0};E220Core::Settings settings;assert(E220Core::settings(registers,9,settings));assert(settings.address==0x1234&&settings.baud==9600&&settings.packet==200&&settings.channel==23&&settings.airRate==2&&settings.power==22&&!settings.fixed);
 assert(!E220Core::settings(registers,8,settings));registers[7]=81;assert(!E220Core::settings(registers,9,settings));registers[7]=23;registers[5]=0x6a;assert(!E220Core::settings(registers,9,settings));registers[5]=0x62;
 for(unsigned size=1;size<=500;++size){std::vector<uint8_t> input(size),encoded(size+size/254+3),decoded(size);for(unsigned i=0;i<size;++i)input[i]=(i*31)%256;size_t n=E220Core::cobsEncode(input.data(),size,encoded.data(),encoded.size());assert(n&&encoded[n-1]==0);size_t m=E220Core::cobsDecode(encoded.data(),n-1,decoded.data(),decoded.size());assert(m==size&&decoded==input);assert(!E220Core::cobsEncode(input.data(),size,encoded.data(),size));}
 uint8_t sender[6]={1,2,3,4,5,6},target[6]={6,5,4,3,2,1};
 for(unsigned packet: {64U,128U,200U})for(unsigned length:{1U,16U,152U,512U,2048U,3072U}){
  std::vector<uint8_t> input(length);for(unsigned i=0;i<length;++i)input[i]=i%251;
  uint8_t step=E220Core::span(packet),raw[220]{},encoded[222]{};unsigned count=(length+step-1)/step;assert(count<=255);E220Core::Assembler assembly;E220Core::Assembly* done=nullptr;
  for(int part=count-1;part>=0;--part){size_t n=E220Core::fragment(input.data(),length,17,sender,target,step,part,raw);assert(n);size_t wire=E220Core::cobsEncode(raw,n+E220Core::Tag,encoded,sizeof(encoded));assert(wire<=packet);E220Core::Fragment f;assert(E220Core::parse(raw,n,f));auto result=assembly.add(f,100);if(result)done=result;if(part>0){assert(!assembly.add(f,100));}}
  assert(done&&done->total==length&&!memcmp(done->data,input.data(),length));assert(!E220Core::span(32));
 }
 uint8_t data[256]{},raw[220];auto n=E220Core::fragment(data,sizeof(data),1,sender,target,100,0,raw);E220Core::Fragment fragment;assert(E220Core::parse(raw,n,fragment));E220Core::Assembler bounded;assert(!bounded.add(fragment,0));raw[4]=2;assert(E220Core::parse(raw,n,fragment));bounded.add(fragment,0);raw[4]=3;assert(E220Core::parse(raw,n,fragment));bounded.add(fragment,0);assert(bounded.rejected==1);bounded.expire(120001);assert(bounded.expired==2);
 raw[21]=0;assert(!E220Core::parse(raw,n,fragment));raw[0]=0;assert(!E220Core::parse(raw,n,fragment));
 E220Core::Routes routes;assert(routes.observe("ESP32-010203040506","ESP32-060504030201",1,0));assert(routes.find("ESP32-010203040506",59999,60000));assert(!routes.find("ESP32-010203040506",60001,60000));routes.expire(60001,60000);assert(!routes.rows[0].destination[0]);assert(!routes.observe("bad id","peer",1,0));
 assert(E220Core::forwardAllowed(3,1,4,"RADIO:next","RADIO:previous"));assert(!E220Core::forwardAllowed(1,1,4,"RADIO:next","RADIO:previous"));assert(!E220Core::forwardAllowed(3,4,4,"RADIO:next","RADIO:previous"));assert(!E220Core::forwardAllowed(3,1,4,"RADIO:same","RADIO:same"));
 assert(routes.observe("target","direct",1,0));assert(routes.observe("target","relay",2,1));assert(!strcmp(routes.find("target",2,60000)->nextHop,"direct"));
 puts("PASS production E220 settings, COBS 500 vectors, all subpacket sizes, reassembly, duplicates, bounds, expiry, routes and role safety");
}
