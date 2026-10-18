"""Inspect built artifacts only. Never reads or writes connected flash."""
from pathlib import Path
import json
import struct
import subprocess

root=Path(__file__).resolve().parents[1]
build=root/'.pio/build/esp32dev';raw=(build/'partitions.bin').read_bytes()
entries=[]
for offset in range(0,len(raw),32):
    magic,kind,sub,address,size,label,flags=struct.unpack('<HBBII16sI',raw[offset:offset+32])
    if magic!=0x50aa:break
    entries.append({'name':label.split(b'\0')[0].decode(),'type':kind,'subtype':sub,'offset':address,'size':size})
assert entries,'Missing partition entries'
previous=0x9000
for entry in sorted(entries,key=lambda entry:entry['offset']):
    assert entry['offset']>=previous,'Partition overlap or reserved boot region collision'
    previous=entry['offset']+entry['size'];assert previous<=0x400000,'Beyond verified 4 MB flash'
app=next(e for e in entries if e['name']=='app0');image=(build/'firmware.bin').read_bytes()
assert image[0]==0xe9 and image[3]>>4==2,'Image header must describe ESP32 4 MB flash'
assert len(image)<=app['size'],'Image does not fit selected app partition'
compiler=Path.home()/'.platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-size.exe'
output=subprocess.check_output([str(compiler),'-A',str(build/'firmware.elf')],text=True)
sections={parts[0]:int(parts[1]) for line in output.splitlines() if len(parts:=line.split())>=3 and parts[0].startswith('.') and parts[1].isdigit()}
# Match PlatformIO espressif32 builder's SIZEPROGREGEXP/SIZEDATAREGEXP.
program=sum(sections.get(name,0) for name in ('.iram0.text','.iram0.vectors','.dram0.data','.flash.text','.flash.rodata'))
static_ram=sum(sections.get(name,0) for name in ('.dram0.data','.dram0.bss','.noinit'))
report={'flash_capacity':0x400000,'partitions':entries,'application_capacity':app['size'],
        'program_bytes':program,'image_bytes':len(image),'image_headroom':app['size']-len(image),
        'program_headroom':app['size']-program,'static_ram_bytes':static_ram,
        'second_ota_slot':any(e['name']=='app1' for e in entries),
        'filesystem_bytes':next(e['size'] for e in entries if e['name']=='spiffs'),
        'physical_upload_performed':False,'passed':True}
(root/'docs/ACCESS_BUILD_EVIDENCE.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2))
