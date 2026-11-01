"""Physical USB read-only public E220 settings inspection; never enables RF or writes registers."""
import argparse,json,time
import serial

def exchange(port,value,seconds=5):
 raw=json.dumps(value,separators=(',',':')).encode()+b'\n'
 # Status frames can span several serial reads; never treat a partial JSON as a reply.
 for attempt in range(3 if value.get('op')=='status' else 1):
  port.reset_input_buffer();port.write(raw);deadline=time.monotonic()+seconds;buffer=b''
  while time.monotonic()<deadline:
   chunk=port.readline()
   if not chunk:continue
   buffer+=chunk
   if len(buffer)>32768:break
   if not buffer.endswith(b'\n'):continue
   line=buffer;buffer=b''
   if line.startswith(b'[COMMISSION] '):
    try:return json.loads(line[13:])
    except (ValueError,UnicodeError):break
 raise RuntimeError('USB_RESPONSE_UNAVAILABLE; outcome unknown, inspect status before retrying')

def main():
 parser=argparse.ArgumentParser();parser.add_argument('--port',required=True);parser.add_argument('--read-only',action='store_true',required=True);parser.add_argument('--uart',type=int,choices=[1,2],default=2,help='Temporary read-only UART peripheral; saved radio configuration is unchanged');parser.add_argument('--samples',action='store_true',help='Print phase and raw GPIO samples during the read-only probe');args=parser.parse_args()
 port=serial.Serial(port=None,baudrate=115200,timeout=.2,write_timeout=2);port.dtr=False;port.rts=False;port.port=args.port
 try:
  port.open();port.reset_input_buffer();before=exchange(port,{'op':'status'})
  print('Physical device:',before.get('device_id'),'role:',before.get('role'))
  if before.get('radio',{}).get('state')=='READY_LINK_UNVERIFIED':
   print(json.dumps({'device_id':before.get('device_id'),'radio':before['radio'],'source':'BOOT_TIME_READ_ONLY_INSPECTION'},indent=2));return 0
  request={'op':'radio_probe','confirm':'READ_ONLY_E220_SETTINGS'}
  if args.uart!=2:request['uart']=args.uart
  accepted=exchange(port,request)
  if not accepted.get('ok'):raise RuntimeError('PROBE_NOT_ACCEPTED; inspect current radio state locally')
  deadline=time.monotonic()+18
  while time.monotonic()<deadline:
   time.sleep(.1 if args.samples else .5);status=exchange(port,{'op':'status'});radio=status.get('radio',{})
   if args.samples:print(json.dumps({'phase':radio.get('probe_phase'),'query_sent':radio.get('settings_query_sent'),'response_bytes':radio.get('settings_response_bytes'),'attempt':radio.get('settings_read_attempts'),'uart':radio.get('uart_peripheral'),'actual_baud':radio.get('uart_actual_baud'),'pin_levels':radio.get('pin_levels'),'state':radio.get('state')}),flush=True)
   if radio.get('state')!='INSPECTING':
    print(json.dumps({'device_id':status.get('device_id'),'radio':radio},indent=2));return 0 if radio.get('public_settings_read') else 1
  raise RuntimeError('PROBE_OUTCOME_UNKNOWN; inspect before retrying')
 finally:port.close()
if __name__=='__main__':
 try:raise SystemExit(main())
 except (serial.SerialException,OSError,RuntimeError) as error:print(str(error));raise SystemExit(1)
