# Master isolated UART loopback — 2026-11-01

Physical Master ESP32-10777D842178 on COM8 received application 1.1.5-uart-loopback after verified DPAPI-protected 4 MB backup. Application-only deployment at 0x10000 was verified against the expected full flash image using device MD5; all bytes outside the erased application sector range remained unchanged before boot. NVS, bootloader, partition table and filesystem were preserved. Slave COM11 was read for identity only and not uploaded.

Owner replied COM8 to the isolation prompt (E220 completely disconnected; jumper GPIO17 to GPIO16). Physical wiring is not independently inspected. First 250 ms test sent and received 16 bytes but data differed. Both subsequent tests passed with an exact 16-byte match at 9600 8N1. This demonstrates successful loopback in two runs, not a consistently fault-free UART or any E220/RF result. The cause of the first mismatch remains unknown. Negative control without the jumper is pending owner physical preparation. COM8 disappeared after this power-off preparation was requested; no further test was attempted. After negative control, restore module wiring with power off and repeat public register inspection before considering RF.

Volatile clock restored over existing USB commissioning after reboot; no stored clock/network/MQTT configuration changed. Saved roles, sensor flags and MQTT active-profile fingerprint/revision compared equal. No radio registers were written. INA219 most recently reported READ_ERROR; no valid supply-voltage measurement was available. Fresh MQTT connection state was not captured after the board disconnected; no reconnection success is claimed in this test.

Build: program 1,366,921 /3,145,728 bytes (43.5%); static RAM 76,036 /327,680 (23.2%); binary 1,373,632 bytes. Build and existing E220 native software tests passed. These are separate from physical loopback outcomes.

## Negative control update
Owner replied check COM8 to jumper-removal prompt. Two observed open-input tests each returned 1 byte for 16 sent (UART_RX_LENGTH_MISMATCH); no full-pattern echo was observed. Strict zero-byte control did not pass; floating input/startup noise is possible but not established. A third command response was unavailable; its outcome remains unknown and it was not blindly retried. No full RF or module success claimed.
