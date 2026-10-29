#pragma once
#include <stdint.h>
namespace MqttRecoveryCore {
inline bool allocationFailure(int error){return error==-0x7f00;}
// The network worker owns stop/destroy; callbacks only report state.
struct LinkWatch {
 bool wasWifi=false,seenWifi=false,waiting=false;uint32_t since=0;
 bool poll(uint32_t now,bool enabled,bool wifi,bool connected,bool running,uint32_t timeoutMs){
  if(!enabled){wasWifi=wifi;seenWifi=wifi;waiting=false;return false;}
  bool returned=seenWifi&&!wasWifi&&wifi;wasWifi=wifi;if(wifi)seenWifi=true;
  if(returned&&running){waiting=false;return true;}
  if(connected){waiting=false;return false;}
  if(!wifi||!running){waiting=false;return false;}
  if(!waiting){waiting=true;since=now;return false;}
  uint32_t limit=timeoutMs+5000;if(limit<15000)limit=15000;if(limit>60000)limit=60000;
  if(now-since>=limit){waiting=false;return true;}return false;
 }
 void recycled(){waiting=false;}
};
struct Policy {
 bool pending=false;uint32_t next=0,delay=1000;
 void failed(uint32_t now){pending=true;next=now+delay;delay=delay<30000?delay*2:60000;}
 bool due(uint32_t now,uint32_t freeBytes,uint32_t largest)const{return pending&&(int32_t)(now-next)>=0&&freeBytes>=60000&&largest>=20000;}
 void started(){pending=false;}
 void connected(){delay=1000;}
};
}
