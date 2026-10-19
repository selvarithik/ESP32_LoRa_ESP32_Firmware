"""Read-only indicator acceptance on the already commissioned COM8 board."""
import argparse,json,time,sqlite3
from pathlib import Path
from device_acceptance import usb
ROOT=Path(__file__).resolve().parents[1]
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--live',action='store_true',required=True);parser.parse_args()
 samples=[];state={}
 for _ in range(8):
  state=usb({'op':'status'});samples.append(state.get('indicators',{}));time.sleep(1)
 indicators=samples[-1];identity=state['device_id'];gateway=ROOT.parent/'ESP32_LoRa_Project'
 db=sqlite3.connect((gateway/'data/gateway.db').as_uri()+'?mode=ro',uri=True);db.row_factory=sqlite3.Row
 try:
  sensor_values=[dict(r) for r in db.execute("SELECT t.parameter_id,t.value,t.timestamp,t.source FROM telemetry_values t JOIN (SELECT parameter_id,MAX(id) id FROM telemetry_values WHERE node_id=? AND source='local-ip' GROUP BY parameter_id) n ON n.id=t.id",(identity,))]
 finally:db.close()
 report=dict(scope='Physical ESP32 runtime metadata and real Gateway database; driver write attempts do not prove visual output',device_id=identity,station_ip=state.get('station_ip'),role=state.get('role'),indicators=indicators,observed_display_texts=sorted({s.get('display_text','') for s in samples}),runtime_checks_passed=bool(indicators.get('server_connected') and indicators.get('led_on_requested') and indicators.get('display_enabled') and indicators.get('display_acknowledged') and indicators.get('display_clk_gpio')==18 and indicators.get('display_dio_gpio')==19),visual_confirmation='NOT_ASSERTED_BY_AUTOMATION',sensor_measurements=sensor_values,mqtt_revision=state.get('mqtt',{}).get('revision'),mqtt_active_fingerprint_preserved=state.get('mqtt',{}).get('active_fingerprint')=='2c327b3f89115315111e80b0461e5b6c8ee21d499452ca08eed8447a05af7ee3')
 for project in (ROOT,gateway):(project/'docs/INDICATOR_PHYSICAL_EVIDENCE.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
 print(json.dumps(report,indent=2));return 0 if report['runtime_checks_passed'] else 1
if __name__=='__main__':raise SystemExit(main())
