from pathlib import Path
import re,os,subprocess
r=Path(__file__).resolve().parents[1];source=(r/'src/core/network_service.cpp').read_text(encoding='utf-8');struct=re.search(r'struct Cached \{[^}]+\};',source).group();release=re.search(r'if\(gateway\)for\(auto& entry:cache\)if\(entry.responseId==m.correlationId\)\{[^}]+\}',source).group()
code='''#include "../test/protocol_stubs/Arduino.h"
#include <cassert>
STRUCT
Cached cache[2];struct Receipt{String correlationId;};
void persist(bool gateway,const Receipt& m){RELEASE}
int main(){cache[0].id="CMD-1";cache[0].request="PING";cache[0].response="full-result";cache[0].responseId="RESULT-1";cache[1].response="other-result";cache[1].responseId="RESULT-2";
persist(false,{"RESULT-1"});assert(cache[0].response=="full-result");persist(true,{"UNKNOWN"});assert(cache[0].response=="full-result");persist(true,{"RESULT-1"});assert(cache[0].response.length()==0);assert(cache[0].responseId.length()==0);assert(cache[0].id=="CMD-1");assert(cache[0].request=="PING");assert(cache[1].response=="other-result");}
'''.replace('STRUCT',struct).replace('RELEASE',release)
cpp=r/'.pio/command_cache_release.cpp';cpp.write_text(code,encoding='utf-8');compiler=Path.home()/'.platformio/packages/toolchain-gccmingw32/bin/g++.exe';env=dict(os.environ);env['PATH']=str(compiler.parent)+os.pathsep+env['PATH'];out=r/'.pio/command_cache_release.exe';subprocess.run([str(compiler),'-std=c++11','-static',str(cpp),'-o',str(out)],check=True,env=env);subprocess.run([str(out)],check=True,env=env);print('PASS production cache release: only matching Gateway persistence frees reply; request identity and unrelated replies preserved')
