"""Fail-closed application target selection; never changes partition/OTA data."""
import struct,hashlib

def expected_flash(raw,image,offset,capacity):
    if not image or len(image)>capacity or offset%4096 or offset+capacity>len(raw):
        raise ValueError('APPLICATION_OUT_OF_BOUNDS')
    end=offset+((len(image)+4095)//4096)*4096
    if end>offset+capacity:raise ValueError('APPLICATION_SECTOR_OUT_OF_BOUNDS')
    expected=bytearray(raw)
    expected[offset:end]=b'\xff'*(end-offset)
    expected[offset:offset+len(image)]=image
    return bytes(expected),end

def application_target(raw, built, allow_erased_ota=False):
    rows=[]
    for offset in range(0x8000,0x9000,32):
        entry=raw[offset:offset+32]
        if entry[:2]!=b'\xaaP':break
        _,kind,sub,start,size,label,flags=struct.unpack('<HBBII16sI',entry)
        if not size or start<0x9000 or start+size>len(raw) or flags:
            raise ValueError('UNSUPPORTED_OR_INVALID_PARTITION')
        rows.append(dict(kind=kind,sub=sub,start=start,size=size,label=label.rstrip(b'\0').decode('ascii')))
    if not rows:raise ValueError('PARTITION_TABLE_UNAVAILABLE')
    md5_offset=0x8000+32*len(rows)
    has_md5=raw[md5_offset:md5_offset+2]==b'\xeb\xeb'
    if has_md5 and raw[md5_offset+16:md5_offset+32]!=hashlib.md5(raw[0x8000:md5_offset]).digest():
        raise ValueError('INVALID_PARTITION_TABLE_MD5')
    ordered=sorted(rows,key=lambda r:r['start'])
    if any(a['start']+a['size']>b['start'] for a,b in zip(ordered,ordered[1:])):
        raise ValueError('OVERLAPPING_PARTITIONS')
    same=raw[0x8000:0x8000+len(built)]==built
    apps=[r for r in rows if r['kind']==0]
    if same:
        candidates=[r for r in apps if r['start']==0x10000]
    elif allow_erased_ota:
        ota=[r for r in rows if r['kind']==1 and r['sub']==0]
        if len(ota)!=1 or ota[0]['size']!=8192:raise ValueError('OTA_SELECTION_NOT_PROVEN')
        o=ota[0]
        if raw[o['start']:o['start']+o['size']]!=b'\xff'*o['size']:
            raise ValueError('ACTIVE_OTA_SELECTION_REQUIRES_REVIEW')
        # IDF boots the first OTA slot when OTA data is erased and no factory app exists.
        if any(r['sub']==0 for r in apps):raise ValueError('FACTORY_APPLICATION_REQUIRES_REVIEW')
        candidates=[r for r in apps if r['sub']==16]
        nvs=[r for r in rows if r['kind']==1 and r['sub']==2 and r['label']=='nvs']
        if len(nvs)!=1:raise ValueError('NVS_PARTITION_REQUIRED')
        n=nvs[0];pages=[raw[i:i+4096] for i in range(n['start'],n['start']+n['size'],4096)]
        if not any(p==b'\xff'*4096 for p in pages):raise ValueError('NVS_NO_ERASED_PAGE_NO_BOOT')
        if any(p!=b'\xff'*4096 and p[8] not in (0xff,0xfe) for p in pages):
            raise ValueError('NVS_VERSION_REQUIRES_REVIEW')
    else:raise ValueError('PARTITION_LAYOUT_DIFFERS_NO_UPLOAD')
    if len(candidates)!=1:raise ValueError('APPLICATION_TARGET_AMBIGUOUS')
    app=candidates[0]
    if app['start']%0x10000:raise ValueError('APPLICATION_ALIGNMENT_INVALID')
    return dict(application_offset=app['start'],application_capacity=app['size'],
                application_label=app['label'],partition_matches_built_huge_app=same,
                partition_md5_valid=has_md5,boot_metadata_repair_required=not has_md5)
