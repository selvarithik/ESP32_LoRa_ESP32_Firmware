#pragma once
#include "hardware_interfaces.h"
#include <string.h>
namespace Hardware {
class MockSensor:public SensorProvider {public:SensorSample next;MockSensor(){next.temperature=25;next.humidity=50;next.valid=true;next.simulated=true;}SensorSample read() override{return next;}};
class MockNetwork:public NetworkDriver {public:NetworkState current;MockNetwork(){current.simulated=true;}NetworkState state()const override{return current;}bool reconnect()override{current.connected=true;return true;}void disconnect()override{current.connected=false;current.gateway=false;}};
class MockRadio:public RadioDriver {bool ready=false,occupied=false;uint8_t bytes[4096];size_t length=0;RadioMetrics counters;public:bool drop=false;bool begin()override{ready=true;return true;}bool send(const uint8_t* p,size_t n)override{if(!ready||!p||!n||n>sizeof(bytes)||occupied){++counters.rejected;return false;}++counters.transmitted;if(drop)return true;memcpy(bytes,p,n);length=n;occupied=true;return true;}size_t receive(uint8_t* p,size_t max)override{if(!occupied||max<length)return 0;memcpy(p,bytes,length);occupied=false;++counters.received;return length;}void stop()override{ready=false;occupied=false;}RadioMetrics metrics()const override{return counters;}};
}
