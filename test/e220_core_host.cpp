#include "../src/communication/e220_core.h"
#include "../src/config/hardware_profile.h"
#include <assert.h>
#include <stdio.h>
#include <vector>
#include <algorithm>
#include <random>

static void capturedRssiFraming() {
 // Verbatim payload and RSSI bytes from the two conducted bench captures.
 // The captured probes were ASCII. Wrap only their payloads in offline RV1
 // fragments, then append the original module RSSI byte after the COBS zero.
 const uint8_t captures[][7]={{0x70,0x69,0x6e,0x67,0x30,0x31,0xdf},
                              {0x70,0x69,0x6e,0x67,0x30,0x32,0xe0}};
 const uint8_t sender[6]={1,2,3,4,5,6},target[6]={6,5,4,3,2,1};
 std::vector<uint8_t> wire[2];
 for(unsigned i=0;i<2;++i){
  uint8_t raw[200]{},encoded[202]{};
  size_t length=E220Core::fragment(captures[i],6,i+1,sender,target,152,0,raw);
  assert(length==E220Core::Header+6);
  // The native callback checks the framing boundary and fragment payload.
  // HMAC generation and validation remain in the hardware driver.
  memset(raw+length,0xa5,E220Core::Tag);
  size_t size=E220Core::cobsEncode(raw,length+E220Core::Tag,encoded,sizeof(encoded));
  assert(size&&encoded[size-1]==0);
  wire[i].assign(encoded,encoded+size);
  wire[i].push_back(captures[i][6]);
 }
 for(unsigned scenario=0;scenario<4;++scenario){
  E220Core::ReceiveStream stream;
  unsigned frames=0;
  std::vector<int> signals;
  auto frame=[&](const uint8_t* encoded,size_t size){
   assert(frames<2);
   uint8_t raw[200]{};
   const size_t length=E220Core::cobsDecode(encoded,size,raw,sizeof(raw));
   assert(length==E220Core::Header+6+E220Core::Tag);
   E220Core::Fragment decoded;
   assert(E220Core::parse(raw,length-E220Core::Tag,decoded));
   assert(decoded.transfer==frames+1&&decoded.length==6&&!memcmp(decoded.bytes,captures[frames],6));
   ++frames;
   return true;
  };
  auto signal=[&](int value){signals.push_back(value);};
  auto feed=[&](unsigned index,size_t first,size_t end,uint32_t now){
   for(size_t i=first;i<end;++i)assert(stream.add(wire[index][i],now,true,frame,signal));
  };
  const size_t end=wire[0].size();
  if(scenario==1){
   feed(0,0,end-1,10);
   assert(frames==1&&signals.empty());
   feed(0,end-1,end,11);
  }else if(scenario==2){
   feed(0,0,end/2,10);
   assert(frames==0&&signals.empty());
   feed(0,end/2,end,11);
  }else{
   feed(0,0,end,10);
   if(scenario==3)feed(1,0,wire[1].size(),10);
  }
  assert(frames==(scenario==3?2U:1U));
  assert(signals.size()==frames&&signals[0]==-33);
  if(scenario==3)assert(signals[1]==-32);
 }
 puts("PASS production UART framing: captured RSSI same burst, delayed, split frame and back to back");
}

static void simulatedTwoBoardBench() {
 std::mt19937 random(0xE220);
 const uint8_t boards[2][6]={{0x78,0x21,0x84,0x7d,0x77,0x10},{0x84,0x1f,0xe8,0x2a,0x2f,0x84}};
 const uint16_t packetSizes[]={64,128,200};
 for(uint32_t trial=1;trial<=100;++trial){
  const unsigned origin=trial%2;
  const uint8_t step=E220Core::span(packetSizes[random()%3]);
  std::vector<uint8_t> payload(1+step+random()%1024);
  for(auto& byte:payload)byte=uint8_t(random());
  const unsigned count=(payload.size()+step-1)/step;
  std::vector<unsigned> order(count);
  for(unsigned i=0;i<count;++i)order[i]=i;
  std::shuffle(order.begin(),order.end(),random);
  E220Core::Assembler receiver;
  const auto deliver=[&](unsigned part,uint32_t now){
   uint8_t raw[200]{},wire[202]{},decoded[200]{};
   size_t length=E220Core::fragment(payload.data(),payload.size(),trial,boards[origin],boards[1-origin],step,part,raw);
   assert(length);
   const size_t encoded=E220Core::cobsEncode(raw,length,wire,sizeof(wire));
   assert(encoded&&E220Core::cobsDecode(wire,encoded-1,decoded,sizeof(decoded))==length);
   E220Core::Fragment f;
   assert(E220Core::parse(decoded,length,f));
   assert(!memcmp(f.sender,boards[origin],6)&&!memcmp(f.target,boards[1-origin],6));
   return receiver.add(f,now);
  };
  const unsigned lost=order.back();
  for(unsigned part:order)if(part!=lost){
   assert(!deliver(part,10));
   assert(!deliver(part,11)); // Duplicate delivery must not complete a partial transfer.
  }
  const uint32_t retryAt=12+E220Core::AssemblyLifetime;
  receiver.expire(retryAt);
  assert(receiver.expired==1);
  for(unsigned part:order){
   auto* received=deliver(part,retryAt);
   if(part==lost)assert(received&&received->total==payload.size()&&!memcmp(received->data,payload.data(),payload.size()));
   else assert(!received);
  }
 }
 puts("PASS seeded two-board simulation: 100 transfers, both directions, loss, expiry, duplicates and retry");
}

int main(){
 assert(!E220Core::freshReception(true,false,100,0,1000));
 assert(!E220Core::freshReception(false,true,100,90,1000));
 assert(E220Core::freshReception(true,true,100,90,10));
 assert(!E220Core::freshReception(true,true,101,90,10));
 assert(E220Core::freshReception(true,true,5,UINT32_MAX-4,10));

 assert(HardwareProfile::radioUartPins(true,16,17));assert(HardwareProfile::radioUartPins(false,16,17));
 assert(HardwareProfile::radioUartPins(true,32,33));assert(!HardwareProfile::radioUartPins(false,32,33));
 assert(!HardwareProfile::radioUartPins(true,33,32));assert(!HardwareProfile::radioUartPins(true,3,1));
 assert(!HardwareProfile::radioUartPins(true,16,33));assert(!HardwareProfile::radioUartPins(true,-1,33));

 { // Noisy, partial and invalid public register replies cannot create fake settings.
  const uint8_t valid[]={0xc1,0,6,0,0,0x62,0,18,3};E220Core::SettingsReader reader;E220Core::Settings decoded;
  for(unsigned i=0;i<8;++i)assert(!reader.add(valid[i],decoded));assert(reader.add(valid[8],decoded));assert(decoded.channel==18);
  reader.reset();for(uint8_t byte:{0xff,0xff,0xff,0xc1,0xc1})assert(!reader.add(byte,decoded));
  bool done=false;for(uint8_t byte:valid)done=reader.add(byte,decoded);assert(done&&reader.total==14);
  reader.reset();uint8_t invalid[9];memcpy(invalid,valid,9);invalid[7]=81;
  for(uint8_t byte:invalid)assert(!reader.add(byte,decoded));for(uint8_t byte:valid)done=reader.add(byte,decoded);assert(done);
  reader.reset();for(unsigned i=0;i<10000;++i)assert(!reader.add(0xff,decoded));assert(reader.length==9&&reader.total==10000);
 }

 assert(!HardwareProfile::DisplayPresent);assert(HardwareProfile::Dht==4&&HardwareProfile::Mq135==34&&HardwareProfile::InaSda==21&&HardwareProfile::InaScl==22);
#ifdef DEVICE_ROLE_MASTER
 assert(HardwareProfile::Master&&HardwareProfile::inaAllowed(true)&&HardwareProfile::inaAllowed(false));
#else
 assert(!HardwareProfile::Master&&HardwareProfile::inaAllowed(false));
#endif
 assert(HardwareProfile::roleMatches(true)&&HardwareProfile::roleMatches(false));
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
 capturedRssiFraming();
 simulatedTwoBoardBench();
 puts("PASS production E220 settings/stream resync, COBS 500 vectors, all subpacket sizes, reassembly, duplicates, bounds, expiry, routes and role safety");
}
