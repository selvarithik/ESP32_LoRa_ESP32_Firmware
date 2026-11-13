#include "../src/communication/compact_radio.h"
#include "../src/communication/e220_core.h"
#include "../src/config/hardware_profile.h"
#include <assert.h>
#include <stdio.h>
#include <vector>
#include <algorithm>
#include <random>
#include "../src/communication/radio_turn.h"
#include "../src/communication/radio_airtime.h"
#include "../src/communication/e220_readback.h"

static void rollingAirtimeAndFragmentCap(){
 assert(!E220Readback::settled(700,200,100,false,true));
 assert(!E220Readback::settled(699,200,100,true,true));
 assert(!E220Readback::settled(700,200,100,true,false));
 assert(E220Readback::settled(700,200,100,true,true));
 assert(!E220Readback::settled(3700,3100,100,true,true));
 assert(E220Readback::settled(504,4,UINT32_MAX-10,true,true));
 using namespace RadioAirtime;
 static_assert(WorstFragmentUs==6754304&&WorstPollUs==13508608&&WorstReplyUs==13508608,"Unexpected airtime model");
 static_assert(ExchangesPerHour==2&&ProductionPollIntervalMs==1800000,"Unsafe production cadence");
#ifndef CONDUCTED_BENCH
 static_assert(PollIntervalMs==ProductionPollIntervalMs&&LimitUs==36000000&&BootQuietMs>=HourMs,"Bench limits leaked into production");
#endif
 const uint32_t start=UINT32_MAX-2000;
 Budget budget;
 for(unsigned i=0;i<5;++i){ // Discovery, original poll, two retries and a reply fragment use the same ledger API.
  assert(budget.debit(start+i,MaxUartBytes+FixedAddressBytes,10));
 }
 assert(budget.used(start+9)==5*WorstFragmentUs);
 assert(!budget.debit(start+9,MaxUartBytes+FixedAddressBytes,10));
 assert(budget.used(start+HourMs+9)==5*WorstFragmentUs);
 assert(budget.used(start+HourMs+10)==4*WorstFragmentUs);
 assert(budget.debit(start+HourMs+10,MaxUartBytes+FixedAddressBytes,10));
 assert(budget.used(start+HourMs+14)==WorstFragmentUs);

 E220Core::Assembler receiver;
 const uint8_t sender[6]={1},target[6]={2},payload[]={1,2,3,4,5};
 uint8_t raw[200]{},wire[202]{};
 unsigned next=0;
 for(uint32_t turn=1;turn<=3;++turn){
  unsigned sent=0;
  const uint32_t now=turn*PollIntervalMs;
  while(next<sizeof(payload)&&slotAllows(sent,MaxReplyFragments)){
   const size_t n=E220Core::fragment(payload,sizeof(payload),42,sender,target,1,next++,raw,turn);
   const size_t wireBytes=E220Core::cobsEncode(raw,n+E220Core::Tag,wire,sizeof(wire));
   assert(wireBytes==E220Core::Header+1+E220Core::Tag+2);
   E220Core::Fragment fragment;
   assert(E220Core::parse(raw,n,fragment)&&fragment.turn==turn&&fragment.transfer==42);
   auto* message=receiver.add(fragment,now,ReplyAssemblyMs);
   if(next==sizeof(payload))assert(message&&!memcmp(message->data,payload,sizeof(payload)));
   else assert(!message);
   ++sent;
  }
  assert(sent==(turn<3?2U:1U));
 }
 puts("PASS rolling airtime: discovery/retry charges, refusal, expiry and clock rollover, budget-derived cadence and capped reply continuation");
}

static void masterPolledTurns(){
 const uint8_t master[6]={1,2,3,4,5,6},slave[6]={6,5,4,3,2,1};
 RadioTurn::Exchange grant,wait;
 assert(!grant.canClaim(master,100)&&!grant.canWrite(100,0));
 const uint32_t now=UINT32_MAX-50;
 wait.open(slave,7,now,now+RadioTurn::ReplyMs);
 grant.open(master,7,now+RadioTurn::TurnaroundMs,now+RadioTurn::ReplyMs-RadioTurn::ReplyGuardMs);
 assert(wait.busy()&&!wait.matches(master,7,now)&&!wait.matches(slave,8,now));
 assert(!grant.canClaim(master,now)&&!grant.canClaim(slave,now+100));
 assert(grant.canClaim(master,now+100)&&grant.claim()==7);
 assert(!grant.canClaim(master,now+101)&&grant.canWrite(now+101,100));
 const uint32_t cutoff=now+RadioTurn::ReplyMs-RadioTurn::ReplyGuardMs;
 assert(!grant.canWrite(cutoff-50,50)&&grant.expired(cutoff));
 assert(wait.matches(slave,7,cutoff)&&!wait.expired(cutoff));
 wait.replied();
 assert(!wait.matches(slave,7,cutoff));
 wait.clear();
 assert(!wait.busy());
 const uint32_t timeout=now+RadioTurn::ReplyMs;
 wait.open(slave,8,now,timeout);
 assert(wait.expired(timeout)&&!wait.matches(slave,8,timeout));
 assert(RadioTurn::retryAt(timeout,0)==timeout+250);
 assert(RadioTurn::retryAt(timeout,750)==timeout+1000);
 assert(RadioTurn::mayAttempt(0,5)&&RadioTurn::mayAttempt(1,5)&&RadioTurn::mayAttempt(2,5));
 assert(!RadioTurn::mayAttempt(3,5)&&!RadioTurn::mayAttempt(1,0));
 puts("PASS master-polled radio turns: addressed token, single claim, fragment cutoff, timeout and jitter across clock rollover");
}

static void capturedRssiFraming() {
 // Verbatim payload and RSSI bytes from the two conducted bench captures.
 // The captured probes were ASCII. Wrap only their payloads in offline RV2
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
 for(unsigned scenario=0;scenario<8;++scenario){
  E220Core::ReceiveStream stream;
  unsigned frames=0,rejects=0;
  std::vector<int> signals;
  auto frame=[&](const uint8_t* encoded,size_t size){
   assert(frames<2);
   uint8_t raw[200]{};
   const size_t length=E220Core::cobsDecode(encoded,size,raw,sizeof(raw));
   if(length!=E220Core::Header+6+E220Core::Tag){++rejects;return false;}
   E220Core::Fragment decoded;
   assert(E220Core::parse(raw,length-E220Core::Tag,decoded));
   assert(decoded.transfer==frames+1&&decoded.length==6&&!memcmp(decoded.bytes,captures[frames],6));
   ++frames;
   return true;
  };
  auto signal=[&](uint8_t value){signals.push_back(RadioRssi::dbm(value));};
  auto feed=[&](unsigned index,size_t first,size_t end,uint32_t now){
   for(size_t i=first;i<end;++i)if(!stream.add(wire[index][i],now,true,frame,signal))++rejects;
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
  }else if(scenario<4){
   feed(0,0,end,10);
   if(scenario==3)feed(1,0,wire[1].size(),10);
  }else{
   auto add=[&](uint8_t b){if(!stream.add(b,10,true,frame,signal))++rejects;};
   feed(0,0,end-1,10);
   if(scenario==5){add(0xdf);add(0x55);}
   if(scenario==6){
    add(0xdf);
    add(0x07);add(0x11);add(0);add(0xe1); // Bad COBS run length, then its RSSI.
   }
   if(scenario==7)add(0);
   feed(1,0,wire[1].size(),10);
  }
  assert(frames==(scenario>=3?2U:1U));
  assert(rejects==(scenario>=4&&scenario<=6?1U:0U));
  if(scenario==4)assert(signals.size()==1&&signals[0]==-32);
  else if(scenario==7)assert(signals.size()==2&&signals[0]==-256&&signals[1]==-32);
  else{
   assert(signals.size()==frames&&signals[0]==-33);
   if(frames==2)assert(signals[1]==-32);
  }
 }
 puts("PASS production UART framing: captured RSSI same burst, delayed, split frame and back to back");
 puts("PASS production UART resync: missing RSSI, stray byte, malformed COBS, and RSSI 0x00");
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

static void compactFrames(){
 const uint8_t lengths[]={2,1,10,7,9,10,0};
 for(uint8_t kind=1;kind<=7;++kind){
  CompactRadio::Frame f,decoded;
  f.kind=kind;f.source[0]=1;f.destination[0]=2;f.token=0x10203040;
  uint8_t raw[64]{},encoded[66]{};
  auto size=CompactRadio::pack(f,raw);
  uint8_t expected[E220Core::Tag]{};
  assert(CompactRadio::validTag(raw+size,expected));
  raw[size+E220Core::Tag-1]^=1;
  assert(!CompactRadio::validTag(raw+size,expected));
  assert(size==CompactRadio::Header+lengths[kind-1]);
  assert(CompactRadio::unpack(raw,size,decoded));
  assert(CompactRadio::addressed(decoded,f.source,f.destination));
  assert(!CompactRadio::addressed(decoded,f.destination,f.source));
  RadioTurn::Exchange turn;turn.open(f.source,f.token,10,100);
  assert(turn.matches(decoded.source,decoded.token,20));
  assert(!turn.matches(decoded.source,decoded.token+1,20));
  auto bytes=E220Core::cobsEncode(raw,size+E220Core::Tag,encoded,sizeof(encoded));
  assert(bytes==size+E220Core::Tag+2&&bytes<=64);
  RadioAirtime::Budget budget;assert(budget.debit(20,bytes,0));
  assert(budget.used(20)==RadioAirtime::fragmentUs(bytes));
  raw[2]=2;assert(!CompactRadio::unpack(raw,size,decoded));
  raw[2]=3;raw[5]=1;assert(!CompactRadio::unpack(raw,size,decoded));
 }
}
static void capturedV3Rssi(){
 const uint8_t capture[]={0x04,0xe2,0x7a,0x03,0x02,0x04,0x23,0x84,0x1f,0xe8,0x2a,0x2f,0x84,0x78,0x21,0x84,0x7d,0x77,0x10,0xa7,0xc9,0x24,0x13,0xc4,0xdd,0x6c,0x44,0x47,0x0c,0x01,0xfb,0x89,0x20,0x6c,0xa4,0x2f,0xe1,0x39,0x34,0x0c,0x31,0x05,0x30,0xfd,0x01,0x7f,0x00,0xff};
 const uint8_t captured0[]={0x04,0xe2,0x7a,0x03,0x02,0x05,0x19,0x84,0x1f,0xe8,0x2a,0x2f,0x84,0x78,0x21,0x84,0x7d,0x77,0x10,0x82,0x57,0x19,0x93,0x23,0xf2,0x8d,0xf0,0xe5,0x44,0x01,0x02,0x11,0x3b,0xb2,0xcc,0x94,0x32,0xb6,0x93,0x98,0xef,0x54,0x15,0x75,0xe9,0x56,0x4e,0x69,0x00,0x00};
 const uint8_t captured1[]={0x04,0xe2,0x7a,0x03,0x02,0x06,0x18,0x84,0x1f,0xe8,0x2a,0x2f,0x84,0x78,0x21,0x84,0x7d,0x77,0x10,0x79,0xe9,0xdf,0xbd,0xb5,0x5b,0xf7,0x6a,0x92,0x04,0x1a,0x01,0x01,0x11,0x2f,0x93,0x16,0x5c,0x2f,0xf4,0xe3,0x70,0x06,0x5d,0x61,0x41,0x81,0x04,0x8c,0x05,0x00,0x00};
 const uint8_t captured2[]={0x04,0xe2,0x7a,0x03,0x02,0x06,0x18,0x84,0x1f,0xe8,0x2a,0x2f,0x84,0x78,0x21,0x84,0x7d,0x77,0x10,0xcc,0xa5,0x1b,0x0e,0xf1,0x5b,0xf7,0x6a,0x91,0x04,0x1c,0x01,0x01,0x11,0xf9,0x75,0x89,0x09,0xed,0xca,0x63,0x59,0x6e,0xaa,0x8e,0xb4,0x14,0xcb,0x9a,0x21,0x00,0x00};
 const auto check=[](const uint8_t* bytes,size_t size,uint8_t expected){
  for(size_t split=1;split<size;++split){
   E220Core::ReceiveStream stream;
   unsigned frames=0,samples=0;
   auto frame=[&](const uint8_t* encoded,size_t n){
    uint8_t raw[64];
    const auto len=E220Core::cobsDecode(encoded,n,raw,sizeof(raw));
    CompactRadio::Frame decoded;
    assert(len>E220Core::Tag&&CompactRadio::unpack(raw,len-E220Core::Tag,decoded));
    ++frames;return true;
   };
   auto rssi=[&](uint8_t raw){assert(raw==expected&&!RadioRssi::available(raw));++samples;};
   for(size_t i=0;i<size;++i)assert(stream.add(bytes[i],i<split?10:20,true,frame,rssi));
   assert(frames==1&&samples==1);
  }
 };
 check(capture,sizeof(capture),0xff);
 check(captured0,sizeof(captured0),0);
 check(captured1,sizeof(captured1),0);
 check(captured2,sizeof(captured2),0);
 assert(!RadioRssi::available(114)&&RadioRssi::available(115)&&RadioRssi::available(245)&&!RadioRssi::available(246));
 assert(!RadioRssi::available(0)&&!RadioRssi::available(0xfe)&&RadioRssi::dbm(0xdf)==-33);
}
int main(){
 capturedV3Rssi();
 compactFrames();
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
 const uint8_t outOfBand[]={0xc1,0,6,0,0,0x62,0,18,0x83};
 assert(E220Core::settings(outOfBand,sizeof(outOfBand),settings));
 assert(settings.channel==18&&settings.power==22&&!settings.noiseRssi&&settings.parity==0);
 assert(!E220Core::settings(registers,8,settings));registers[7]=81;assert(!E220Core::settings(registers,9,settings));registers[7]=23;registers[5]=0x6a;assert(!E220Core::settings(registers,9,settings));registers[5]=0x62;
 for(unsigned size=1;size<=500;++size){std::vector<uint8_t> input(size),encoded(size+size/254+3),decoded(size);for(unsigned i=0;i<size;++i)input[i]=(i*31)%256;size_t n=E220Core::cobsEncode(input.data(),size,encoded.data(),encoded.size());assert(n&&encoded[n-1]==0);size_t m=E220Core::cobsDecode(encoded.data(),n-1,decoded.data(),decoded.size());assert(m==size&&decoded==input);assert(!E220Core::cobsEncode(input.data(),size,encoded.data(),size));}
 uint8_t sender[6]={1,2,3,4,5,6},target[6]={6,5,4,3,2,1};
 for(unsigned packet: {64U,128U,200U})for(unsigned length:{1U,16U,152U,512U,2048U,3072U}){
  std::vector<uint8_t> input(length);for(unsigned i=0;i<length;++i)input[i]=i%251;
  uint8_t step=E220Core::span(packet),raw[220]{},encoded[222]{};unsigned count=(length+step-1)/step;
  if(count>255){assert(!E220Core::fragment(input.data(),length,17,sender,target,step,0,raw));continue;}
  E220Core::Assembler assembly;E220Core::Assembly* done=nullptr;
  for(int part=count-1;part>=0;--part){size_t n=E220Core::fragment(input.data(),length,17,sender,target,step,part,raw);assert(n);size_t wire=E220Core::cobsEncode(raw,n+E220Core::Tag,encoded,sizeof(encoded));assert(wire<=packet);E220Core::Fragment f;assert(E220Core::parse(raw,n,f));auto result=assembly.add(f,100);if(result)done=result;if(part>0){assert(!assembly.add(f,100));}}
  assert(done&&done->total==length&&!memcmp(done->data,input.data(),length));assert(!E220Core::span(32));
 }
 uint8_t data[256]{},raw[220];auto n=E220Core::fragment(data,sizeof(data),1,sender,target,100,0,raw);E220Core::Fragment fragment;assert(E220Core::parse(raw,n,fragment));E220Core::Assembler bounded;assert(!bounded.add(fragment,0));raw[4]=2;assert(E220Core::parse(raw,n,fragment));bounded.add(fragment,0);raw[4]=3;assert(E220Core::parse(raw,n,fragment));bounded.add(fragment,0);assert(bounded.rejected==1);bounded.expire(120001);assert(bounded.expired==2);
 raw[21]=0;assert(!E220Core::parse(raw,n,fragment));raw[0]=0;assert(!E220Core::parse(raw,n,fragment));
 E220Core::Routes routes;assert(routes.observe("ESP32-010203040506","ESP32-060504030201",1,0));assert(routes.find("ESP32-010203040506",59999,60000));assert(!routes.find("ESP32-010203040506",60001,60000));routes.expire(60001,60000);assert(!routes.rows[0].destination[0]);assert(!routes.observe("bad id","peer",1,0));
 assert(E220Core::forwardAllowed(3,1,4,"RADIO:next","RADIO:previous"));assert(!E220Core::forwardAllowed(1,1,4,"RADIO:next","RADIO:previous"));assert(!E220Core::forwardAllowed(3,4,4,"RADIO:next","RADIO:previous"));assert(!E220Core::forwardAllowed(3,1,4,"RADIO:same","RADIO:same"));
 assert(routes.observe("target","direct",1,0));assert(routes.observe("target","relay",2,1));assert(!strcmp(routes.find("target",2,60000)->nextHop,"direct"));
 capturedRssiFraming();
 masterPolledTurns();
 rollingAirtimeAndFragmentCap();
 simulatedTwoBoardBench();
 puts("PASS production E220 settings/stream resync, COBS 500 vectors, all subpacket sizes, reassembly, duplicates, bounds, expiry, routes and role safety");
}
