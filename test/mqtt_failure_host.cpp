#include "../src/config/mqtt_failure.h"
#include <assert.h>
#include <stdio.h>
int main(){assert(classifyMqttFailure(true,4,0,0)==2);assert(classifyMqttFailure(true,5,0,0)==2);assert(classifyMqttFailure(true,3,0,0)==3);assert(classifyMqttFailure(true,2,0,0)==3);assert(classifyMqttFailure(false,0,-1,0)==1);assert(classifyMqttFailure(false,0,0,8)==1);assert(classifyMqttFailure(false,0,0,0)==3);puts("PASS production MQTT failure classification: auth, broker refusal, TLS and generic transport remain distinct");}
