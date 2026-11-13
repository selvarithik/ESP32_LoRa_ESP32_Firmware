#pragma once
#include <stdint.h>
#include <string.h>
#include "conducted_build.h"

namespace RadioTurn {
#ifdef CONDUCTED_BENCH
constexpr uint8_t MaxRetries=1;
constexpr uint32_t ReplyMs=48000;
#else
constexpr uint8_t MaxRetries=2;
constexpr uint32_t ReplyMs=20000;
#endif
constexpr uint32_t ReplyGuardMs=2000;
constexpr uint32_t TurnaroundMs=100;
constexpr uint32_t RetryJitterMinMs=250,RetryJitterRangeMs=751;

inline bool due(uint32_t now,uint32_t deadline){return int32_t(now-deadline)>=0;}
inline uint32_t retryAt(uint32_t now,uint32_t entropy){return now+RetryJitterMinMs+entropy%RetryJitterRangeMs;}
inline bool mayAttempt(uint8_t attempts,uint8_t configuredRetries){return attempts<=configuredRetries&&attempts<=MaxRetries;}

// The Master gives exactly one addressed peer one reply, using its authenticated
// transfer ID. A Slave may send multiple fragments of that one reply.
class Exchange {
 uint8_t peer[6]{};
 uint32_t token=0,opensAt=0,endsAt=0;
 bool active=false,claimed=false,received=false;
public:
 void open(const uint8_t* address,uint32_t id,uint32_t start,uint32_t end){
  memcpy(peer,address,sizeof(peer));token=id;opensAt=start;endsAt=end;
  active=true;claimed=received=false;
 }
 void clear(){active=claimed=received=false;}
 bool busy()const{return active;}
 bool expired(uint32_t now)const{return active&&due(now,endsAt);}
 bool matches(const uint8_t* address,uint32_t id,uint32_t now)const{
  return active&&!received&&!expired(now)&&id==token&&!memcmp(peer,address,sizeof(peer));
 }
 bool canClaim(const uint8_t* address,uint32_t now)const{
  return matches(address,token,now)&&!claimed&&due(now,opensAt);
 }
 uint32_t claim(){claimed=true;return token;}
 bool canWrite(uint32_t now,uint32_t guard)const{
  return active&&claimed&&due(now,opensAt)&&!expired(now+guard);
 }
 void replied(){received=true;}
};
}
