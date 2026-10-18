"""Transfer a Windows DPAPI-protected local Gateway token to its ESP32 over USB."""
import argparse
import ctypes
import json
from pathlib import Path
import re
from device_acceptance import usb

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--token-file',type=Path,required=True)
    args=parser.parse_args()
    class Blob(ctypes.Structure):
        _fields_=[('size',ctypes.c_ulong),('data',ctypes.POINTER(ctypes.c_ubyte))]
    raw=args.token_file.read_bytes()
    buffer=ctypes.create_string_buffer(raw)
    source=Blob(len(raw),ctypes.cast(buffer,ctypes.POINTER(ctypes.c_ubyte)))
    target=Blob()
    if not ctypes.windll.crypt32.CryptUnprotectData(ctypes.byref(source),None,None,None,None,1,ctypes.byref(target)):
        raise ValueError('Protected credential unavailable')
    try:credential=json.loads(ctypes.string_at(target.data,target.size))
    finally:ctypes.windll.kernel32.LocalFree(target.data)
    state=usb({'op':'status'})
    if credential.get('device_id')!=state.get('device_id') or not re.fullmatch('[0-9a-f]{64}',credential.get('gateway_token','')):
        raise ValueError('Identity or credential format differs')
    result=usb({'op':'config','config':{'gateway_token':credential['gateway_token']}})
    print('USB credential provisioning saved; restart required.' if result.get('ok') else 'USB provisioning rejected; inspect commissioning state.')

if __name__=='__main__':
    try:main()
    except Exception:raise SystemExit('Local provisioning failed; no credential details displayed.')
