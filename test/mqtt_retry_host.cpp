#include "../src/config/mqtt_retry_stamp.h"
#include <cassert>
#include <cstdio>
int main(){
 const uint32_t original=1700000100;
 assert(MqttRetryStamp::select(1700000110,true,original)==original);
 assert(MqttRetryStamp::select(1700000190,true,original)==original);
 assert(MqttRetryStamp::select(1700000190,true,0)==0);
 assert(MqttRetryStamp::select(1700000190,false,original)==1700000190);
 std::puts("PASS MQTT retries preserve canonical packet timestamps; fresh controls use current time");
}
