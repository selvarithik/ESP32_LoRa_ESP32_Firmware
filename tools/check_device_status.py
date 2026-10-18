"""Read-only USB status and unauthenticated HTTP rejection evidence."""
import json
from pathlib import Path
import urllib.request
import urllib.error
from device_acceptance import usb

root=Path(__file__).resolve().parents[1]
state=usb({'op':'status'})
report={'station_ip':state['station_ip'],'shared_revision':state['mqtt']['shared_revision'],
        'mqtt_state':state['mqtt']['runtime']['state'],'pending':state['mqtt']['pending_configured'],
        'free_8bit_heap':state['free_8bit_heap'],'largest_8bit_block':state['largest_8bit_block'],
        'unauthenticated_routes':{}}
for path in ('/','/mqtt','/api/status','/api/config','/api/mqtt'):
    try:
        with urllib.request.urlopen('http://'+state['station_ip']+path,timeout=10) as response:
            code=response.status
    except urllib.error.HTTPError as error:
        code=error.code;error.close()
    report['unauthenticated_routes'][path]=code
report['passed']=all(code==401 for code in report['unauthenticated_routes'].values()) and report['mqtt_state']=='connected' and not report['pending']
for project in (root,root.parent/'ESP32_LoRa_Project'):
    (project/'docs/PHYSICAL_FINAL_EVIDENCE.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report,indent=2))
raise SystemExit(not report['passed'])
