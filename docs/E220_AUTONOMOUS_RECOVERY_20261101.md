# Autonomous Master E220 recovery — 2026-11-01

Master ESP32-10777D842178 is COM8; Slave ESP32-842F2AE81F84 is COM11. Both identities checked. Owner requested no Wi-Fi/MQTT/sensor settings changes and autonomous software work while unavailable.

## Implemented and deployed

Shared firmware 1.1.6-e220-recovery contains a bounded public-register stream parser, synchronization past noise/stale prefixes, at most three read attempts, 100 ms configuration-mode settling with 20 ms observed AUX-high stability, clean UART initialization, weak RX pull-up, small RX FIFO threshold/timeout, and explicit diagnostic actual baud/peripheral. Added physical USB-only optional read-only UART1 diagnostic on the same GPIO16/17 wires; saved UART selection is unchanged and normal activation still uses UART2. Isolated loopback explicitly selects UART2 and biases/waits for an idle RX before testing. No radio register writes or application RF delivery test was performed.

## Physical outcome

Master UART2: three failed public-register read attempts, actual configured baud 9600; final partial reply F9FF. Master UART1 on the same pins: three failed attempts, actual configured baud 9600; final partial reply 7E. During queries raw M0/M1 sampled HIGH and AUX mostly HIGH at ESP32 pads. Those digital readings do not measure voltage at module pins. Slave public settings read passed. The fault is not shown to be UART2-specific; wiring/module-side mode signals, electrical signal quality and supply remain unverified. Exact cause is not proven and no hardware fix is claimed. Earlier closed-loop tests passed twice, with a first mismatch; open-input control saw stray bytes. These results do not prove consistent E220 communication.

Master INA219 still returns READ_ERROR, so no valid numeric E220 supply measurement is available. Physical voltage/waveform verification is the remaining action; repeating additional software probes is not justified by these results. Check load-side E220 VCC/GND and M0/M1 at the actual module before further radio activation.

## Preservation and verification

Full 4 MB Master flash backup was device-verified and DPAPI-protected before writing only application at 0x10000. Full expected-image MD5 verification proved all bytes outside erased application sectors unchanged before boot, including NVS, partition table, bootloader and filesystem. Saved role, sensor enable flags, MQTT active-profile fingerprint and revision compared equal after reboot. No Slave upload. Volatile UTC clock was restored via existing physical USB operation, without writing saved network or profile configuration.

Build succeeded: program 1,368,109 /3,145,728 bytes (43.5%); static RAM 76,140 /327,680 (23.2%); binary 1,374,816 bytes. New parser cases and existing E220 native tests passed in both role builds. Contract copies identical (98 settings inventory), SHA-256 2ab672c11966684547bda89f0b2022bb461e6e34584b0834450a625a53d9d183. Eight flash-layout tests passed. MQTT runtime recovery samples are recorded in the JSON report; do not equate compilation or these unit checks with RF success.

RF delivery/ACK/RESPONSE/forwarding remain NOT TESTED. Both saved radio enables were left unchanged. Slave readback remains channel18, power22dBm, LBT off; this observed profile is not a verified permitted Indian RF test profile. No permitted profile or pairing was silently invented or committed.
