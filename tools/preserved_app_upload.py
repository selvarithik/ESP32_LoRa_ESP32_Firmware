"""Owner-authorized application-only identity-confirmed deployment with verified encrypted backup."""
import argparse,subprocess,sys,json,hashlib,struct,time,os,ctypes,re
from pathlib import Path
from flash_layout import application_target,expected_flash
ROOT=Path(__file__).resolve().parents[1]
ESP=Path.home()/'.platformio/packages/tool-esptoolpy/esptool.py'
GATEWAY=ROOT.parent/'ESP32_LoRa_Project'
class SecretFile:
 @staticmethod
 def protect(raw,decrypt=False):
  class Blob(ctypes.Structure):_fields_=[('size',ctypes.c_ulong),('data',ctypes.POINTER(ctypes.c_ubyte))]
  buffer=ctypes.create_string_buffer(raw);source=Blob(len(raw),ctypes.cast(buffer,ctypes.POINTER(ctypes.c_ubyte)));target=Blob()
  call=ctypes.windll.crypt32.CryptUnprotectData if decrypt else ctypes.windll.crypt32.CryptProtectData
  if not call(ctypes.byref(source),None,None,None,None,1,ctypes.byref(target)):raise RuntimeError('BACKUP_PROTECTION_FAILED')
  try:return ctypes.string_at(target.data,target.size)
  finally:ctypes.windll.kernel32.LocalFree(target.data)
def digest(raw):return hashlib.sha256(raw).hexdigest()
def main():
 p=argparse.ArgumentParser();p.add_argument('--stage',choices=['backup','verify-backup','upload'],required=True);p.add_argument('--read-chunk-size',type=int,choices=[0,65536,131072,262144],default=0);p.add_argument('--baud',type=int,choices=[115200,230400,460800],default=460800);p.add_argument('--command-timeout',type=int,default=240);p.add_argument('--port',required=True);p.add_argument('--confirm-device-id',required=True);p.add_argument('--run-name',default='recovery-upload-20261029');p.add_argument('--live',action='store_true',required=True);p.add_argument('--allow-erased-ota-layout',action='store_true');p.add_argument('--verify-device-md5',action='store_true');a=p.parse_args()
 assert os.name=='nt','WINDOWS_ONLY'
 assert re.fullmatch(r'COM[1-9][0-9]*',a.port),'INVALID_COM_PORT'
 assert 30<=a.command_timeout<=1200,'INVALID_COMMAND_TIMEOUT'
 assert re.fullmatch(r'ESP32-[0-9A-F]{12}',a.confirm_device_id),'INVALID_DEVICE_ID'
 expected_mac=':'.join(f'{v:02x}' for v in bytes.fromhex(a.confirm_device_id[6:])[::-1])
 assert re.fullmatch(r'[A-Za-z0-9-]{1,64}',a.run_name),'INVALID_BACKUP_NAME'
 folder=ROOT/'data/system-audit'/a.run_name;folder.mkdir(parents=True,exist_ok=True)
 def command(name,args):
  logfile=folder/(name+'.log')
  with logfile.open('w') as log:
   result=subprocess.run([sys.executable,'-u',str(ESP),'--chip','esp32','--port',a.port,'--baud',str(a.baud),'--before','default_reset','--after','no_reset',*args],stdout=log,stderr=subprocess.STDOUT,text=True,timeout=a.command_timeout)
  output=logfile.read_text(errors='replace')
  if result.returncode:raise RuntimeError('ESPTOOL_'+name.upper()+'_FAILED; inspect private log, no later stage performed')
  return output
 def read(name):
  target=folder/(name+'.bin')
  if a.read_chunk_size:
   data=bytearray()
   for offset in range(0,0x400000,a.read_chunk_size):
    part=folder/(name+'-part.bin');length=min(a.read_chunk_size,0x400000-offset)
    command(name+'-'+format(offset,'06x'),['read_flash',hex(offset),hex(length),str(part)])
    chunk=part.read_bytes();assert len(chunk)==length,'PARTIAL_FLASH_READ';data.extend(chunk);part.unlink()
    print(json.dumps({'flash_read_bytes':len(data),'total_bytes':0x400000}),flush=True)
   target.write_bytes(data)
  else:command(name,['read_flash','0','0x400000',str(target)])
  raw=target.read_bytes();assert len(raw)==0x400000;return target,raw
 if a.stage in ('backup','verify-backup'):
  if a.stage=='backup' and (folder/'backup.dpapi').exists():raise RuntimeError('EXISTING_BACKUP_PRESERVED')
  identity=command('flash-id',['flash_id']);assert expected_mac in identity.lower(),'PHYSICAL_MAC_DIFFERS';assert '4MB' in identity,'FLASH_SIZE_NOT_4MB'
  if a.stage=='verify-backup':
   before=SecretFile.protect((folder/'backup.dpapi').read_bytes(),True);assert len(before)==0x400000
   one=folder/'reverify-private.bin';one.write_bytes(before)
  else:one,before=read('full-before')
  try:command('full-before-verify',['verify_flash','0',str(one)])
  finally:
   if a.stage=='verify-backup':one.unlink()
  built=(ROOT/'.pio/build/esp32dev/partitions.bin').read_bytes()
  if a.stage=='backup':
   encrypted=SecretFile.protect(before);assert SecretFile.protect(encrypted,True)==before;(folder/'backup.dpapi').write_bytes(encrypted)
   assert digest(SecretFile.protect((folder/'backup.dpapi').read_bytes(),True))==digest(before)
   one.unlink()
  layout=application_target(before,built,a.allow_erased_ota_layout)
  report={'port':a.port,'device_id':a.confirm_device_id,'flash_bytes':len(before),'backup_sha256':digest(before),'full_backup_verified':True,'backup_encrypted':True,'physical_upload':False,**layout}
  (folder/'backup.json').write_text(json.dumps(report,indent=2));print(json.dumps(report));return
 report=json.loads((folder/'backup.json').read_text());assert report['port']==a.port and report['device_id']==a.confirm_device_id,'BACKUP_IDENTITY_DIFFERS';identity=command('prewrite-identity',['flash_id']);assert expected_mac in identity.lower(),'PHYSICAL_MAC_DIFFERS';before=SecretFile.protect((folder/'backup.dpapi').read_bytes(),True);assert digest(before)==report['backup_sha256']
 built=(ROOT/'.pio/build/esp32dev/partitions.bin').read_bytes();layout=application_target(before,built,a.allow_erased_ota_layout)
 assert not layout['boot_metadata_repair_required'],'LEGACY_PARTITION_TABLE_REQUIRES_APPROVED_BOOT_METADATA_REPAIR_NO_UPLOAD'
 # The board has stayed in the ROM/stub after both full reads. Recheck identity
 # and protected regions immediately before writing; do not run the old app.
 app_offset=report.get('application_offset',0x10000);app_capacity=report.get('application_capacity',0x300000)
 if a.verify_device_md5:
  current_path=folder/'prewrite-expected.bin';current_path.write_bytes(before)
  try:command('prewrite-recheck',['verify_flash','0',str(current_path)])
  finally:current_path.unlink()
 else:
  current_path=folder/'prewrite-protected.bin';command('prewrite-recheck',['read_flash','0',hex(app_offset),str(current_path)]);current=current_path.read_bytes();assert current==before[:app_offset],'PROTECTED_REGIONS_CHANGED_NO_WRITE';current_path.unlink()
 image=(ROOT/'.pio/build/esp32dev/firmware.bin').read_bytes();assert len(image)<=app_capacity,'APPLICATION_DOES_NOT_FIT';assert image[3]>>4==2,'IMAGE_NOT_4MB'
 binary=ROOT/'.pio/build/esp32dev/firmware.bin';command('write-application',['write_flash',hex(app_offset),str(binary)])
 expected,end=expected_flash(before,image,app_offset,app_capacity)
 if a.verify_device_md5:
  after_path=folder/'full-after-expected.bin';after_path.write_bytes(expected)
  try:command('full-after-verify',['verify_flash','0',str(after_path)])
  finally:after_path.unlink()
  after=expected
 else:after_path,after=read('full-after')
 assert after[app_offset:app_offset+len(image)]==image,'APPLICATION_READBACK_DIFFERS'
 assert after[:app_offset]==before[:app_offset] and after[end:]==before[end:],'OUTSIDE_APPLICATION_DIFFERS_NO_RESET'
 assert after[0x8000:0x9000]==before[0x8000:0x9000],'PARTITION_TABLE_DIFFERS'
 report.update(full_flash_verification_method='DEVICE_MD5_EXPECTED_IMAGE' if a.verify_device_md5 else 'FULL_READBACK',physical_upload=True,application_bytes=len(image),application_sha256=digest(image),application_readback_verified=not a.verify_device_md5,application_verified=True,all_bytes_outside_application_unchanged=True,nvs_ota_partitions_bootloader_preserved=True,filesystem_preserved=True,reset_requested=False)
 (folder/'upload.json').write_text(json.dumps(report,indent=2));(ROOT/'docs'/('APPLICATION_UPLOAD_'+a.run_name+'.json')).write_text(json.dumps(report,indent=2));
 if after_path.exists():after_path.unlink()
 # All comparisons pass before booting. No erase, partition or filesystem write.
 result=subprocess.run([sys.executable,str(ESP),'--chip','esp32','--port',a.port,'--before','no_reset','--after','hard_reset','run'],capture_output=True,text=True,timeout=30)
 (folder/'reset.log').write_text(result.stdout+'\n'+result.stderr);report['reset_requested']=result.returncode==0;(folder/'upload.json').write_text(json.dumps(report,indent=2));(ROOT/'docs'/('APPLICATION_UPLOAD_'+a.run_name+'.json')).write_text(json.dumps(report,indent=2));print(json.dumps(report))
if __name__=='__main__':
 try:main()
 except Exception as e:raise SystemExit(type(e).__name__+': '+str(e))
