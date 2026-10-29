#pragma once
#include <stdint.h>
namespace TransportTrial {
enum class Phase {IDLE,PREPARED,TRIAL,COMMITTED,CANCELLED,ROLLED_BACK};
inline const char* name(Phase p){switch(p){case Phase::PREPARED:return "PREPARED";case Phase::TRIAL:return "TRIAL";case Phase::COMMITTED:return "COMMITTED";case Phase::CANCELLED:return "CANCELLED";case Phase::ROLLED_BACK:return "ROLLED_BACK";default:return "IDLE";}}
inline bool busy(Phase p){return p==Phase::PREPARED||p==Phase::TRIAL;}
inline bool expired(Phase p,uint32_t now,uint32_t deadline){return busy(p)&&now>=deadline;}
inline bool canConfirm(Phase p,bool applied,bool ping,bool receipt,uint32_t now,uint32_t deadline){return p==Phase::TRIAL&&applied&&ping&&receipt&&now<deadline;}
inline bool id(const char* s){unsigned n=0;for(;*s;++s){if(++n>48||!((*s>='A'&&*s<='Z')||(*s>='a'&&*s<='z')||(*s>='0'&&*s<='9')||*s=='-'||*s=='_'))return false;}return n>0;}
}
