#pragma once
#include <stdint.h>
namespace MqttRecoveryCore {
inline bool allocationFailure(int error){return error==-0x7f00;}
struct Policy {
 bool pending=false;uint32_t next=0,delay=1000;
 void failed(uint32_t now){pending=true;next=now+delay;delay=delay<30000?delay*2:60000;}
 bool due(uint32_t now,uint32_t freeBytes,uint32_t largest)const{return pending&&(int32_t)(now-next)>=0&&freeBytes>=60000&&largest>=20000;}
 void started(){pending=false;}
 void connected(){delay=1000;}
};
}
