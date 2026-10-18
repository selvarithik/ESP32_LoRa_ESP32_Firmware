"""Local secret-presence check. Prints counts/paths only, never values."""
from pathlib import Path
import subprocess
import sys

root=Path(__file__).resolve().parents[1]
gateway=root.parent/'ESP32_LoRa_Project'
sys.path.insert(0,str(gateway/'backend'))
from mqtt_store import Store
profile=Store(gateway/'data/gateway.db',gateway/'data/mqtt-secrets.bin').profile()
secrets=[profile['password'].encode()]
result=subprocess.run(['netsh','wlan','show','profile','name=SRI SKANDA PG 301','key=clear'],capture_output=True,text=True)
for line in result.stdout.splitlines():
    if 'Key Content' in line:
        value=line.split(':',1)[1].strip()
        if value:secrets.append(value.encode())
skip={'.pio','.venv','.git','data','__pycache__','node_modules','.vscode'}
suffixes={'.py','.cpp','.h','.html','.js','.cjs','.md','.json','.ini','.txt','.ps1','.yaml','.yml','.csv'}
findings=[];count=0
for project in (root,gateway):
    for p in project.rglob('*'):
        if any(part in skip for part in p.relative_to(project).parts) or not p.is_file() or p.suffix.lower() not in suffixes:continue
        raw=p.read_bytes();count+=1
        if any(secret and secret in raw for secret in secrets):findings.append(str(p))
binary=root/'.pio/build/esp32dev/firmware.bin'
if any(secret and secret in binary.read_bytes() for secret in secrets):findings.append(str(binary))
print(f'Checked {count} source/document files and the final firmware image; private-default findings: {len(findings)}')
for p in findings:print(p)
raise SystemExit(bool(findings))
