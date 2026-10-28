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
