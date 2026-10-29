#pragma once
#include <stdint.h>
#include <stddef.h>
namespace FeatureCore {
inline bool ipv4(const char* s,uint32_t& result){
 result=0;for(int part=0;part<4;++part){if(!*s)return false;unsigned v=0,n=0;const char* begin=s;while(*s>='0'&&*s<='9'){v=v*10+(*s++-'0');if(++n>3||v>255)return false;}if(!n||(n>1&&*begin=='0'))return false;result=(result<<8)|v;if(part<3){if(*s++!='.')return false;}else if(*s)return false;}return true;
}
inline bool unicast(uint32_t n){return n&&n!=0xffffffff&&(n>>24)>0&&(n>>24)!=127&&(n>>24)<224;}
inline bool staticNetwork(const char* address,const char* mask,const char* gateway,const char* dns,const char* dns2){
 uint32_t ip,m,g,d,d2=0;if(!ipv4(address,ip)||!ipv4(mask,m)||!ipv4(gateway,g)||!ipv4(dns,d))return false;
 uint32_t host=~m;if(!m||!host||(host&(host+1))||!unicast(ip)||!unicast(g)||!unicast(d)||ip==g)return false;
 if((ip&m)!=(g&m)||(ip&host)==0||(ip&host)==host||(g&host)==0||(g&host)==host)return false;
 return !*dns2||(ipv4(dns2,d2)&&unicast(d2));
}
struct Schedule {bool enabled=false;uint16_t start=0;uint32_t duration=3600;uint8_t days=127;int offset=330;};
inline bool valid(const Schedule& s){return s.start<1440&&s.duration>=60&&s.duration<=86400&&s.days>=1&&s.days<=127&&s.offset>=-720&&s.offset<=840;}
// Return remaining sleep seconds, or zero outside a configured window / without trusted UTC.
inline uint32_t sleepRemaining(const Schedule& s,int64_t utc){
 if(!s.enabled||!valid(s)||utc<1700000000)return 0;
 int64_t local=utc+s.offset*60,day=local/86400;
 for(int yesterday=0;yesterday<=1;++yesterday){int64_t anchor=day-yesterday;unsigned weekday=(anchor+4)%7;int64_t start=anchor*86400+s.start*60;if((s.days&(1<<weekday))&&local>=start&&local<start+s.duration)return (uint32_t)(start+s.duration-local);}
 return 0;
}
inline uint32_t manualSleepRemaining(uint32_t now,uint32_t started,uint32_t seconds){uint32_t elapsed=(now-started)/1000;return elapsed>=seconds+5?0:seconds+5-elapsed;}
enum class PowerDecision {ACTIVE,CLOCK_REQUIRED,DRAINING,SLEEP,RESTART};
inline PowerDecision powerDecision(bool restart,uint32_t sinceRequest,bool enabled,bool clock,uint32_t remaining,unsigned pending,bool idle,bool coordinatorIdle){
 if(!restart&&(!enabled||!remaining))return enabled&&!clock?PowerDecision::CLOCK_REQUIRED:PowerDecision::ACTIVE;
 if(pending||!idle||!coordinatorIdle||(restart&&sinceRequest<3000))return PowerDecision::DRAINING;
 return restart?PowerDecision::RESTART:PowerDecision::SLEEP;
}
inline uint32_t reconnectDelay(unsigned attempts){unsigned power=attempts>5?5:attempts;uint32_t n=1000U<<power;return n>30000?30000:n;}
}
