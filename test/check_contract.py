"""Static contract coverage/parity audit, not a runtime conformance test."""
from pathlib import Path
import hashlib
import re

root = Path(__file__).resolve().parents[1]
gateway = root.parent / 'ESP32_LoRa_Project'
raw = (root / 'CONFIG_CONTRACT.md').read_bytes()
assert raw == (gateway / 'CONFIG_CONTRACT.md').read_bytes(), 'Contract bytes differ'
contract = raw.decode('utf-8')
assert not raw.startswith(b'\xef\xbb\xbf') and b'\r' not in raw
fields = set()
schema = (root / 'src/config/config_schema.h').read_text()
for _, declaration in re.findall(r'^\s*(String|bool|uint8_t|uint16_t|uint32_t|int|DeviceRole)\s+([^;]+);', schema, re.M):
    fields.update(x.split('=')[0].strip() for x in declaration.split(','))
legacy = {'mqtt_host', 'mqtt_port', 'mqtt_tls', 'mqtt_user', 'mqtt_password', 'mqtt_ca', 'mqtt_keepalive'}
for field in fields - legacy:
    assert '| '+field+' |' in contract, 'Uninventoried '+field
for field in legacy:
    assert '| '+field+' |' in contract, 'Missing migration mapping '+field
profile = ['profile_name','provider','host','port','mqtt_version','tls','ca_cert','username','password','client_id','keepalive_s','timeout_s','qos_default','topic_prefix','gateway_id']
header = (root / 'src/config/mqtt_contract.h').read_text()
actual = re.findall(r'"([a-z_/]+)"', re.search(r'fields\[\].*?\{(.*?)\}', header, re.S)[1])
assert actual == profile
for name in profile:
    assert '| profile.'+name+' |' in contract
types = re.findall(r'"([a-z_/]+)"', re.search(r'messageTypes\[\].*?\{(.*?)\}', header, re.S)[1])
assert types == ['telemetry','status','command','command_ack','config','events','discovery','chat/message','chat/ack','chat/status','chat/command']
for name in types:
    assert '| '+name+' |' in contract
assert 'ENABLE_E220_DRIVER' not in (root / 'platformio.ini').read_text()
print('PASS: identical contract; all '+str(len(fields))+' firmware settings inventoried; 15 profile fields and 11 topic entries match declaration')
print('SHA-256: '+hashlib.sha256(raw).hexdigest())
print('This static check does not test runtime behavior; see the separate parity, TLS integration and hardware results.')
