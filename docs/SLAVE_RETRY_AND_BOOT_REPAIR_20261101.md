# Slave COM8 retry, recovery and proposed boot repair — 1 November 2026

## Actual result

Slave ESP32-842F2AE81F84 on COM8: full 4 MB flash backup completed using 128 KB block reads at 115200 baud; whole-device checksum matched, Windows DPAPI encryption round-trip verified. Encrypted backup is private under data/system-audit/slave-COM8-20261101-chunked/backup.dpapi. Earlier uninterrupted streams stalled; no claim is made that those attempts succeeded.

The shared 1.1.3-pair-sensors application was written at 0x100000 and its whole-flash expected-image device checksum matched, including every byte outside its application sectors. Application hash: 78859f95252149792872cc2c9ca7c457c70a62b2ae1eb7fb26b32b08b83202ba. No bootloader, partition table, NVS or second application slot was written.

Physical boot then failed before Arduino setup: factory bootloader is ESP-IDF 3.0.3; table has no MD5 record, whereas current SDK rejects it. Observed errors: 'No MD5 found in partition table', 'load_partitions returned 0x105', and esp_ota_get_running_partition assertion. The previous partition preflight missed this compatibility requirement; the uploader now rejects such application deployment and has MD5/legacy-format regression coverage.

## Recovery completed

Only the original Slave app slot was restored from the verified backup. The device-computed checksum of the entire 4 MB flash now matches that original backup. The Slave was left in download mode; current project firmware is NOT running on it. No settings reset, flash erase-all, metadata or partition-layout change was performed. A successful upload verification must not be confused with successful boot.

Restoration evidence: SLAVE_COM8_RESTORATION_20261101.json. Backup checksum SHA-256: c52ccb54fa1c5be45e71c926ea9a0e09cb442007156635dd58a718f411a869de.

## Prepared repair — NOT deployed; owner approval required

Prepared files and REVIEW.json are in deployment/prepared/slave-legacy-boot-repair-20261101/. The proposal replaces the old bootloader with the current compatible bootloader, adds the partition-table MD5 record, and installs the shared application. All six existing partition entries, offsets, sizes and labels remain identical. NVS stays at 0x12000 (57,344 bytes), customization data stays at 0x20000, and both 1.5 MB app slots remain at 0x100000 and 0x280000. No huge_app migration is proposed for the Slave.

Planned writes:

| Region | Offset | File bytes | Sector range affected, end exclusive |
|---|---|---|---|
| Bootloader | 0x1000 | 17,536 | 0x1000–0x6000 |
| Partition-table sector with checksum | 0x8000 | 4,096 | 0x8000–0x9000 |
| Shared application | 0x100000 | 1,369,936 | 0x100000–0x24F000 |

The existing verified encrypted full backup provides recovery material. Before any approved repair: confirm physical MAC and verify the current full flash against that original backup; write only the three reviewed regions; verify the whole flash against the exact expected image before boot. Recovery would restore those same regions from the original encrypted backup, without erasing or replacing NVS/OTA/customization regions. Bootloader and table updates are not atomic; an interruption may require USB download-mode recovery. No eFuse, flash-encryption or secure-boot setting is to be changed.

Approval is required by the owner's earlier instruction not to automatically flash changed bootloader/partition metadata. Files were prepared only; this report is not authorization to apply them.

## Master COM11 and LoRa

Master ESP32-10777D842178 remains MASTER at 192.168.1.106, EMQX connected with TLS VERIFIED. No Wi-Fi, broker profile or sensor settings were changed in this retry. Latest physical sensor observation: INA219 OK, MQ135 raw warm-up readings, DHT11 READ_ERROR.

Read-only Master E220 probe failed: SETTINGS_READ_TIMEOUT, AUX not ready, public_settings_read false, zero register writes and zero UART transmit fragments. No RF test, Slave discovery, LoRa ACK/RESPONSE or forwarded telemetry passed. Slave cannot be inspected by the project service until compatible firmware boots. Verify actual E220 power/common ground, antenna and TX/RX/M0/M1/AUX wiring before further RF work; do not bypass the guard or enable an unverified radio profile.

## Software checks

Eight flash-target/expected-image/NVS/MD5 tests pass, including rejection of ambiguous selection and corrupted checksums. The current firmware build, native LoRa/E220/shared-control tests and contract/gzip checks passed in the preceding deployment phase. The retry changed PC-side deployment/serial helpers only; application image was unchanged. No Gateway UI or paused manual work was altered.
