import unittest,sys,struct,hashlib
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from flash_layout import application_target,expected_flash
class LayoutTests(unittest.TestCase):
 def image(self):
  raw=bytearray(b'\xff'*0x400000)
  entries=[(1,0,0x10000,8192,'otadata'),(1,2,0x12000,0xe000,'nvs'),(64,0,0x20000,0xe0000,'at_customize'),(0,16,0x100000,0x180000,'ota_0'),(0,17,0x280000,0x180000,'ota_1')]
  for i,(kind,sub,start,size,label) in enumerate(entries):raw[0x8000+32*i:0x8020+32*i]=struct.pack('<HBBII16sI',0x50aa,kind,sub,start,size,label.encode(),0)
  return raw
 def test_factory_ota_slot_preserves_layout(self):
  raw=self.image();r=application_target(raw,b'different',True);self.assertEqual(r['application_offset'],0x100000);self.assertEqual(r['application_capacity'],0x180000);self.assertFalse(r['partition_matches_built_huge_app']);self.assertTrue(r['boot_metadata_repair_required'])
 def test_legacy_md5_upgrade_and_corruption_are_distinguished(self):
  raw=self.image();pos=0x8000+5*32
  raw[pos:pos+32]=b'\xeb\xeb'+b'\xff'*14+hashlib.md5(raw[0x8000:pos]).digest()
  self.assertFalse(application_target(raw,b'different',True)['boot_metadata_repair_required'])
  raw[pos+16]^=1
  with self.assertRaisesRegex(ValueError,'INVALID_PARTITION_TABLE_MD5'):application_target(raw,b'different',True)
 def test_no_implicit_layout_change(self):
  with self.assertRaisesRegex(ValueError,'LAYOUT_DIFFERS'):application_target(self.image(),b'different')
 def test_active_ota_rejected(self):
  raw=self.image();raw[0x10000]=1
  with self.assertRaisesRegex(ValueError,'ACTIVE_OTA'):application_target(raw,b'different',True)
 def test_nvs_new_version_rejected(self):
  raw=self.image();raw[0x12000]=0xfc;raw[0x12008]=0xfd
  with self.assertRaisesRegex(ValueError,'NVS_VERSION'):application_target(raw,b'different',True)
 def test_legacy_nvs_version_is_supported(self):
  raw=self.image();raw[0x12000]=0xfc;raw[0x12008]=0xff
  self.assertEqual(application_target(raw,b'different',True)['application_offset'],0x100000)
 def test_expected_flash_preserves_every_other_region(self):
  raw=bytes([37])*0x400000;image=b'new-app'*1000
  expected,end=expected_flash(raw,image,0x100000,0x180000)
  self.assertEqual(expected[:0x100000],raw[:0x100000])
  self.assertEqual(expected[0x100000:0x100000+len(image)],image)
  self.assertEqual(expected[0x100000+len(image):end],b'\xff'*(end-0x100000-len(image)))
  self.assertEqual(expected[end:],raw[end:])
 def test_oversized_application_rejected(self):
  with self.assertRaisesRegex(ValueError,'OUT_OF_BOUNDS'):expected_flash(b'\xff'*8192,b'x'*8193,0,8192)
if __name__=='__main__':unittest.main()
