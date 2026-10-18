from pathlib import Path
import os
import subprocess
root=Path(__file__).resolve().parents[1]
compiler=Path.home()/'.platformio/packages/toolchain-gccmingw32/bin/g++.exe'
env=dict(os.environ);env['PATH']=str(compiler.parent)+os.pathsep+env['PATH']
exe=root/'.pio/lora_host.exe'
subprocess.run([str(compiler),'-std=c++11','-static','-DENABLE_E220_DRIVER','-I'+str(root/'test/host_stubs'),str(root/'src/communication/lora_transport.cpp'),str(root/'test/lora_host.cpp'),'-o',str(exe)],check=True,env=env)
subprocess.run([str(exe)],check=True,env=env)
