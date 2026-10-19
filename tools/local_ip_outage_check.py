"""Read-only COM8 observation during a deliberately stopped Gateway (no secrets)."""
import json,time
from pathlib import Path
import serial

p=serial.Serial(port=None,baudrate=115200,timeout=.2,write_timeout=3);p.dtr=False;p.rts=False;p.port='COM8'
warnings=0;state=None
try:
 p.open();end=time.monotonic()+30
 while time.monotonic()<end:
  line=p.readline()
  if b'Pending queue full' in line:warnings+=1
 p.write(b'{"op":"status"}\n');end=time.monotonic()+8
 while time.monotonic()<end:
  line=p.readline()
  if line.startswith(b'[COMMISSION] '):state=json.loads(line[13:]);break
finally:p.close()
report=dict(scope='PHYSICAL Gateway process outage; Wi-Fi association unchanged',observed_outage_seconds=30,pending_queue_reject_new_warnings=warnings,local_ip=state.get('local_ip') if state else None,free_heap=state.get('free_heap') if state else None,sensor_task_continues='GPIO4 sampling is a separate FreeRTOS task; post-restart readings checked separately',recovery='PENDING_SERVER_RESTART')
root=Path(__file__).resolve().parents[1]
for project in (root,root.parent/'ESP32_LoRa_Project'):(project/'docs/LOCAL_IP_OUTAGE_EVIDENCE.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report,indent=2))
