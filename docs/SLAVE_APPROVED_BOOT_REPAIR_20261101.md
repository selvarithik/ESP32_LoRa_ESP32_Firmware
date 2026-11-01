# Approved Slave COM8 boot repair — 1 November 2026

Owner explicitly approved repairing Slave bootloader and partition checksum while preserving partition sizes and Wi-Fi/MQTT/sensor settings.

## Completed

- Slave ESP32-842F2AE81F84, COM8: compatible bootloader at 0x1000, checksum-bearing table at 0x8000, and the exact shared 1.1.3-pair-sensors image at 0x100000 installed.
- All six partition entries, offsets, sizes and labels unchanged. Both 1.5 MB application slots retained. No huge_app migration on the Slave. No erase-all, eFuse or flash-encryption change.
- Initial preservation check detected that only NVS differed from the earlier backup. Current NVS was read and verified; a composite whole-flash image was independently device-checksummed and encrypted to current-before.dpapi before writing. Original backup remains intact. No newer NVS contents were rolled back silently.
- Entire post-write flash matched the expected device checksum before boot, preserving NVS, OTA metadata, factory customization and second application slot. Normal firmware initialization may subsequently write its own new NVS namespaces; pre-boot byte preservation is not a claim of byte-identical NVS forever.
- Slave boots and responds to physical USB status as NODE. Uptime increased across repeated checks, resolving the earlier checksum/assert boot loop. This is bounded boot verification, not a soak test.
- Master ESP32-10777D842178 remains MASTER on COM11, Wi-Fi 192.168.1.106, EMQX connected with verified TLS. Its firmware/profile was not rewritten in this repair.
- No Wi-Fi credentials, MQTT profiles or sensor settings were commissioned or edited. New Slave DHT11/MQ135 remain NOT_CONFIGURED; INA219 reads voltage/shunt but reports SHUNT_RESISTANCE_REQUIRED for current/power. These statuses were not replaced with invented values or automatic sensor configuration.

## LoRa physical result — not passed

Both E220 read-only inspections fail with AUX LOW and no public settings readback. Master: SETTINGS_READ_TIMEOUT. Slave: MODE_RESTORE_TIMEOUT. Zero radio-register writes and zero radio UART fragments; no RF transmissions or Master–Slave ACK/RESPONSE are claimed. Radio/peer/mesh commissioning remains pending after reliable hardware communication and permitted settings are established.

The owner reports E220 VCC connected to the ESP32 3.3 V pin. That voltage is within the module's working range, but an overloaded regulator or supply sag is a possible cause, not a confirmed diagnosis. Ebyte specifies typical 5 V power and approximately 110 mA TX current at 22 dBm; serial logic is 3.3 V. Reference: https://www.ebyte.com/Uploadfiles/Files/2021-7-6/202176188184448.pdf

Power off before rewiring. Prefer a stable regulated 5 V supply, common ground; use a board VIN/5V pin only if its actual voltage is confirmed around 5 V in the intended USB-powered arrangement. VIN is not universally a regulated 5 V output. Keep UART/control GPIO signals at 3.3 V and fit antennas before transmitting. Check AUX -> GPIO27, module TXD -> GPIO16, RXD -> GPIO17, M0 -> GPIO25, M1 -> GPIO26 using GPIO labels, not header pin numbers.

## Verification evidence

Physical deployment details and hashes are in SLAVE_APPROVED_BOOT_REPAIR_20261101.json. Bootloader SHA: 3d234a7471f67b013686dabd4dee7c1fa915c9928463616a94bc9297acf1abf8. Shared app SHA: 78859f95252149792872cc2c9ca7c457c70a62b2ae1eb7fb26b32b08b83202ba, matching the Master image from the prior upload. Eight host flash-layout/checksum/NVS regression tests passed previously; application image was unchanged in this approved repair. Private logs and encrypted backups remain under ignored local data folders. No Gateway UI, database device history, or paused manual work was changed.
