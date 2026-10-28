"""Physical USB read-only public E220 settings inspection; never enables RF or writes registers."""
import argparse,json,time
import serial

def exchange(port,value,seconds=5):
 raw=json.dumps(value,separators=(',',':')).encode()+b'\n';port.write(raw);deadline=time.monotonic()+seconds
 while time.monotonic()<deadline:
  line=port.readline()
  if line.startswith(b'[COMMISSION] '):
   try:return json.loads(line[13:])
   except (ValueError,UnicodeError):continue
 raise RuntimeError('USB_RESPONSE_UNAVAILABLE')
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--port',required=True);parser.add_argument('--read-only',action='store_true',required=True);args=parser.parse_args()
 port=serial.Serial(port=None,baudrate=115200,timeout=.2,write_timeout=2);port.dtr=False;port.rts=False;port.port=args.port
 try:
  port.open();port.reset_input_buffer();before=exchange(port,{'op':'status'})
  print('Physical device:',before.get('device_id'),'role:',before.get('role'))
  if before.get('radio',{}).get('state')=='READY_LINK_UNVERIFIED':
   print(json.dumps({'device_id':before.get('device_id'),'radio':before['radio'],'source':'BOOT_TIME_READ_ONLY_INSPECTION'},indent=2));return 0
  accepted=exchange(port,{'op':'radio_probe','confirm':'READ_ONLY_E220_SETTINGS'})
  if not accepted.get('ok'):raise RuntimeError('PROBE_NOT_ACCEPTED; inspect current radio state locally')
  for _ in range(8):
   time.sleep(.5);status=exchange(port,{'op':'status'});radio=status.get('radio',{})
   if radio.get('state')!='INSPECTING':
    print(json.dumps({'device_id':status.get('device_id'),'radio':radio},indent=2));return 0 if radio.get('public_settings_read') else 1
  raise RuntimeError('PROBE_OUTCOME_UNKNOWN; inspect before retrying')
 finally:port.close()
if __name__=='__main__':
 try:raise SystemExit(main())
 except (serial.SerialException,OSError,RuntimeError) as error:print(str(error));raise SystemExit(1)
