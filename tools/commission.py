"""Physical USB commissioning. Values are never echoed or printed.
Close the serial monitor first. This tool does not flash or erase the device.
"""
import argparse
import getpass
import json
from pathlib import Path
import time
import serial

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', required=True)
    parser.add_argument('--operation', choices=['password', 'wifi', 'mqtt','recover-account','apply-build-role'], required=True)
    parser.add_argument('--confirm-device-id',help='Exact physical device ID required for account recovery or build-role commissioning')
    parser.add_argument('--profile', type=Path)
    parser.add_argument('--revision', type=int, default=0)
    args = parser.parse_args()
    if args.operation in ('recover-account','apply-build-role'):
        if not args.confirm_device_id:parser.error('This physical operation requires --confirm-device-id')
        request={'op':'recover_account' if args.operation=='recover-account' else 'apply_build_role','confirm_device_id':args.confirm_device_id}
    elif args.operation == 'password':
        password = getpass.getpass('New administrator password (12..64 characters): ')
        if password != getpass.getpass('Confirm new password: '):
            raise SystemExit('Password confirmation differs')
        request = {'op': 'password', 'password': password}
    elif args.operation == 'wifi':
        request = {'op': 'config', 'config': {
            'wifi_ssid': input('SSID: '), 'wifi_password': getpass.getpass('Wi-Fi password: '),
            'setup_password': getpass.getpass('Recovery AP password (10..63 characters): ')}}
    else:
        if not args.profile:
            parser.error('--profile must name a non-secret canonical profile JSON file')
        profile = json.loads(args.profile.read_text(encoding='utf-8'))
        profile['password'] = getpass.getpass('MQTT password: ')
        request = {'op': 'mqtt', 'request': {'expected_revision': args.revision, 'profile': profile}}
    payload = json.dumps(request, separators=(',', ':')).encode() + b'\n'
    if len(payload) > 8192:
        raise SystemExit('Commissioning request exceeds the firmware limit')
    # Leave DTR/RTS inactive; do not intentionally reset the board.
    port = serial.Serial(port=None, baudrate=115200, timeout=.2, write_timeout=5)
    port.dtr = False
    port.rts = False
    port.port = args.port
    try:
        port.open()
        port.reset_input_buffer()
        for offset in range(0,len(payload),64):
            port.write(payload[offset:offset+64]);time.sleep(.03)
        deadline = time.monotonic() + 60
        while time.monotonic() < deadline:
            line = port.readline()
            if line.startswith(b'[COMMISSION] '):
                result = json.loads(line[len(b'[COMMISSION] '):])
                print('Commissioning succeeded' if result.get('ok') else 'Commissioning rejected; inspect settings and revision')
                return 0 if result.get('ok') else 1
        print('No commissioning response; outcome unknown. Inspect status before retrying.')
        return 2
    except (serial.SerialException, OSError):
        print('Serial connection unavailable. Close the monitor and check the port.')
        return 2
    finally:
        port.close()

if __name__ == '__main__':
    raise SystemExit(main())
