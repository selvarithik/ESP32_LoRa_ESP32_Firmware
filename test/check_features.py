from pathlib import Path
import os,subprocess
root=Path(__file__).resolve().parents[1];compiler=Path.home()/'.platformio/packages/toolchain-gccmingw32/bin/g++.exe';out=root/'.pio/features_host.exe';env=dict(os.environ);env['PATH']=str(compiler.parent)+os.pathsep+env['PATH']
subprocess.run([str(compiler),'-std=c++11','-static',str(root/'test/features_host.cpp'),'-o',str(out)],check=True,env=env)
subprocess.run([str(out)],check=True,env=env)
