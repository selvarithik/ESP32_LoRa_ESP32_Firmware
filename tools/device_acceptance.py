"""Reset the ESP32 password over USB and test HTTP. Default: no typing, clipboard password."""
import base64
import argparse
import getpass
import json
from pathlib import Path
import time
import sys
import urllib.error
import urllib.request
import http.cookiejar
import serial
import secrets
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def usb(request):
    port = serial.Serial(port=None, baudrate=115200, timeout=.2)
    port.dtr = False
    port.rts = False
    port.port = 'COM8'
    try:
        port.open()
        data = json.dumps(request, separators=(',', ':')).encode() + b'\n'
        for offset in range(0, len(data), 64):
            port.write(data[offset:offset+64])
            time.sleep(.03)
        deadline = time.monotonic() + 60
        while time.monotonic() < deadline:
            line = port.readline()
            if line.startswith(b'[COMMISSION] '):
                return json.loads(line[13:])
        raise RuntimeError('USB response unavailable')
    finally:
        port.close()

def wait_ready(timeout=90):
    """Wait for actual transport readiness; USB time is an explicit commissioning source."""
    state=usb({'op':'status'})
    seeded=False
    if state.get('unix_time',0)<1700000000:
        if not usb({'op':'time','unix_time':int(time.time())}).get('ok'):
            raise RuntimeError('Clock commissioning failed')
        seeded=True
    deadline=time.monotonic()+timeout
    while time.monotonic()<deadline:
        state=usb({'op':'status'})
        if state.get('mqtt',{}).get('runtime',{}).get('state')=='connected':
            state['usb_clock_seeded']=seeded
            return state
        time.sleep(1)
    raise RuntimeError('Physical MQTT connection did not become ready')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--temporary', dest='temporary', action='store_true', help='Generate a clipboard password without typing (default)')
    mode.add_argument('--manual', dest='temporary', action='store_false', help='Choose a password using hidden terminal input')
    parser.set_defaults(temporary=True)
    parser.add_argument('--check-only',action='store_true',help='Use the current clipboard password without resetting credentials')
    args = parser.parse_args()
    if not args.temporary and not sys.stdin.isatty():
        print('Run this tool directly in your own PowerShell terminal; secret input requires a console.')
        return 1
    print('ESP32 local password reset and acceptance tests. Input stays on this computer.', flush=True)
    if args.temporary:
        if args.check_only:
            captured=subprocess.run(['powershell.exe','-NoProfile','-NonInteractive','-Command','Get-Clipboard -Raw'],capture_output=True,text=True)
            password=captured.stdout.rstrip('\r\n')
            if captured.returncode or not 1<=len(password.encode())<=64:
                print('Clipboard credential unavailable. No changes made.');return 1
            print('Check only: using the clipboard credential. No password reset.',flush=True)
        else:
            print('Automatic mode: no keyboard input needed. Provisioning over COM8; allow about 30 seconds.', flush=True)
            password = secrets.token_urlsafe(18)
    else:
        print('Typing is hidden: no letters or stars appear. Press Enter after each entry.')
        while True:
            password = getpass.getpass('Choose a new admin password (12..64 UTF-8 bytes): ')
            if args.check_only:break
            if not 12 <= len(password.encode()) <= 64:
                print('Use 12..64 UTF-8 bytes. No change made; try again.')
                continue
            if password == getpass.getpass('Confirm password: '):
                break
            print('Entries differ. No change made; try again.')
    before=usb({'op':'status'})
    if not args.check_only and not usb({'op':'password','password':password}).get('ok'):
        print('Password change rejected. No password details displayed.')
        return 1
    if args.temporary and not args.check_only:
        # Secret travels through stdin, never command arguments, files or logs.
        copied = subprocess.run(['powershell.exe','-NoProfile','-NonInteractive','-Command',
                                 'Set-Clipboard -Value ([Console]::In.ReadToEnd())'],
                                input=password, text=True, capture_output=True)
        if copied.returncode:
            print('Password changed, but clipboard transfer failed. Rerun --temporary locally.')
            return 1
        print('Temporary password copied to Windows clipboard. Paste it into the web login; no value is printed or saved.', flush=True)
    print('Testing the device now...',flush=True)
    state = usb({'op':'status'})
    ip = state['station_ip']
    username=state.get('username','admin')
    authorization = 'Basic ' + base64.b64encode((username+':'+password).encode()).decode()
    opener=urllib.request.build_opener(urllib.request.HTTPCookieProcessor(http.cookiejar.CookieJar()))
    session_mode=False;csrf=''
    try:
        with opener.open('http://'+ip+'/api/auth/session',timeout=15) as response:initial=json.load(response)
        request=urllib.request.Request('http://'+ip+'/api/auth/login',data=json.dumps({'username':username,'password':password}).encode(),headers={'Content-Type':'application/json','X-CSRF-Token':initial['csrf']})
        with opener.open(request,timeout=60) as response:json.load(response)
        with opener.open('http://'+ip+'/api/auth/session',timeout=15) as response:csrf=json.load(response)['csrf']
        session_mode=True
    except urllib.error.HTTPError as error:
        code=error.code;error.close()
        if code!=404:print('Session sign-in rejected. No credential details displayed.');return 1
    results = []
    def check(name, path, expected, body=None, mutation=True, auth=True):
        headers = {'Content-Type':'application/json'}
        if auth and not session_mode:
            headers['Authorization'] = authorization
        if mutation:
            headers['X-Config-Request'] = '1'
            if session_mode:headers['X-CSRF-Token']=csrf
        request = urllib.request.Request('http://'+ip+path, data=body, headers=headers)
        started = time.monotonic()
        try:
            with (opener.open(request,timeout=60) if auth else urllib.request.urlopen(request,timeout=60)) as response:
                code, raw = response.status, response.read()
        except urllib.error.HTTPError as response:
            code, raw = response.code, response.read()
        passed = code == expected and password.encode() not in raw
        results.append({'name':name,'expected':expected,'actual':code,'passed':passed,
                        'elapsed_s':round(time.monotonic()-started,2)})
        print(('PASS ' if passed else 'FAIL ')+name+' HTTP '+str(code), flush=True)
        return raw
    for path in ('/','/mqtt','/api/status','/api/config','/api/mqtt'):
        check('authenticated '+path, path, 200)
    check('unauthenticated status rejected','/api/status',401,auth=False)
    check('mutation header required','/api/config',403,b'{}',mutation=False)
    check('malformed config rejected','/api/config',400,b'{')
    check('invalid role rejected','/api/config',400,b'{"role":"INVALID"}')
    check('Wi-Fi secret HTTP rejected','/api/config',403,b'{"wifi_password":"fixture-only"}')
    check('MQTT bypass rejected','/api/config',409,b'{"mqtt_host":"example.com"}')
    check('oversized config rejected','/api/config',413,b' '*12289)
    check('malformed MQTT rejected','/api/mqtt/candidate',400,b'{')
    check('stale revision rejected','/api/mqtt/candidate',409,b'{"expected_revision":4294967294}')
    check('duplicate JSON fields rejected','/api/mqtt/candidate',400,b'{"expected_revision":0,"expected_revision":1}')
    final = usb({'op':'status'})
    report = {'station_ip':ip,'password_reset':not args.check_only,'tests':results,
              'authentication':'BROWSER_SESSION' if session_mode else 'LEGACY_BASIC',
              'mqtt_profile_preserved':all(before['mqtt'].get(k)==final['mqtt'].get(k) for k in ('revision','shared_revision','active_fingerprint')),
              'temporary_password':args.temporary,
              'free_heap':final.get('free_heap'),'minimum_free_heap':final.get('minimum_free_heap'),
              'largest_free_block':final.get('largest_free_block'),'uptime_ms':final.get('uptime_ms')}
    (ROOT/'docs/PHYSICAL_HTTP_EVIDENCE.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print('Web page: http://'+ip+'/  Username: '+username)
    print('Acceptance results saved without credentials.')
    return 0 if report['mqtt_profile_preserved'] and all(result['passed'] for result in results) else 1

if __name__ == '__main__':
    try:
        exit_code = main()
    except Exception:
        print('Local operation stopped; inspect device status. No credential details displayed.')
        exit_code = 1
    raise SystemExit(exit_code)
