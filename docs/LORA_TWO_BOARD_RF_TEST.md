# Two-board LoRa acceptance

Use Master COM10 (`ESP32-10777D842178`), Slave COM8 (`ESP32-842F2AE81F84`), the two E220-900T22D modules and the Gateway laptop. The current HTTP telemetry path provides a baseline. A relay is unnecessary for this test.

## Setup

1. Have the test operator specify the authorized frequency, occupied bandwidth, antenna or conducted setup, transmit power, channel-access method and airtime limits. Record the approval or lab test reference. The observed channel 18 (868.125 MHz), 22 dBm setup is a UART observation and remains disabled for RF.
2. Install the current firmware using the application-only backup and verification workflow. Preserve each board's partition layout, Wi-Fi, MQTT and sensor settings. Inspect both E220s over USB before applying the test profile.
3. Set the radio profile on both devices and verify the module registers after restart. Provision the mesh credential over USB if needed. Keep normal mode for the initial test and enable packet RSSI only with matching settings on both devices.

## Evidence required

| Check | Evidence to record |
| --- | --- |
| Link in both directions | Authenticated RX messages on both boards, recent peer IDs and packet RSSI in dBm. |
| Direct route | Master reports the Slave as next hop, one hop. |
| Command delivery | Target the Slave with an addressed PING. Record one command ID, the Slave's ACK and execution response, and radio RX evidence on both boards. |
| End-to-end telemetry | Record fresh Slave INA219 bus and shunt readings persisted by Gateway and matching radio receive evidence at Master. Isolate direct HTTP for this run so an HTTP receipt cannot pass the LoRa test. Restore direct HTTP afterward. |
| Recovery | Interrupt and restore the test link. Record route expiry, retry outcome and fresh readings after recovery. |

Leave unmeasured items pending. A UART register read, UART transmit count or Gateway MQTT receipt by itself cannot prove the RF path.

## Conducted test result, 12 November 2026

- Master COM10 and Slave COM8 both read back channel 16 (866.125 MHz), 10 dBm, 2.4 kbps, transparent normal mode and packet RSSI enabled after restart. The radios remained enabled for the attenuated bench setup.
- Both hosts wrote UART fragments. A staggered restart and observation showed zero valid received messages at either end. The Master reported one rejected frame. There was no usable packet RSSI.
- Gateway PING `CMD-90269D34A4B4` was dispatched and timed out without a device ACK or execution response. The last persisted Slave INA219 readings available during the test came from direct HTTP before the route was switched to Master-only.
- The firmware source now clears the previous settings result after a persistent-write echo, waits for AUX and requests a fresh readback. This correction was built and tested in software. The running boards used the previous firmware and read the new persistent register values on their next boot.
- The next physical check is a known-good 50-ohm conducted path with a measured signal at each receiving port. The E220 also has write-only radio key registers, so matching public settings do not establish matching radio keys. Inspect that commissioning history if the conducted path checks out.
