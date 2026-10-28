from pathlib import Path
import os,subprocess
root=Path(__file__).resolve().parents[1];compiler=Path.home()/'.platformio/packages/toolchain-gccmingw32/bin/g++.exe';environment=dict(os.environ);environment['PATH']=str(compiler.parent)+os.pathsep+environment['PATH']
for role in ('MASTER','SLAVE'):
 executable=root/'.pio'/('e220_'+role.lower()+'.exe')
 subprocess.run([str(compiler),'-std=c++11','-static','-DDEVICE_ROLE_'+role+'=1',str(root/'test/e220_core_host.cpp'),'-o',str(executable)],check=True,env=environment)
 subprocess.run([str(executable)],check=True,env=environment)
print('No physical UART, RF, GPIO or sensor action performed.')
