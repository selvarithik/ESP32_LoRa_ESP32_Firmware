"""Offline release evidence: no serial port, sockets, migrations or running application."""
from pathlib import Path
import hashlib,json,subprocess,sys

root=Path(__file__).resolve().parents[1]
gateway=root.parent/'ESP32_LoRa_Project'
sha=lambda data:hashlib.sha256(data).hexdigest()
contract=(root/'CONFIG_CONTRACT.md').read_bytes()
assert contract==(gateway/'CONFIG_CONTRACT.md').read_bytes()
compiler=Path.home()/'.platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-size.exe'
builds={}
for role in ('master','slave'):
    directory=root/'.pio/build'/('esp32_'+role)
    sections={}
    for line in subprocess.check_output([str(compiler),'-A',str(directory/'firmware.elf')],text=True).splitlines():
        parts=line.split()
        if len(parts)>1 and parts[0].startswith('.') and parts[1].isdigit():sections[parts[0]]=int(parts[1])
    program=sum(sections.get(k,0) for k in ('.iram0.text','.iram0.vectors','.dram0.data','.flash.text','.flash.rodata'))
    ram=sum(sections.get(k,0) for k in ('.dram0.data','.dram0.bss','.noinit'))
    image=(directory/'firmware.bin').read_bytes()
    table=(directory/'partitions.bin').read_bytes()
    entries=[]
    for offset in range(0,len(table),32):
        row=table[offset:offset+32]
        if row[:2]!=b'\xaa\x50':break
        entries.append({'name':row[12:28].split(b'\0')[0].decode(),'type':row[2],'subtype':row[3],'offset':int.from_bytes(row[4:8],'little'),'size':int.from_bytes(row[8:12],'little')})
    for first,second in zip(entries,entries[1:]):assert first['offset']+first['size']<=second['offset']
    assert max(e['offset']+e['size'] for e in entries)<=4*1024*1024
    by_name={e['name']:e for e in entries}
    assert by_name['app0']['offset']==0x10000 and by_name['app0']['size']==0x300000
    assert 'app1' not in by_name
    assert by_name['nvs']['offset']==0x9000 and by_name['nvs']['size']==0x5000
    assert by_name['spiffs']['offset']==0x310000 and by_name['spiffs']['size']==0xE0000
    capacity=by_name['app0']['size']
    assert len(image)<=capacity and program<=capacity
    builds[role]={'program_bytes':program,'application_capacity_bytes':capacity,'program_percent':round(program/capacity*100,2),'static_ram_bytes':ram,'ram_capacity_bytes':327680,'ram_percent':round(ram/327680*100,2),'image_bytes':len(image),'image_headroom_bytes':capacity-len(image),'image_sha256':sha(image),'partition_sha256':sha(table),'partitions':entries}
assert builds['master']['partition_sha256']==builds['slave']['partition_sha256']
# Compare the release's app-only images with both deployed slot capacities.
# The legacy Slave OTA layout is not the PlatformIO build layout.
assert all(build['image_bytes'] <= 0x180000 for build in builds.values())
# Values remain in this process; never print or include them in evidence.
sys.path.insert(0,str(gateway/'backend'))
from mqtt_store import SecretFile
secrets=SecretFile(gateway/'data/mqtt-secrets.bin').read()
values=[v.encode() for k,v in secrets.items() if k!='device_id' and isinstance(v,str) and len(v)>=8]
files=[p for base in (root/'src',root/'deployment/prepared',gateway/'deployment/prepared') for p in base.rglob('*') if p.is_file()]
files.extend(root/'.pio/build'/('esp32_'+role)/'firmware.bin' for role in ('master','slave'))
assert not any(secret in p.read_bytes() for p in files for secret in values)
print('PASS: both artifacts fit the build layout and deployed Slave app slot; identical partition tables; contract parity; stored MQTT secrets absent from checked artifacts')
for role,build in builds.items():print(role,json.dumps({k:v for k,v in build.items() if k!='partitions'}))
