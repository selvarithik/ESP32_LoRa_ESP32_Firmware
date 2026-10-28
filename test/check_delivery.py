from pathlib import Path
import subprocess,os
root=Path(__file__).resolve().parents[1];compiler=Path.home()/'.platformio/packages/toolchain-gccmingw32/bin/g++.exe'
environment=dict(os.environ);environment['PATH']=str(compiler.parent)+os.pathsep+environment['PATH'];out=root/'.pio/delivery.exe'
subprocess.run([str(compiler),'-std=c++11','-static',str(root/'test/delivery_host.cpp'),'-o',str(out)],check=True,env=environment)
subprocess.run([str(out)],check=True,env=environment)
