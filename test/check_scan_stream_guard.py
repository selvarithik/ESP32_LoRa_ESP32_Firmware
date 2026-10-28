from pathlib import Path
import re,os,subprocess
r=Path(__file__).resolve().parents[1];source=(r/'src/core/network_service.cpp').read_text(encoding='utf-8');block=re.search(r'else if\(type=="WIFI_SCAN"\).*?(?=\s*else if\(type=="WIFI_RECONNECT"\))',source,re.S).group()
code='''#include "../test/protocol_stubs/Arduino.h"
#include <ArduinoJson.h>
#include <cassert>
struct Network{bool connected=false;bool mqttConnected(){return connected;}}transportManager;
struct Wifi{int calls=0;String scanJson(){++calls;return "[]";}}wifiManager;
bool execute(String& result){String type="WIFI_SCAN";if(false){}BLOCK return true;}
int main(){String result;transportManager.connected=true;assert(!execute(result));assert(wifiManager.calls==0);assert(strstr(result.c_str(),"WIFI_SCAN_BUSY_MQTT_ACTIVE"));transportManager.connected=false;assert(execute(result));assert(wifiManager.calls==1);}
'''.replace('BLOCK',block)
cpp=r/'.pio/scan_stream_guard.cpp';cpp.write_text(code,encoding='utf-8');compiler=Path.home()/'.platformio/packages/toolchain-gccmingw32/bin/g++.exe';env=dict(os.environ);env['PATH']=str(compiler.parent)+os.pathsep+env['PATH'];out=r/'.pio/scan_stream_guard.exe';subprocess.run([str(compiler),'-std=c++11','-static','-DARDUINOJSON_ENABLE_ARDUINO_STRING=1','-DARDUINOJSON_POOL_CAPACITY=64','-I'+str(r/'test/protocol_stubs'),'-I'+str(r/'.pio/libdeps/esp32_master/ArduinoJson/src'),str(cpp),'-o',str(out)],check=True,env=env);subprocess.run([str(out)],check=True,env=env);print('PASS production WIFI_SCAN handler never starts scan during active MQTT; paused-session scan path preserved')
