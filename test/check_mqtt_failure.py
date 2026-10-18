from pathlib import Path
import os
import subprocess
root=Path(__file__).resolve().parents[1];compiler=Path.home()/'.platformio/packages/toolchain-gccmingw32/bin/g++.exe'
env=dict(os.environ);env['PATH']=str(compiler.parent)+os.pathsep+env['PATH'];exe=root/'.pio/mqtt_failure_host.exe'
subprocess.run([str(compiler),'-std=c++11','-static',str(root/'test/mqtt_failure_host.cpp'),'-o',str(exe)],check=True,env=env)
subprocess.run([str(exe)],check=True,env=env)
