# Part 1 change manifest

Firmware changes compared with the pre-change ZIP backup. No original source files removed.

| Status | File |
| --- | --- |
| MODIFIED | `src/command/command_manager.cpp` |
| MODIFIED | `src/communication/lora_transport.cpp` |
| MODIFIED | `src/communication/transport_manager.cpp` |
| MODIFIED | `src/communication/transport_manager.h` |
| MODIFIED | `src/communication/wifi_manager.cpp` |
| CREATED | `src/config/config_fields.h` |
| MODIFIED | `src/config/config_schema.h` |
| MODIFIED | `src/config/default_config.h` |
| MODIFIED | `src/core/config_manager.cpp` |
| MODIFIED | `src/core/config_manager.h` |
| CREATED | `src/core/network_service.cpp` |
| CREATED | `src/core/network_service.h` |
| MODIFIED | `src/core/storage_manager.cpp` |
| MODIFIED | `src/devices/device_manager.cpp` |
| MODIFIED | `src/diagnostics/diagnostics_manager.cpp` |
| MODIFIED | `src/identity/device_identity.cpp` |
| MODIFIED | `src/main.cpp` |
| MODIFIED | `src/protocol/packet.cpp` |
| MODIFIED | `src/protocol/protocol.cpp` |
| MODIFIED | `src/protocol/protocol.h` |
| MODIFIED | `src/roles/master.cpp` |
| MODIFIED | `src/roles/node.cpp` |
| CREATED | `src/sensors/sensor_manager.cpp` |
| CREATED | `src/sensors/sensor_manager.h` |
| MODIFIED | `src/web/local_web_server.cpp` |
| MODIFIED | `src/web/local_web_server.h` |
| MODIFIED | `platformio.ini` |

Additional artifacts: `docs/PART1_AUDIT.md`, `docs/PART1_REPORT.md`, this manifest, `docs/local-page-mock-1280.png`, `docs/local-page-mock-375.png`, and `test/local_page.cjs`.

## 2026-10-18 cleanup

The four generated mock-page screenshots and three placeholder README files were removed at the user's request. Historical screenshot references above describe earlier test evidence; the files are no longer retained. Browser screenshot capture now requires CAPTURE_SCREENSHOTS=1.
