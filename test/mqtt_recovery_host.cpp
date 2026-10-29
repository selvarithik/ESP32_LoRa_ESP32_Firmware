#include "../src/communication/mqtt_recovery_core.h"
#include <cassert>
#include <cstdio>
int main(){using namespace MqttRecoveryCore;
 LinkWatch w;assert(!w.poll(0,true,true,true,true,10000));assert(!w.poll(1,true,false,false,true,10000));assert(w.poll(2,true,true,false,true,10000));w.recycled();assert(!w.poll(3,true,true,false,true,10000));assert(!w.poll(15002,true,true,false,true,10000));assert(w.poll(15003,true,true,false,true,10000));assert(!w.poll(16000,false,true,false,true,10000));assert(!w.poll(99999,false,true,false,true,10000));
 LinkWatch wrap;assert(!wrap.poll(0xfffffff0,true,true,false,true,10000));assert(!wrap.poll(10,true,true,false,true,10000));assert(wrap.poll(16000,true,true,false,true,10000));
assert(allocationFailure(-0x7f00));assert(!allocationFailure(-0x2700));assert(!allocationFailure(0));Policy p;assert(!p.due(10000,100000,50000));p.failed(100);assert(!p.due(1099,100000,50000));assert(!p.due(1100,59000,50000));assert(!p.due(1100,100000,19000));assert(p.due(1100,100000,50000));p.started();assert(!p.due(90000,100000,50000));for(int i=0;i<20;++i)p.failed(1000);assert(p.delay==60000);p.connected();p.failed(0xfffffff0);assert(!p.due(0xfffffff1,100000,50000));assert(p.due(0x400,100000,50000));puts("PASS actual TLS recovery policy: allocation-only, bounded retries, memory guard, pause and timer wrap");}
