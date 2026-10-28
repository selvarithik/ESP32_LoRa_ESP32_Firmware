from pathlib import Path
import os,subprocess
r=Path(__file__).resolve().parents[1];cc=Path.home()/'.platformio/packages/toolchain-gccmingw32/bin/g++.exe';env=dict(os.environ);env['PATH']=str(cc.parent)+os.pathsep+env['PATH'];out=r/'.pio/mqtt_recovery.exe';subprocess.run([str(cc),'-std=c++11','-static',str(r/'test/mqtt_recovery_host.cpp'),'-o',str(out)],check=True,env=env);subprocess.run([str(out)],check=True,env=env)
