#include "../src/sensors/dht_pulse_core.h"
#include <vector>
#include <assert.h>
#include <stdio.h>
using namespace DhtPulseCore;
std::vector<Pulse> capture(uint8_t h,uint8_t hd,uint8_t t,uint8_t td){
 uint8_t bytes[]={h,hd,t,td,uint8_t(h+hd+t+td)};
 std::vector<Pulse> p={{15,false},{35,true},{80,false},{80,true}};
 for(uint8_t b:bytes)for(int bit=7;bit>=0;--bit){p.push_back({50,false});p.push_back({uint16_t((b&(1<<bit))?70:27),true});}
 return p;
}
int main(){
 float t=0,h=0;auto p=capture(56,0,33,3);assert(decode(p.data(),p.size(),t,h)==Result::Ok);assert(t>33.29&&t<33.31&&h==56);
 auto bad=p;bad[83].us=70;assert(decode(bad.data(),bad.size(),t,h)==Result::Checksum);
 assert(decode(p.data(),20,t,h)==Result::Truncated);
 Pulse none[]={{2000,false},{2000,true}};assert(decode(none,2,t,h)==Result::NoResponse);
 bad=p;bad[4].us=100;assert(decode(bad.data(),bad.size(),t,h)==Result::Timing);
 bad=p;bad[5].high=false;assert(decode(bad.data(),bad.size(),t,h)==Result::Timing);
 auto range=capture(101,0,30,0);assert(decode(range.data(),range.size(),t,h)==Result::Range);
 for(unsigned value=0;value<=50;++value){auto c=capture(60,0,value,0);assert(decode(c.data(),c.size(),t,h)==Result::Ok&&t==value);}
 puts("PASS production DHT11 pulse decoding: 51 temperatures, decimal values, no response, truncation, timing, checksum and range rejection");
}
