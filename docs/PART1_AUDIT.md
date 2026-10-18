# Part 1 audit and implementation checklist

Audit date: 2026-10-15. Firmware: `D:\esp32 selva\ESP32_LoRa_ESP32_Firmware` (no Git metadata). Gateway: `D:\esp32 selva\ESP32_LoRa_Project` (Git checkout; clean at audit).
Original firmware backup: `D:\esp32 selva\firmware-before-part1-20261015.zip`.

## Existing execution path and critical gaps

`main.cpp` loads Preferences, identity and watchdog, starts Wi-Fi and unconditionally starts E220 UART/pins, then initializes transport, mesh, command and empty role managers. Heartbeats go directly to LoRa. TransportManager and CommandManager both access that transport; the latter drains incoming packets and only handles COMMAND. Master and Node loop bodies are empty. No sensor driver exists. DeviceManager advertises LoRa and OTA foundation despite no verified hardware.

The local WebServer has status/config/network/command/restart/reset routes. The page only displays JSON. Mutations have no authentication. Config updates mutate live fields before validation/save; a rejected role can leave prior fields modified. Wi-Fi connection and scans block; a 12-second connect can exceed the 10-second watchdog. The setup AP always runs with a shared password. Gateway host is a developer-specific address. NVS writes span many keys and there is no schema/revision or safe commit. Logger already provides a bounded diagnostic ring worth retaining. Existing identity persists a 24-bit MAC suffix; preserve established IDs and use all 48 bits for new devices.

The Flask Gateway has node/parameter/config CRUD, telemetry ingestion, SQLite migrations, command lifecycle/event tables and a functioning dashboard with Socket.IO updates. Dashboard `command_center.js` uses `/api/commands`; `sensor-management.js` manages parameter definitions; dashboard/sensor widgets consume actual telemetry APIs. Reuse these components. General web authentication is explicitly disabled in `backend/server.py`; account-specific routes have separate session/CSRF protection. Existing telemetry/command feedback ingress is public. No MQTT client was found. Firmware packet JSON differs from Gateway packet-manager binary/CRC framing. Existing firmware/OTA routes already exist in the Gateway; Part 1 must not expand or use them.

## Baseline validation

- PlatformIO Core 6.2.0; espressif32 7.0.1; Arduino ESP32 2.0.17 (framework package 3.20017.241212); Xtensa 8.4.0; ArduinoJson 7.4.3.
- `C:\Users\Admin\.platformio\penv\Scripts\pio.exe run`: PASS, 141.99 s, RAM 48,392 / 327,680 bytes; flash 1,001,213 / 1,310,720 bytes. No hardware flashed.
- Combined Gateway unittest discovery: 15 tests ran, one setup error because two suites call a non-idempotent shared fixture (`UNIQUE constraint failed: nodes.node_id`). Windows temp cleanup also encounters an open SQLite handle. Run suites in separate processes until fixture isolation is repaired. This is a baseline issue, not proof of device integration.

## Requirement-to-file implementation map

| Requirement | Existing/planned implementation |
| --- | --- |
| Unified roles and lifecycle | `src/main.cpp`, `src/roles/master.*`, `src/roles/node.*` |
| Identity, logical IDs, role assignment | `src/identity/device_identity.*`, `src/config/config_schema.h` |
| Schema, migration, atomic NVS, revisions | `src/core/config_manager.*`, `src/config/config_fields.h` |
| Packet types, bounded framing, integrity, validation | `src/protocol/protocol.*`, `packet.*`, `ack.*`, `response.*` |
| Wi-Fi STA/recovery/AP/static IPv4 | `src/communication/wifi_manager.*` |
| Transport interface, bounded network worker and queues | `src/communication/transport_manager.*` |
| Single dispatcher, retries, registration, peer health | `src/core/network_service.*` (planned); role wrappers call common service |
| Safe commands, ACK/result deduplication | `src/command/command_manager.*`, common network service |
| Local offline webpage, authentication, all settings/actions | `src/web/local_web_server.*` |
| Sensor interface and DHT11, disabled by default | `src/sensors/sensor_manager.*` (planned) |
| Heartbeat/status/logs | `src/diagnostics/diagnostics_manager.*`, existing logger |
| Versioned device API, authenticated polling, idempotent ingress | Gateway `backend/device_api.py` (planned), registered in `server.py` |
| Device assignments/status, telemetry, commands, diagnostics | Reuse Gateway `database.py`, `node_manager.py`, `sensor_manager.py`, `telemetry_manager.py`, `command_manager.py`; additive migration only |
| Existing dashboard integration | Existing Socket.IO emit helpers and API models; preserve layout |
| Optional MQTT, TLS and application delivery tracking | Transport worker plus Gateway MQTT adapter (planned); HTTP remains required/available |
| Ethernet/BLE settings and honest availability | Config schema, local page, diagnostics; hardware driver deferred |
| Future E220 settings; no radio startup | Config schema and interface boundaries; existing radio code disconnected |
| Mesh extension | Existing `src/mesh/` retained; no fabricated multi-hop routing |
| OTA excluded | No new OTA handlers, downloads or update workflow |
| Build and security/protocol integration tests | `platformio.ini`, Gateway tests, hardware procedure in Part 1 report |

## Controlled sequence

1. Audit and baseline build (complete).
2. Config identity/versioning/validation/persistence and protocol.
3. Nonblocking Wi-Fi and bounded HTTP transport; one receive dispatcher.
4. Authenticated local configuration workflow and honest diagnostics.
5. Registration, safe commands, retries/deduplication, DHT11 sampling.
6. Reuse Gateway database/dashboard and expose authenticated versioned device exchange/polling.
7. Optional MQTT/TLS and unavailable-interface settings.
8. Regression/security tests, final build, exact two-board hardware instructions.

## Hardware decisions authorized by the user

Two ESP32 DevKit V1 / ESP32-WROOM-32 boards. DHT11 is currently disconnected; user delegated pin choice. Proposed configurable default DATA is GPIO 4. Leave DHT disabled until connected. Use 3.3 V, common GND, and a 4.7–10 kΩ DATA-to-3.3 V pull-up for a bare sensor; confirm module pull-up/supply arrangement before connecting a module. No Ethernet PHY or E220 is available. No partition changes or flashing are authorized/needed for the software baseline. Hardware behavior remains untested.
