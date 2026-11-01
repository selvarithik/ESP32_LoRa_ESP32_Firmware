import os,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
compiler=Path.home()/'.platformio/packages/toolchain-gccmingw32/bin/g++.exe'
environment=dict(os.environ);environment['PATH']=str(compiler.parent)+os.pathsep+environment['PATH']
binary=root/'.pio/config_storage.exe'
subprocess.run([str(compiler),'-std=c++11','-static',str(root/'test/config_storage_host.cpp'),'-o',str(binary)],check=True,env=environment)
subprocess.run([str(binary)],check=True,env=environment)
