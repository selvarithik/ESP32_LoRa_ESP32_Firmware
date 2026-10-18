#pragma once
#include <stdint.h>
// MQTT 3.1.1 CONNACK 4/5 are authentication failures. Other refusals and
// DNS/socket errors are not evidence of a certificate failure.
inline int classifyMqttFailure(bool refused,int connack,int tlsStackError,uint32_t certificateFlags){
    if(refused)return connack==4||connack==5?2:3;
    return tlsStackError||certificateFlags?1:3;
}
