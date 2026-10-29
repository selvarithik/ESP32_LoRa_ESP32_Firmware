"""Executes the production C++ admission rules and compares registry with Python."""
from pathlib import Path
import os,subprocess,sys
root=Path(__file__).resolve().parents[1];gateway=root.parent/'ESP32_LoRa_Project'
sys.path.insert(0,str(gateway/'backend'));import device_control_contract as contract
compiler=Path.home()/'.platformio/packages/toolchain-gccmingw32/bin/g++.exe';out=root/'.pio/command_rules.exe'
env=dict(os.environ);env['PATH']=str(compiler.parent)+os.pathsep+env['PATH']
subprocess.run([str(compiler),'-std=c++11','-static','-I'+str(root/'.pio/libdeps/esp32dev/ArduinoJson/src'),str(root/'test/command_rules_host.cpp'),'-o',str(out)],check=True,env=env)
result=subprocess.check_output([str(out)],env=env,text=True);assert set(result.splitlines())==contract.COMMANDS
print('PASS executed C++ command admission and exact Gateway/firmware registry parity')

# The advertised surface is deliberately smaller than accepted legacy aliases.
import re
catalog=(root/'src/command/command_catalog.h').read_text(encoding='utf-8')
primary=re.findall(r'"([A-Z_]+)"',re.search(r'Primary\[\]=\{(.*?)\}',catalog)[1])
assert tuple(primary)==contract.PRIMARY_COMMANDS and len(primary)==21
local=(root/'src/web/local_web_server.cpp').read_text(encoding='utf-8')
assert re.findall(r'<option>(.*?)</option>',re.search(r'<select id="command">(.*?)</select>',local)[1])==primary
page=(gateway/'dashboard/templates/command_center.html').read_text(encoding='utf-8')
assert re.findall(r'<option value="([A-Z_]+)"',re.search(r'<select id="cmd-type-select".*?>(.*?)</select>',page,re.S)[1])==primary
print('PASS exactly 21 primary commands shared by firmware, Gateway and both command UIs')
