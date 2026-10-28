from pathlib import Path
import re,os,subprocess
r=Path(__file__).resolve().parents[1];source=(r/'src/communication/canonical_mqtt.cpp').read_text(encoding='utf-8');block=re.search(r'(Message ack;ack.messageId=mid;.*?serializeJson\(payload,ack.payload\);)accepted=transportManager',source,re.S).group(1)
code='''#include "../src/protocol/protocol.h"
#include <ArduinoJson.h>
#include <cassert>
#include <ctime>
#include <iostream>
int main(){String mid="RECEIPT-1",device="ESP32-MASTER";JsonDocument data;data["received_message_id"]="MSG-ORIGINAL";
BLOCK
Message parsed;assert(Message::fromJson(ack.toJson(),parsed));assert(parsed.nextHop==device);assert(parsed.correlationId=="MSG-ORIGINAL");assert(parsed.type==PacketType::ACK);
ack.nextHop="";assert(!Message::fromJson(ack.toJson(),parsed));std::cout<<"PASS actual PERSISTED adapter roundtrip through production Message parser; missing routing field rejected\\n";}
'''.replace('BLOCK',block)
cpp=r/'.pio/persistence_receipt.cpp';cpp.write_text(code,encoding='utf-8');compiler=Path.home()/'.platformio/packages/toolchain-gccmingw32/bin/g++.exe';env=dict(os.environ);env['PATH']=str(compiler.parent)+os.pathsep+env['PATH'];out=r/'.pio/persistence_receipt.exe'
subprocess.run([str(compiler),'-std=c++11','-static','-DARDUINOJSON_ENABLE_ARDUINO_STRING=1','-DARDUINOJSON_POOL_CAPACITY=64','-I'+str(r/'test/protocol_stubs'),'-I'+str(r/'.pio/libdeps/esp32_master/ArduinoJson/src'),str(r/'src/protocol/protocol.cpp'),str(cpp),'-o',str(out)],check=True,env=env);subprocess.run([str(out)],check=True,env=env)
