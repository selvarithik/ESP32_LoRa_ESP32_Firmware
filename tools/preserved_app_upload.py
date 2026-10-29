"""Owner-authorized application-only COM8 deployment with verified encrypted backup."""
import argparse,subprocess,sys,json,hashlib,struct,time,os,ctypes,re
from pathlib import Path
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
 p=argparse.ArgumentParser();p.add_argument('--stage',choices=['backup','upload'],required=True);p.add_argument('--port',required=True);p.add_argument('--run-name',default='recovery-upload-20261029');p.add_argument('--live',action='store_true',required=True);a=p.parse_args()
 assert os.name=='nt' and a.port=='COM8','WINDOWS_COM8_ONLY'
 assert re.fullmatch(r'[A-Za-z0-9-]{1,64}',a.run_name),'INVALID_BACKUP_NAME'
 folder=ROOT/'data/system-audit'/a.run_name;folder.mkdir(parents=True,exist_ok=True)
 def command(name,args):
  result=subprocess.run([sys.executable,str(ESP),'--chip','esp32','--port',a.port,'--baud','460800','--before','default_reset','--after','no_reset',*args],capture_output=True,text=True,timeout=240)
  (folder/(name+'.log')).write_text(result.stdout+'\n'+result.stderr)
  if result.returncode:raise RuntimeError('ESPTOOL_'+name.upper()+'_FAILED; inspect private log, no later stage performed')
  return result.stdout
 def read(name):
  target=folder/(name+'.bin');command(name,['read_flash','0','0x400000',str(target)]);raw=target.read_bytes();assert len(raw)==0x400000;return target,raw
 if a.stage=='backup':
  if (folder/'backup.dpapi').exists():raise RuntimeError('EXISTING_BACKUP_PRESERVED')
  identity=command('flash-id',['flash_id']);assert '78:21:84:7d:77:10' in identity.lower(),'MASTER_MAC_DIFFERS';assert '4MB' in identity,'FLASH_SIZE_NOT_4MB'
  one,before=read('full-before');command('full-before-verify',['verify_flash','0',str(one)])
  built=(ROOT/'.pio/build/esp32dev/partitions.bin').read_bytes();assert before[0x8000:0x8000+len(built)]==built,'PARTITION_LAYOUT_DIFFERS_NO_UPLOAD'
  encrypted=SecretFile.protect(before);assert SecretFile.protect(encrypted,True)==before;(folder/'backup.dpapi').write_bytes(encrypted)
  assert digest(SecretFile.protect((folder/'backup.dpapi').read_bytes(),True))==digest(before)
  report={'port':a.port,'device_id':'ESP32-10777D842178','flash_bytes':len(before),'backup_sha256':digest(before),'full_backup_verified':True,'backup_encrypted':True,'partition_matches_built_huge_app':True,'physical_upload':False}
  (folder/'backup.json').write_text(json.dumps(report,indent=2));one.unlink();print(json.dumps(report));return
 report=json.loads((folder/'backup.json').read_text());before=SecretFile.protect((folder/'backup.dpapi').read_bytes(),True);assert digest(before)==report['backup_sha256']
 # The board has stayed in the ROM/stub after both full reads. Recheck identity
 # and protected regions immediately before writing; do not run the old app.
 current_path=folder/'prewrite-protected.bin';command('prewrite-recheck',['read_flash','0','0x10000',str(current_path)]);current=current_path.read_bytes();assert current==before[:0x10000],'PROTECTED_REGIONS_CHANGED_NO_WRITE';current_path.unlink()
 image=(ROOT/'.pio/build/esp32dev/firmware.bin').read_bytes();assert len(image)<=0x300000;assert image[3]>>4==2,'IMAGE_NOT_4MB'
 binary=ROOT/'.pio/build/esp32dev/firmware.bin';command('write-application',['write_flash','0x10000',str(binary)])
 after_path,after=read('full-after');end=0x10000+((len(image)+4095)//4096)*4096
 assert after[0x10000:0x10000+len(image)]==image,'APPLICATION_READBACK_DIFFERS'
 assert after[:0x10000]==before[:0x10000] and after[end:]==before[end:],'OUTSIDE_APPLICATION_DIFFERS_NO_RESET'
 assert after[0x9000:0x10000]==before[0x9000:0x10000],'NVS_OR_OTA_DIFFERS'
 report.update(physical_upload=True,application_bytes=len(image),application_sha256=digest(image),application_readback_verified=True,all_bytes_outside_application_unchanged=True,nvs_ota_partitions_bootloader_preserved=True,filesystem_preserved=True,reset_requested=False)
 (folder/'upload.json').write_text(json.dumps(report,indent=2));(ROOT/'docs/MASTER_RECOVERY_UPLOAD_20261029.json').write_text(json.dumps(report,indent=2));after_path.unlink()
 # All comparisons pass before booting. No erase, partition or filesystem write.
 result=subprocess.run([sys.executable,str(ESP),'--chip','esp32','--port',a.port,'--before','no_reset','--after','hard_reset','run'],capture_output=True,text=True,timeout=30)
 (folder/'reset.log').write_text(result.stdout+'\n'+result.stderr);report['reset_requested']=result.returncode==0;(folder/'upload.json').write_text(json.dumps(report,indent=2));(ROOT/'docs/MASTER_RECOVERY_UPLOAD_20261029.json').write_text(json.dumps(report,indent=2));print(json.dumps(report))
if __name__=='__main__':
 try:main()
 except Exception as e:raise SystemExit(type(e).__name__+': '+str(e))
