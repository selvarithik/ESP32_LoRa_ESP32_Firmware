#include "../src/communication/mqtt_recovery_core.h"
#include <cassert>
#include <cstdio>
int main(){using namespace MqttRecoveryCore;assert(allocationFailure(-0x7f00));assert(!allocationFailure(-0x2700));assert(!allocationFailure(0));Policy p;assert(!p.due(10000,100000,50000));p.failed(100);assert(!p.due(1099,100000,50000));assert(!p.due(1100,59000,50000));assert(!p.due(1100,100000,19000));assert(p.due(1100,100000,50000));p.started();assert(!p.due(90000,100000,50000));for(int i=0;i<20;++i)p.failed(1000);assert(p.delay==60000);p.connected();p.failed(0xfffffff0);assert(!p.due(0xfffffff1,100000,50000));assert(p.due(0x400,100000,50000));puts("PASS actual TLS recovery policy: allocation-only, bounded retries, memory guard, pause and timer wrap");}
