from pathlib import Path
import os, subprocess
root=Path(__file__).resolve().parents[1]
cc=Path.home()/'.platformio/packages/toolchain-gccmingw32/bin/g++.exe'
env=dict(os.environ);env['PATH']=str(cc.parent)+os.pathsep+env['PATH']
out=root/'.pio/shared_controls.exe'
subprocess.run([str(cc),'-std=c++11','-static',str(root/'test/shared_controls_host.cpp'),'-o',str(out)],env=env,check=True)
subprocess.run([str(out)],env=env,check=True)
out=root/'.pio/transport_trial.exe'
subprocess.run([str(cc),'-std=c++11','-static','-DTRANSPORT_TRIAL_HOST_TEST','-I'+str(root/'test/transport_stubs'),'-I'+str(root/'.pio/libdeps/esp32dev/ArduinoJson/src'),str(root/'test/transport_trial_host.cpp'),str(root/'src/config/transport_trial.cpp'),'-o',str(out)],env=env,check=True)
subprocess.run([str(out)],env=env,check=True)
