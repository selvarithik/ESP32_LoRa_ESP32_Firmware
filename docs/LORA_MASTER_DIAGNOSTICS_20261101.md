# Master E220 diagnostic update — 2026-11-01

Master ESP32-10777D842178 is COM8; Slave ESP32-842F2AE81F84 is COM11.

Implemented and deployed application 1.1.4-radio-diag2 to Master only. Added raw AUX/M0/M1 pad readings independent of driver readiness, probe phase, read-query flag, bounded public register response length/hex and a truthful partial-response error. Updated read-only host inspector to sample diagnostics. No register or saved configuration writes were issued. No RF test was attempted.

On Master, during READ_PUBLIC_REGISTERS, AUX, M0 and M1 sampled HIGH at ESP32 pads. Received invalid partial replies FC40F0 (3 bytes) and 0E06 (2 bytes); expected a valid 9-byte response. These are not the documented FF FF FF format-error response and do not establish the cause. Slave still returns a valid settings frame. Previous module swaps kept the failure on the Master setup. The remaining issue could involve the board UART pins, electrical connection, signal integrity or supply; raw GPIO levels at the ESP32 do not prove the signals reach the module correctly or measure voltage. Electrical isolation is required; no software fix or RF success is claimed.

Full 4 MB Master backups were device-verified and DPAPI-protected before each application update. Application-only writes at 0x10000 were verified by device MD5 against the expected full image. All bytes outside erased application sectors, including NVS, partition table, bootloader and filesystem, were unchanged before boot. No partition changes. MQTT profile fingerprint/revision, roles and sensor enable flags compared equal after boot.

Build: 1,364,457 program bytes / 3,145,728 (43.4%); static RAM 76,036 / 327,680 (23.2%); firmware binary 1,371,168 bytes. Contract and portable E220 checks passed for both role builds; 8 flash-layout tests passed. These software checks do not verify radio delivery. Master live transport status is recorded separately in the private status snapshots.

Pending: verify voltage/common ground and UART waveforms at the Master module pads, or perform an isolated UART loopback/alternate-pin hardware test. After valid public settings reads, check compatible permitted radio settings and authenticated pairing before RF ACK/RESPONSE and telemetry tests. Slave firmware remains unchanged.

After reboot, Wi-Fi recovered at 192.168.1.106. Restored volatile UTC clock through existing physical USB commissioning (no NVS/config change) because NTP had not synchronized. Master then reported MQTT connected, TLS VERIFIED and subscription READY.
