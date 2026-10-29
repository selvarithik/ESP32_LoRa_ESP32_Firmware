from pathlib import Path
import os,subprocess
root=Path(__file__).resolve().parents[1]
cc=Path.home()/'.platformio/packages/toolchain-gccmingw32/bin/g++.exe'
env=dict(os.environ);env['PATH']=str(cc.parent)+os.pathsep+env['PATH']
out=root/'.pio/operational_host.exe'
subprocess.run([str(cc),'-std=c++11','-static','-I'+str(root/'test/transport_stubs'),'-I'+str(root/'.pio/libdeps/esp32dev/ArduinoJson/src'),str(root/'test/operational_host.cpp'),'-o',str(out)],env=env,check=True)
subprocess.run([str(out)],env=env,check=True)
