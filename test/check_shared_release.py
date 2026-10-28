"""Offline release evidence: no serial port, sockets, migrations or running application."""
from pathlib import Path
import hashlib,json,re,sqlite3,subprocess,sys

root=Path(__file__).resolve().parents[1]
gateway=root.parent/'ESP32_LoRa_Project'
sha=lambda data:hashlib.sha256(data).hexdigest()
evidence={'stage':'A_BUILD_ONLY','hardware_tests':'NOT_TESTED','live_broker_tests':'NOT_APPROVED_NOT_RUN'}
contract=(root/'CONFIG_CONTRACT.md').read_bytes()
assert contract==(gateway/'CONFIG_CONTRACT.md').read_bytes()
evidence['contract_sha256']=sha(contract)
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
    assert by_name['app0']['offset']==0x10000 and by_name['app0']['size']==0x140000
    assert by_name['app1']['offset']==0x150000 and by_name['app1']['size']==0x140000
    assert by_name['nvs']['offset']==0x9000 and by_name['nvs']['size']==0x5000
    assert by_name['spiffs']['offset']==0x290000 and by_name['spiffs']['size']==0x160000
    assert len(image)<=0x140000 and program<=0x140000
    builds[role]={'program_bytes':program,'application_capacity_bytes':0x140000,'program_percent':round(program/0x140000*100,2),'static_ram_bytes':ram,'ram_capacity_bytes':327680,'ram_percent':round(ram/327680*100,2),'image_bytes':len(image),'image_headroom_bytes':0x140000-len(image),'image_sha256':sha(image),'partition_sha256':sha(table),'partitions':entries}
assert builds['master']['partition_sha256']==builds['slave']['partition_sha256']
evidence['builds']=builds
baseline=json.loads((root/'docs/SOFTWARE_EXCLUSIONS_BASELINE.json').read_text())
assert all(Path(p).is_file() and sha(Path(p).read_bytes())==digest for p,digest in baseline.items())
evidence['excluded_chat_ota_baseline_unchanged']=True
preservation=json.loads((gateway/'data/shared-firmware-preservation.json').read_text())
with sqlite3.connect((gateway/'data/gateway.db').as_uri()+'?mode=ro',uri=True) as db:
    document=db.execute('SELECT document FROM mqtt_configuration WHERE id=1').fetchone()[0]
assert sha(document.encode())==preservation['active_mqtt_document_sha256']
assert all(sha((gateway/'data'/name).read_bytes())==digest for name,digest in preservation['protected_files'].items())
evidence['active_mqtt_document_and_protected_stores_unchanged']=True
# Values remain in this process; never print or include them in evidence.
sys.path.insert(0,str(gateway/'backend'))
from mqtt_store import SecretFile
secrets=SecretFile(gateway/'data/mqtt-secrets.bin').read()
values=[v.encode() for k,v in secrets.items() if k!='device_id' and isinstance(v,str) and len(v)>=8]
files=[p for base in (root/'src',root/'deployment/prepared',gateway/'deployment/prepared') for p in base.rglob('*') if p.is_file()]
files.extend(root/'.pio/build'/('esp32_'+role)/'firmware.bin' for role in ('master','slave'))
assert not any(secret in p.read_bytes() for p in files for secret in values)
evidence['stored_mqtt_secrets_absent_from_checked_source_profiles_images']=True
evidence['gateway_tests']=json.loads((gateway/'docs/SOFTWARE_TEST_RESULTS.json').read_text())
for destination in (root,gateway):
    (destination/'docs/SHARED_FIRMWARE_EVIDENCE.json').write_text(json.dumps(evidence,indent=2)+'\n',encoding='utf-8')
print('PASS: both artifacts fit; identical preserved partition tables; contract parity; excluded files unchanged; active MQTT document and protected stores unchanged; stored MQTT secrets absent from checked source/profiles/images')
for role,build in builds.items():print(role,json.dumps({k:v for k,v in build.items() if k!='partitions'}))
