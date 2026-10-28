#pragma once
#include <stdint.h>
#include <stddef.h>
namespace DhtPulseCore {
struct Pulse {uint16_t us;bool high;};
enum class Result:uint8_t {Ok,NoResponse,Truncated,Timing,Checksum,Range,Driver,PollingRead};
inline const char* name(Result r){switch(r){case Result::Ok:return "OK";case Result::NoResponse:return "NO_RESPONSE";case Result::Truncated:return "TRUNCATED";case Result::Timing:return "PULSE_TIMING";case Result::Checksum:return "CHECKSUM";case Result::Range:return "OUT_OF_RANGE";case Result::PollingRead:return "POLLING_READ_ERROR";default:return "CAPTURE_DRIVER_ERROR";}}
inline Result decode(const Pulse* p,size_t n,float& t,float& h){
 size_t start=n;
 for(size_t i=0;i+1<n;++i)if(!p[i].high&&p[i].us>=60&&p[i].us<=115&&p[i+1].high&&p[i+1].us>=60&&p[i+1].us<=115){start=i+2;break;}
 if(start==n)return Result::NoResponse;
 if(n-start<80)return Result::Truncated;
 uint8_t data[5]{};
 for(unsigned bit=0;bit<40;++bit){auto low=p[start+bit*2],high=p[start+bit*2+1];if(low.high||!high.high||low.us<25||low.us>90||high.us<10||high.us>110)return Result::Timing;data[bit/8]=(data[bit/8]<<1)|(high.us>=50);}
 if(uint8_t(data[0]+data[1]+data[2]+data[3])!=data[4])return Result::Checksum;
 t=data[2];if(data[3]&128)t=-1-t;t+=(data[3]&15)*.1f;h=data[0]+data[1]*.1f;
 return t>=0&&t<=50&&h>=0&&h<=100?Result::Ok:Result::Range;
}
}
