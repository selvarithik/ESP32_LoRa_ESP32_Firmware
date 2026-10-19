> Historical snapshot: BLE references below describe earlier revisions only. BLE is excluded and removed from active features as of 2026-10-19; see BLE_SENSOR_REPORT.md.

# Part 1 implementation report and bench procedure

Date: 2026-10-15. Target: two ESP32 DevKit V1 / ESP32-WROOM-32 boards. Sensor currently disconnected.

**Software foundation IMPLEMENTED. Firmware build and automated Gateway/UI checks TESTED. Two-board acceptance and real DHT11/MQTT/TLS operation NOT TESTED. Part 1 hardware acceptance is not signed off.**

## Audit and architecture

See `PART1_AUDIT.md` for the original execution trace, defects, baseline build, and requirement-to-file checklist. The original firmware is backed up at `D:\esp32 selva\firmware-before-part1-20261015.zip`. No original source files were removed. See `CHANGE_MANIFEST.md` for firmware file changes and the Gateway file list below.

One firmware loads a persisted MASTER/NODE role. Role wrappers invoke one NetworkService dispatcher. Application messages use TransportManager rather than the radio driver. A separate FreeRTOS network task owns HTTP/MQTT clients and bounded queues; a separate DHT task samples every two seconds. The main task services role logic, local WebServer, Wi-Fi maintenance, and watchdog. Runtime interval changes execute in that main task. Connection/role edits save one validated NVS JSON snapshot, return a response, and restart atomically; old runtime settings remain active until restart. Submitting unchanged settings avoids writes and restart.

MASTER stores up to 12 registered Node IDs, logical addresses, observed LAN IPs, last-seen times, and latest telemetry. NODE sends HELLO every 30 seconds, heartbeat at its configured interval, and sensor status/readings at its telemetry interval. MASTER forwards accepted messages to its configured Gateway and polls every two seconds for commands. Node IDs are hardware-derived for new boards; established persisted IDs are preserved. Logical addresses must be unique within a Master. The Gateway supports multiple enrolled Masters and explicit Node assignments.

The Gateway reuses `nodes`, `sensor_parameters`, `telemetry_values`, `commands`, `command_events`, `logs`, existing widget transforms and Socket.IO event names. Additive migration extends `nodes` with role, assignment, revision, interval, status and a hashed Master token. `device_receipts` holds a bounded persistent deduplication window. No parallel telemetry or command database was created. Production migrations apply on the next server start; automated tests used disposable databases.

## Feature status

| Feature | Status and evidence |
| --- | --- |
| One binary, MASTER/NODE, hardware identity, validated NVS snapshots, migration, revisions | IMPLEMENTED; build TESTED; board persistence HARDWARE REQUIRED |
| Local page with device/network/configuration/secret fields, save/apply, command panel, restart and deliberate reset | IMPLEMENTED; desktop/mobile mocked browser checks TESTED; ESP32 serving HARDWARE REQUIRED |
| Nonblocking Wi-Fi join/retry, DHCP/static IPv4, recovery AP and asynchronous scan | IMPLEMENTED; compile TESTED; actual association/recovery HARDWARE REQUIRED |
| HTTP LAN packets, registration, addressed commands, ACK/result replay, peer health | IMPLEMENTED; compile TESTED; two-board traffic HARDWARE REQUIRED |
| DHT11 driver, sampling worker, errors and valid readings | IMPLEMENTED; disabled by default; no fabricated samples; actual measurements HARDWARE REQUIRED |
| Versioned Gateway API, assignments, authenticated exchange/polling, telemetry, commands, config requests, diagnostics/events/alarms | IMPLEMENTED; isolated Python integration tests TESTED |
| Gateway authentication and authorization | IMPLEMENTED; account sessions/CSRF for browser APIs, per-Master bearer tokens for device API, authenticated Socket.IO; tests TESTED |
| Optional MQTT | IMPLEMENTED in firmware and optional Gateway adapter; fake-broker callback tests TESTED; actual broker, ACLs and TLS NOT TESTED |
| TLS verification | IMPLEMENTED using configured CA certificates; no insecure verification bypass; real TLS handshakes NOT TESTED |
| Internet status | IMPLEMENTED: optional configured HEAD probe every 60 seconds, separate from Wi-Fi/Gateway/broker state; default UNKNOWN; external endpoint/TLS operation NOT TESTED |
| Ethernet/BLE | PARTIALLY IMPLEMENTED: configuration fields and truthful NOT_AVAILABLE capability boundaries; no PHY driver or authenticated BLE service |
| E220 | Stored future settings IMPLEMENTED; startup/traffic disabled; driver integration and radio validation HARDWARE REQUIRED |
| OTA | NOT IMPLEMENTED in Part 1. Existing Gateway OTA features predate this work and are not invoked by this firmware |
| Multi-hop routing, durable exactly-once commands, persistent MQTT sessions | NOT IMPLEMENTED; reserved for later phases |

## Protocol, security and delivery contract

JSON frame: maximum 4096 UTF-8 bytes. Payload: maximum 2048 UTF-8 bytes, encoded as a JSON object string inside the outer frame. This deliberate string encoding makes payload length and CRC independent of JSON whitespace/order differences between C++ and Python.

Required fields: `protocol_version=1`, `message_id`, `source`, `destination`, `origin_id`, `final_destination_id`, `sequence_number`, `timestamp`, `ttl`, `type`, `payload`, `payload_length`, `payload_crc32`, `correlation_id`. `next_hop` is retained for future routing. Source/destination/origin/message IDs use 1–48 safe alphanumeric/`_.:-` characters. TTL is 1–15. Timestamp is Unix seconds when the sender has a valid clock, otherwise zero; uptime/sample timestamps are explicit payload fields. Gateway reception time is authoritative for last-seen and stored measurement arrival time. CRC32 detects payload corruption; it is not cryptographic authentication. Message IDs combine boot entropy and a counter; retries retain the entire original packet ID/content.

Part 1 accepts HELLO, HEARTBEAT, STATUS, TELEMETRY, COMMAND, ACK, RESPONSE, CONFIG_GET, CONFIG_UPDATE, EVENT, ALARM and DIAGNOSTIC. OTA/routing/chat traffic is not accepted by this adapter.

LAN POST `/api/v1/packets` requires Basic authentication (`mesh`, configured mesh_key), bounded valid frames, a permitted source and the configured/observed endpoint. HTTP 202 means admission to the authenticated receive queue. Data is retried until the Master's application ACK. That ACK means the Master accepted the data locally and, when configured, admitted its Gateway forwarding job. It does not promise durable Gateway storage. Command ACK means received, and only RESPONSE means execution completed/failed. Gateway HTTP acknowledgement follows validation/storage. MQTT publish success alone never clears delivery tracking; application ACK does.

Outgoing/incoming network queues each hold six frames; network results hold eight; application pending jobs hold twelve; recent firmware IDs/results hold 32. Full queues reject new work explicitly. Retry count 0–5, command timeout 1–30 seconds, increasing retry spacing capped at four times timeout, and maximum message age 1–300 seconds (not less than timeout) bound retries. Pending jobs and cached results are RAM-only; telemetry can be lost after bounded retry exhaustion or power loss. Master peer discovery is rebuilt by HELLO after restart. Gateway retains the latest 4096 source/message receipts across restart. Command state transitions/results use the existing idempotent command manager. There is no claim of durable exactly-once effects across ESP32 reset. Initial commands are reads or absolute, idempotent interval/configuration assignments.

HTTP and Basic/bearer credentials require a trusted local LAN; plain HTTP provides no encryption. Remote access uses outbound HTTPS/MQTT TLS with CA verification, never public forwarding of the ESP32 configuration page. Setup/admin passwords are random per board. During initial commissioning (SSID and mesh_key both empty), physically attached serial can display provisioning credentials on reset; those credentials are never included in diagnostic history or config API output. Save them before commissioning. Recovery AP stops after 60 seconds of stable station connectivity and reappears after disconnection/retry. Its SSID is `Setup-<device_id>` and default IP is `192.168.4.1`.

Browser API mutations require an administrator/creator session and CSRF. Existing general Gateway APIs and Socket.IO were previously open; they now require an Account Settings session. Existing simulator/integration clients must authenticate or use the new device API. New Gateway account databases require an explicit initial administrator password; existing accounts/legacy hashes are preserved. Existing administrators should change any old development password before LAN deployment.

## Build, dependencies and actual validation

PowerShell, from the firmware directory:

```powershell
& 'C:\Users\Admin\.platformio\penv\Scripts\pio.exe' run
```

PlatformIO Core 6.2.0; espressif32 7.0.1; Arduino ESP32 2.0.17 / framework package 3.20017.241212; Xtensa compiler 8.4.0; ArduinoJson 7.4.3; PubSubClient 2.8.0; Adafruit DHT sensor library 1.4.6; Adafruit Unified Sensor 1.1.15. Existing `esp32dev` 4 MB board/partition settings are preserved; no OTA/partition workflow was introduced. Main loop stack is explicitly 16 KB, network worker 14 KB, sensor worker 3 KB. Reported static RAM does not include queues, task stacks, TLS buffers or dynamic JSON allocations; soak tests must inspect free/minimum heap.

Final build result and image checksum are appended below after verification. Firmware includes no active E220 initialization. The same image is intended for both boards. Build success does not prove on-board behavior.

Gateway test command:

```powershell
Set-Location 'D:\esp32 selva\ESP32_LoRa_Project'
& '.\.venv\Scripts\python.exe' -m unittest discover -s tests -p 'test_*.py'
```

Actual result: **34 tests PASS**, including 14 existing Gateway regressions, five existing account/sensor cases, twelve device-bridge cases and three MQTT callback cases. The baseline combined-run fixture collision was repaired with explicit disposable-database isolation. No production telemetry was seeded or replaced. Python 3.14.3, Flask 3.1.3, Flask-SocketIO 5.6.1 and Werkzeug 3.1.8 were used; optional paho-mqtt 2.1.0 was installed/tested.

```powershell
Set-Location 'D:\esp32 selva\ESP32_LoRa_ESP32_Firmware'
node test/local_page.cjs
```

Actual result: PASS at 1280 px and 375 px. The test checks fields/types, absence of stored secrets, multiline CA fields, validation errors, unchanged saves, commands, reset confirmation, JavaScript errors and horizontal overflow. Screenshots `local-page-mock-1280.png` and `local-page-mock-375.png` use simulated API data, not a connected ESP32. Gateway firmware-page JavaScript syntax and Git whitespace checks passed. No serial flash/monitor, DHT11 read, network soak, real broker or TLS handshake was performed.

## Start and enroll the Gateway

```powershell
Set-Location 'D:\esp32 selva\ESP32_LoRa_Project'
& '.\.venv\Scripts\python.exe' -m pip install -r requirements-part1.txt
# Needed only for a new account database without legacy credentials:
$env:GATEWAY_ADMIN_USER = 'admin'
$env:GATEWAY_ADMIN_PASSWORD = Read-Host 'Choose an initial administrator password (12+ characters)'
$env:GATEWAY_BIND = '0.0.0.0'
& '.\.venv\Scripts\python.exe' backend/server.py
```

Use the PC's real LAN IP on port 5000 from ESP32 devices, never localhost/127.0.0.1. Permit port 5000 only on the intended private LAN. The server defaults to loopback unless GATEWAY_BIND is set. Sign in at `/accounts`; existing accounts keep their passwords. The original dashboard layout/widgets and command center remain in use.

After reading each board's actual hardware ID, enroll the Master first and the Node second in a separate terminal:

```powershell
Set-Location 'D:\esp32 selva\ESP32_LoRa_Project'
& '.\.venv\Scripts\python.exe' backend/provision_device.py --id '<MASTER-ID>' --name 'ESP32-01' --role MASTER
& '.\.venv\Scripts\python.exe' backend/provision_device.py --id '<NODE-ID>' --name 'ESP32-02' --role NODE --master-id '<MASTER-ID>' --node-address '1'
```

Replace angle-bracket placeholders with the real values. Store the Master's one-time `gateway_token` output securely and put it only in the Master's local Gateway token field. Enrollment creates the existing dashboard parameter definitions for temperature and humidity but no sample readings. The authenticated `/api/v1/devices` POST API is an alternative to the local operator CLI. Re-enrollment of an existing ID is rejected. `/api/v1/devices/<id>/rotate-token` explicitly rotates a Master token; update that board afterwards.

## Configure and connect both boards

1. List ports with `pio device list`; identify each physical board. Upload the same built firmware to each desired COM port using `pio run --target upload --upload-port COMx`. Flashing has not been performed by this task.
2. Open `pio device monitor --port COMx --baud 115200`; press EN while still uncommissioned to read the device ID and its random setup/admin credentials. Preserve those secrets locally. Do not publish console captures containing them.
3. Join that board's `Setup-<device_id>` AP and open `http://192.168.4.1`. Browser user is `admin`; password is its printed per-device admin password.
4. Use the table below. Both boards must join the same 2.4 GHz LAN with client isolation disabled. Use DHCP initially, and reserve the Master's address in the router so the Node endpoint stays stable.
5. Save and apply. The response confirms the save before the board restarts. Reconnect your computer to the LAN and open the board's reported LAN IP. If a network change fails, reconnect through its recovery AP. Configuration persists in NVS.

| Setting | ESP32-01 MASTER | ESP32-02 NODE |
| --- | --- | --- |
| device_name | ESP32-01 | ESP32-02 |
| role | MASTER | NODE |
| master_id | Automatically its hardware ID on save | Actual MASTER hardware ID |
| node_address | May remain default | Unique logical address `1` |
| allowed_peers_csv | Actual NODE hardware ID | Not used for Master authorization |
| registration_open | false with explicit allowed peer ID | false |
| mesh_key | Same generated 32–64 character secret | Same secret as Master |
| master_host | Not needed | Master's actual reserved LAN IP/hostname |
| wifi_enabled / SSID / password | Actual LAN credentials | Same LAN credentials |
| wifi_dhcp | true initially | true initially |
| server_host / port | PC LAN IP, port 5000 | Not needed for initial relay design |
| gateway_token | One-time token from Master enrollment | Leave blank |
| heartbeat_seconds | 30 | 30 |
| telemetry_seconds | 10 | 10 |
| command_timeout_ms / retry_count | 5000 / 3 | 5000 / 3 |
| dht_enabled / dht_pin | false / 4 | false / 4 until connected |
| mqtt_enabled | false initially | false |
| ethernet_enabled / ble_enabled / lora_enabled | false | false |

Generate a mesh secret locally, for example with Python `secrets.token_hex(16)`. No shared production secret is embedded in source. Secret fields left blank in the page preserve their stored value; the deliberate clear-password checkbox supports an open Wi-Fi network. Use caution when changing admin/setup passwords: save the new values and update browser authentication after restart.

## Master–Node, DHT11 and Gateway command bench tests

- With the Gateway stopped or unconfigured, confirm the Master's local status lists the registered Node and changing last-seen/health. Send PING from the Master's command panel to the exact Node hardware ID. Refresh status to see ACKNOWLEDGED and COMPLETED/result. A queued/SENT state alone is not execution success.
- Repeat GET_STATUS and GET_TELEMETRY. With no sensor connected, expect NOT_CONFIGURED and no temperature/humidity values.
- Power off before wiring DHT11. Connect labeled VCC to 3.3 V, GND to GND, DATA to GPIO 4. For a bare sensor add a 4.7–10 kΩ pull-up from DATA to 3.3 V; a compatible module may already include one. Check the actual part/module labels and supply arrangement rather than assuming pin order. The selected power/pull-up arrangement follows [Adafruit's DHT wiring guide](https://learn.adafruit.com/dht/connecting-to-a-dhtxx-sensor).
- Enable DHT only on the Node and save/apply. After the first two-second sample, verify valid temperature and humidity in the Node's status, the Master's latest Node telemetry, and the existing Gateway Sensors/Live pages. Unplug the sensor and verify READ_ERROR, an ALARM record and omitted measurements; no zero/fabricated replacement readings should be inserted.
- In the existing Gateway Command Center select the enrolled Node and issue PING or STATUS. The Master polls within approximately two seconds, forwards the addressed command, and relays Node ACK and RESPONSE. Observe existing command history reach COMPLETED with real data. GET_STATUS, GET_TELEMETRY, GET_CONFIG, SET_TELEMETRY_INTERVAL and SET_HEARTBEAT_INTERVAL are also supported by the Part 1 adapter.
- For an interval command, use payload `{"seconds":15}`. Confirm the Node's config revision/interval changes and persists after EN/reset. Reissuing an identical absolute interval avoids an unnecessary NVS write. The versioned config API accepts only telemetry_seconds, heartbeat_seconds and enabled; connection/role/credential changes require the local page.
- Legacy dashboard options such as CUSTOM, RESTART and WIFI_SCAN remain in the existing Gateway UI but are explicitly FAILED as unsupported when delivered through this Part 1 device adapter. They are not silently executed.
- Disconnect router/Gateway, restore them, and inspect reconnection, bounded retries, TIMED_OUT states and counters. Power off the Node and verify ONLINE → STALE → OFFLINE based on received timestamps. Run a multi-hour soak while watching free/minimum heap, watchdog/reset reason and queue overflow counters. These tests remain pending.

For authenticated CLI requests, POST JSON credentials to `/api/auth/login` with a PowerShell WebRequestSession, retain its cookie, and send the returned `csrf` in `X-CSRF-Token` on mutations. `/api/v1/commands` POST body is `{"node_id":"<NODE-ID>","command_type":"SET_TELEMETRY_INTERVAL","payload":{"seconds":15}}`. A supplied command_id makes identical retries idempotent; conflicting reuse returns 409. `/api/v1/devices/<id>/config` GET explicitly returns last-received status; submit GET_CONFIG for a fresh response stored in command history.

## Optional MQTT test

HTTP works without paho-mqtt installed or a broker configured. To test MQTT, install the optional dependency:

```powershell
Set-Location 'D:\esp32 selva\ESP32_LoRa_Project'
& '.\.venv\Scripts\python.exe' -m pip install -r requirements-mqtt.txt
$env:GATEWAY_MQTT_HOST = '<BROKER-LAN-HOST>'
$env:GATEWAY_MQTT_PORT = '1883'
$env:GATEWAY_MQTT_USER = '<GATEWAY-SERVICE-USER>'
$env:GATEWAY_MQTT_PASSWORD = Read-Host 'Gateway MQTT password'
$env:GATEWAY_MQTT_TRUST_ACL = '1'
# For TLS use the TLS broker port plus a real local CA file:
# $env:GATEWAY_MQTT_CA = 'C:\certs\broker-ca.pem'
& '.\.venv\Scripts\python.exe' backend/server.py
```

Set TRUST_ACL only after configuring broker ACLs: each Master may publish only `gateway/v1/devices/<its-id>/status|telemetry|events|responses`, subscribe only to its own `/commands`, and must not publish `/commands`. The Gateway service can subscribe to device uplinks and publish commands/application ACKs. No anonymous broker access, wildcard device write permission or retained command publication. Tokens/Wi-Fi credentials are never published in packets.

Configure the Master's broker host/port/user/password, optional mqtt_tls and PEM CA, then enable MQTT. Firmware publishes QoS 0 with application acknowledgement/retry; it subscribes to commands at QoS 1. Gateway publishes commands/ACKs at QoS 1 without retain and rejects retained uplinks. Sessions are clean; persistent Gateway commands and application IDs provide replay/retry behavior. HTTP polling continues every two seconds even when MQTT is connected. Selected uplinks use MQTT when connected, otherwise HTTP; IDs are preserved and Gateway receipts prevent double insertion. Disable/stop the broker and confirm HTTP continues. Real broker/TLS testing has not been performed.

## Known limitations and next-phase checklist

- [ ] Perform every two-board/DHT11 bench step above, real broker/TLS checks, queue saturation and a multi-hour Wi-Fi/Gateway outage/soak test before marking Part 1 acceptance complete.
- [ ] If Internet health is required, configure internet_probe_host/port/tls/ca to an authorized external service and verify it. No default third-party probe is configured. HEAD on its root runs every 60 seconds in the network worker. REACHABLE, REMOTE_SERVICE_UNAVAILABLE, PROBE_UNREACHABLE and UNKNOWN describe that probe, not proof that every Internet service works.
- [ ] Confirm actual Ethernet hardware/PHY and its pin map before adding a driver. BLE currently has settings only, not provisioning.
- [ ] Verify the precise E220 model, regional frequency/power rules, supply requirements and UART/control pins before enabling a radio driver. The old radio code is behind ENABLE_E220_DRIVER as an unvalidated future reference; that flag is not supported/enabled in Part 1.
- [ ] Design durable command deduplication before adding non-idempotent actuators. Bounded RAM caches reset on reboot and can evict older IDs; UTC replay age checks only apply when the clock is valid.
- [ ] Harden local HTTP provisioning and denial-of-service behavior before hostile-network deployment. Arduino WebServer buffers request bodies before handler-level size checks. Local mesh credentials are shared within the configured group; Gateway tokens are scoped per Master. Broker identity isolation depends on correctly enforced ACLs.
- [ ] Peer/queue capacities are explicit initial limits, not an unlimited network claim. Disabled Masters require local re-enabling because normal Gateway polling stops; a disabled Node can still accept an authenticated management command from its Master.
- [ ] Existing Gateway command manager operations commit their own lifecycle events; receipt insertion is subsequently idempotent, but arbitrary future external side effects need a stronger transactional outbox.
- [ ] Current firmware nearly fills the existing 1.25 MiB application slot. Revisit flash/partition strategy only after confirming physical flash capacity and later-phase requirements. OTA remains exclusively Part 4.

## Gateway files

Created: `backend/device_api.py`, `backend/mqtt_adapter.py`, `backend/provision_device.py`, `requirements-part1.txt`, `requirements-mqtt.txt`, `tests/test_device_api.py`, `tests/test_mqtt_adapter.py`, `docs/PART1_INTEGRATION.md`.

Modified: `backend/server.py`, `backend/account_settings.py`, `backend/node_manager.py`, `dashboard/static/js/firmware.js`, `tests/gateway_fixture.py`, `tests/test_accounts.py`, `tests/test_gateway.py`. Changes add the bridge/security, redact token hashes, honor enrolled heartbeat intervals, retain CSRF on existing firmware requests, and isolate/authenticate existing tests. No Gateway files were removed. Existing telemetry, widgets, dashboard layout, firmware and chat modules were retained.

## Final verified build artifact

Final PlatformIO build: **SUCCESS**, 47.74 seconds. Static RAM: **56,736 / 327,680 bytes (17.3%)**. Application flash: **1,270,425 / 1,310,720 bytes (96.9%)**. No compiler warnings were emitted in this final build. Approximately 40 KB remains in the current application slot.

Artifact: `.pio/build/esp32dev/firmware.bin`

SHA-256: `e488ccde6a9693600fc724786bbde6095ed6eb9acdf53aec8d3c10f58c926568`

## COM8 first-boot follow-up

User-provided serial output confirms startup as NODE, watchdog initialization and setup AP creation on ESP32-10777D842178. It does not yet verify page access, station association or messaging. Absent legacy NVS string keys caused misleading Preferences error logs; all optional string reads now check key existence before using a default. Existing settings are retained.

Rebuild: SUCCESS, 39.39 seconds; static RAM 56,736 bytes; flash 1,270,985 / 1,310,720 bytes (97.0%). This binary has not been uploaded by the agent.

Current firmware SHA-256: `ff5a657b00240bd098032b55ab959968eff5fd471d54ddeedbc46b478a2c5b7a`

## Temporary Wi-Fi defaults

At the user's request, local-only `include/local_wifi_credentials.h` now supplies the temporary SSID/password when Wi-Fi is enabled and no SSID is saved. This file is Git ignored; the committed example contains placeholders. Saved nonempty Wi-Fi settings take precedence. Remove the local header to stop including the defaults in future builds; any settings already saved to NVS must be changed through the local configuration page. The ESP32-WROOM-32 requires a 2.4 GHz network; an SSID name ending in _5G does not establish which bands it actually provides. No connection has been verified for this network.

Temporary-defaults build: SUCCESS, 143.02 seconds; RAM 56,736 bytes; flash 1,271,113 / 1,310,720 bytes (97.0%). Both configured Wi-Fi values were verified in the image without printing them. Not uploaded or connection-tested. Current firmware SHA-256: `be1bea87e8a2d08493903ac22a73092d6edfee66c4301d008c1a8fd4fcb8b8c4`

## Shared ten-character bench credentials

The user explicitly requested a shared ten-character password for setup AP and local admin login, plus the 2.4 GHz SSID SRI SKANDA PG 301. The Git-ignored local header now enables LOCAL_TEST_CREDENTIALS, overriding saved Wi-Fi/admin/setup credentials on every boot of this local test build. Password validation accepts 10 characters; ordinary builds still generate unique random credentials. The Gateway token, mesh key and MQTT authentication are unchanged. Disable the local testing flag before expecting webpage credential/network changes to survive a reboot; remove the private header to exclude these bench defaults from future builds. No actual Wi-Fi association is claimed by this configuration change.

Shared-credentials build: SUCCESS, 32.23 seconds; RAM 56,736 bytes; flash 1,271,109 / 1,310,720 bytes (97.0%). Image checked for the new SSID and matching ten-character credentials; old _5G SSID absent. Not uploaded. SHA-256: `cf1a584eda1b765cec07f9dedf3f280abd65206210a698493983cdda9acf80ad`


2026-10-18 supersession: temporary compiled Wi-Fi headers have been removed. Use the non-echo physical USB commissioning mechanism in COMMISSIONING.md; the historical header-based procedure above no longer applies. Existing NVS values are not erased.
