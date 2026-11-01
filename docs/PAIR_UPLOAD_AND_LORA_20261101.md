# Pair deployment and LoRa check — 1 November 2026

User assignment: COM11 Master, COM8 Slave. Both are ESP32 with E220-900T22D, DHT11, INA219 and MQ135 modules. No RF success is claimed.

## Implemented and software-tested

Shared image 1.1.3-pair-sensors. Removed the obsolete Master-only INA219 restriction and corresponding misleading Slave disabled reason. Both roles retain the same firmware; role is stored in configuration. Kept sensor enable, GPIO, I2C address, shunt resistance and MQ135 warm-up settings because they are necessary. No schema fields, LoRa packet formats or MQTT topics were removed/changed. Tests now require INA219 availability on both roles; pin-conflict validation still applies.

PlatformIO esp32dev build passed: program 1,363,229 bytes, static RAM 76,036 bytes; image file 1,369,936 bytes. Native E220 settings/fragmentation/reassembly/duplicates/routing, shared controls, operation handlers, contract and gzip checks passed. Four new flash-layout guard tests passed. These are software results, not RF verification.

## Master COM11 — uploaded and verified

ESP32-10777D842178. Full 4 MB backup verified, encrypted with Windows DPAPI and kept under ignored local data. Only application at 0x10000 written. Complete readback verified image hash and every byte outside the application unchanged before boot; partition table, NVS, OTA metadata and filesystem preserved. Existing huge_app layout retained. After boot: MASTER at 192.168.1.106, EMQX connected with verified TLS. Broker fingerprint/client ID/revisions and sensor enable/GPIO flags match the pre-upload snapshot. DHT11 and INA219 still report read errors; MQ135 produces raw warm-up readings. No fabricated sensor values.

## Slave COM8 — upload pending, not written

ESP32-842F2AE81F84, ESP32-D0WD-V3 rev3.1, 4 MB flash. Existing factory ESP-AT layout has NVS at 0x12000, customization data at 0x20000 and 1.5 MB OTA applications at 0x100000/0x280000. OTA selection data was erased. The new image fits the first existing application slot, so partition migration is not needed. Espressif documents selecting the first OTA slot when OTA data is erased and no factory application exists: https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32/api-reference/system/ota.html

The uploader was extended to identify the board by its confirmed MAC, select only proven existing app slots, reject active/ambiguous OTA selection, overlapping partitions or unsupported NVS versions, and preserve every non-application byte. No partition or bootloader flashing is included.

Full Slave flash backup timed out after 240 seconds at 460800 baud. A smaller 64 KB read also timed out after 35 seconds at 230400 baud. Chip identity, partition-table and OTA-data reads succeeded earlier. Root cause is not established; do not claim a successful backup or upload. No Slave flash write/erase was attempted, no role or credential was changed. A manual BOOT/EN retry with reliable USB/power is required; if bulk reads still stall, isolate external-module power and inspect cable/board stability before writing.

## LoRa — physical prerequisite failed

Read-only Master E220 inspection returned SETTINGS_READ_TIMEOUT; AUX not ready, no public register readback, zero register writes and zero transmitted UART fragments. Radio stayed disabled; RF transmission, discovery, Master–Slave PING/ACK/RESPONSE and forwarded sensor data have not been verified.

Next physical checks: correct E220 TXD->GPIO16, RXD->GPIO17, M0->25, M1->26, AUX->27, stable 5 V supply and common ground on both boards. Fit antennas before any future RF test; verify each MQ135 divider protects GPIO34. Only after hardware readback, matching permitted radio settings and successful Slave deployment can physical pair traffic be tested.

No Gateway application, Wi-Fi credential or MQTT profile was changed. User's earlier UI/manual work remains paused. The matching JSON and per-application upload evidence contain exact results; private dumps stay out of Git.
