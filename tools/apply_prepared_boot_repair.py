"""Apply the owner's reviewed COM8 boot repair without repartitioning or NVS writes."""
import argparse,hashlib,json,subprocess,sys
from pathlib import Path
from preserved_app_upload import SecretFile
from flash_layout import application_target
ROOT=Path(__file__).resolve().parents[1]
ESP=Path.home()/'.platformio/packages/tool-esptoolpy/esptool.py'

def main():
 p=argparse.ArgumentParser();p.add_argument('--live',action='store_true',required=True);p.add_argument('--apply-approved-repair',action='store_true',required=True);p.add_argument('--confirm-device-id',required=True);a=p.parse_args()
 assert a.confirm_device_id=='ESP32-842F2AE81F84','EXPECTED_SLAVE_REQUIRED'
 source=ROOT/'deployment/prepared/slave-legacy-boot-repair-20261101'
 backup_folder=ROOT/'data/system-audit/slave-COM8-20261101-chunked'
 folder=ROOT/'data/system-audit/slave-COM8-20261101-approved-repair';folder.mkdir(parents=True,exist_ok=True)
 review=json.loads((source/'REVIEW.json').read_text());backup=json.loads((backup_folder/'backup.json').read_text())
 baseline=folder/'current-before.dpapi' if (folder/'current-before.dpapi').exists() else backup_folder/'backup.dpapi'
 baseline_meta=json.loads((folder/'current-before.json').read_text()) if baseline.parent==folder else backup
 assert baseline_meta.get('device_id')==a.confirm_device_id,'BASELINE_IDENTITY_DIFFERS'
 before=SecretFile.protect(baseline.read_bytes(),True)
 assert len(before)==0x400000 and hashlib.sha256(before).hexdigest()==baseline_meta.get('sha256',baseline_meta.get('backup_sha256')),'BACKUP_INTEGRITY_FAILED'
 assert backup['device_id']==a.confirm_device_id and review['prepared_only'] is True
 allowed=[0x1000,0x8000,0x100000];expected=bytearray(before);writes=[]
 for item,offset in zip(review['planned_writes'],allowed):
  assert int(item['offset'],16)==offset
  file=source/item['file'];data=file.read_bytes()
  assert len(data)==item['bytes'] and hashlib.sha256(data).hexdigest()==item['sha256'],'PREPARED_FILE_DIFFERS'
  end=offset+((len(data)+4095)//4096)*4096
  assert item['erased_sector_range']==[hex(offset),hex(end)]
  expected[offset:end]=b'\xff'*(end-offset);expected[offset:offset+len(data)]=data
  writes.extend([hex(offset),str(file)])
 table=(source/'partition-table-same-layout-with-md5.bin').read_bytes()
 assert table[:192]==before[0x8000:0x80c0],'PARTITION_ENTRIES_CHANGED'
 assert table[192:194]==b'\xeb\xeb' and table[208:224]==hashlib.md5(table[:192]).digest()
 assert (source/'bootloader.bin').read_bytes()[0]==0xe9
 assert (source/'bootloader.bin').read_bytes()[3]>>4==2,'BOOTLOADER_NOT_4MB'
 assert (source/'firmware.bin').read_bytes()==(ROOT/'.pio/build/esp32dev/firmware.bin').read_bytes(),'SHARED_IMAGE_DIFFERS'
 assert expected[0xf000:0x100000]==before[0xf000:0x100000],'NVS_OTA_CUSTOMIZATION_DIFFERS'
 assert expected[0x280000:]==before[0x280000:],'SECOND_SLOT_DIFFERS'
 layout=application_target(bytes(expected),(ROOT/'.pio/build/esp32dev/partitions.bin').read_bytes(),True)
 assert layout['application_offset']==0x100000 and layout['application_capacity']==0x180000 and not layout['boot_metadata_repair_required']
 def command(name,args,timeout=180):
  logpath=folder/(name+'.log')
  with logpath.open('w') as log:
   result=subprocess.run([sys.executable,'-u',str(ESP),'--chip','esp32','--port','COM8','--baud','115200','--before','default_reset','--after','no_reset',*args],stdout=log,stderr=subprocess.STDOUT,timeout=timeout)
  if result.returncode:raise RuntimeError(name+'_FAILED; inspect private log, no later action')
  return logpath.read_text()
 identity=command('identity',['flash_id'])
 assert '84:1f:e8:2a:2f:84' in identity.lower() and '4MB' in identity,'PHYSICAL_DEVICE_DIFFERS'
 original=folder/'before-private.bin';original.write_bytes(before)
 try:command('original-flash-verified',['verify_flash','0',str(original)])
 finally:original.unlink()
 print(json.dumps({'preflight_verified':True,'same_partition_entries':True,'physical_write_starting':True}),flush=True)
 command('approved-three-region-write',['write_flash',*writes])
 wanted=folder/'after-private.bin';wanted.write_bytes(expected)
 try:command('whole-flash-verified',['verify_flash','0',str(wanted)])
 finally:wanted.unlink()
 report={'baseline_sha256':hashlib.sha256(before).hexdigest(),'device_id':a.confirm_device_id,'port':'COM8','owner_approved_boot_metadata_repair':True,'physical_upload':True,'whole_flash_verified':'DEVICE_MD5_EXPECTED_IMAGE','partition_entries_unchanged':True,'nvs_ota_customization_second_slot_preserved_before_boot':True,'application_sha256':hashlib.sha256((source/'firmware.bin').read_bytes()).hexdigest(),'application_offset':'0x100000','application_capacity':0x180000,'bootloader_sha256':review['planned_writes'][0]['sha256'],'partition_table_sha256':review['planned_writes'][1]['sha256'],'reset_requested':False,'runtime_boot_verified':False}
 path=ROOT/'docs/SLAVE_APPROVED_BOOT_REPAIR_20261101.json'
 path.write_text(json.dumps(report,indent=2));(folder/'result.json').write_text(json.dumps(report,indent=2))
 result=subprocess.run([sys.executable,'-u',str(ESP),'--chip','esp32','--port','COM8','--before','no_reset','--after','hard_reset','run'],capture_output=True,text=True,timeout=30)
 (folder/'reset.log').write_text(result.stdout+result.stderr);report['reset_requested']=result.returncode==0
 path.write_text(json.dumps(report,indent=2));(folder/'result.json').write_text(json.dumps(report,indent=2));print(json.dumps(report),flush=True)

if __name__=='__main__':main()
