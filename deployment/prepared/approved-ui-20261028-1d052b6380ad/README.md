# Approved ESP32 UI: deployment handover

The approved NASA-style header and exact FF0000 navigation text are compiled into
the shared firmware. Both roles use the same application; the saved role selects
MASTER or NODE behavior. Do not use older cached per-role images.

This is a build handover, not a completed physical deployment. No device was
flashed or reconfigured. Wi-Fi, active MQTT profiles and sensor NVS were not written.

## Package

The versioned directory under deployment/prepared/approved-ui-20261028-* contains
firmware.bin, bootloader.bin, partitions.bin, the selected huge_app.csv,
manifest.json and this handover. Manifest checksums identify the exact build.
The package intentionally contains no automatic flashing script, filesystem image,
provisioning credentials, device backups or OTA-metadata reset image.

## Why the usual upload command is not the next safe step

This build needs the selected huge_app layout. It cannot fit the previous app0
partition. An application-only upload against the old layout can overwrite the
start of app1. A normal PlatformIO upload also writes partition/boot metadata.
The owner previously requested manual physical deployment of changed layouts.

| Region | Previous layout | Prepared layout |
| --- | --- | --- |
| NVS | 0x9000, size 0x5000 | unchanged |
| OTA metadata | 0xE000, size 0x2000 | unchanged region; boot selection must be inspected |
| app0 | 0x10000, size 0x140000 | 0x10000, size 0x300000 |
| app1 | 0x150000, size 0x140000 | removed |
| SPIFFS | 0x290000, size 0x160000 | 0x310000, size 0xE0000 |
| Core dump | 0x3F0000, size 0x10000 | unchanged |

## Physical migration sequence — not executed

1. Identify the intended Master and its actual COM port, hardware identity, flash
   capacity and active application selection. COM port names can change.
2. Before any writes, make and verify a private full-flash backup, including NVS,
   both old application slots, OTA metadata and filesystem. Firmware configuration
   backups contain secrets; keep them protected and outside Git/reports.
3. Inspect whether SPIFFS contains user files. Export needed files and confirm
   they fit the smaller new filesystem before repacking. Do not copy the old
   larger filesystem image raw into the smaller region.
4. Plan the partition and application writes together. Preserve NVS and account,
   Wi-Fi, MQTT, device identity and sensor settings. Inspect app1 boot selection;
   do not blindly reset OTA metadata or use a factory reset.
5. Deploy only after the owner accepts the app1 removal and filesystem migration.
   Physical deployment remains with the owner unless explicitly authorized later.
6. Verify hashes, boot without a reset loop, configured MASTER role, unchanged
   settings, actual DHCP IP, local authentication and approved desktop/mobile UI.
7. Then test Master-only sensor telemetry and safe PING ACK/RESPONSE. Test transport
   switching, sleep and LoRa separately; their hardware acceptance is still pending.

Retain the complete original flash backup for recovery. Never restore only an old
partition table over an incompatible new application/filesystem arrangement.
No general erase or default-credential commissioning is required by this UI update.
