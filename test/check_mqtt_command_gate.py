from pathlib import Path
import os,re,subprocess
root=Path(__file__).resolve().parents[1]
source=(root/'src/communication/canonical_mqtt.cpp').read_text(encoding='utf-8')
match=re.search(r' if\((type=="command".*?)\)\{\n   String packet;',source,re.S)
assert match, "Production MQTT command admission condition not found"
condition=match.group(1)
code='''#include <ArduinoJson.h>
#include <cassert>
using String = std::string;
#include <string>
bool gate(const char* raw,std::string type="command"){JsonDocument d;assert(!deserializeJson(d,raw));auto data=d.as<JsonObjectConst>();const String device="MASTER";return CONDITION;}
int main(){assert(gate(R"JSON({"packet":{"destination":"SLAVE"},"radio_async":true})JSON"));assert(!gate(R"JSON({"packet":{"destination":"MASTER"},"radio_async":true})JSON"));assert(!gate(R"JSON({"packet":{"destination":"SLAVE"},"radio_async":false})JSON"));assert(gate("{\\\"packet\\\":{\\\"type\\\":\\\"COMMAND\\\"}}"));assert(!gate("{\\\"packet\\\":[]}"));assert(!gate("{\\\"packet\\\":null}"));assert(!gate("{\\\"packet\\\":{},\\\"extra\\\":1}"));assert(!gate("{\\\"packet\\\":{}}","telemetry"));}
'''.replace('CONDITION',condition)
cpp=root/'.pio/mqtt_command_gate.cpp';cpp.write_text(code,encoding='utf-8');compiler=Path.home()/'.platformio/packages/toolchain-gccmingw32/bin/g++.exe';env=dict(os.environ);env['PATH']=str(compiler.parent)+os.pathsep+env['PATH'];out=root/'.pio/mqtt_command_gate.exe'
subprocess.run([str(compiler),'-std=c++11','-static','-I'+str(root/'.pio/libdeps/esp32_master/ArduinoJson/src'),str(cpp),'-o',str(out)],env=env,check=True);subprocess.run([str(out)],env=env,check=True)
print('PASS actual firmware MQTT command gate: const packet object accepted; arrays, null, extra fields and wrong message types rejected')
