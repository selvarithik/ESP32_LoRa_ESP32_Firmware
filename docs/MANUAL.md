# Conducted E220 operating manual

## Bench setup

Use the two E220-900T22D antenna ports through an SMA cable and at least 30 dB total attenuation. Keep both ports terminated and remove radiating antennas. Verify the bench profile on both units before the first transmission.

| Signal | Master ESP32 | Slave ESP32 |
| --- | --- | --- |
| E220 TXD to ESP32 RX | GPIO32 | GPIO16 |
| E220 RXD from ESP32 TX | GPIO33 | GPIO17 |
| E220 M0 / M1 / AUX | GPIO25 / GPIO26 / GPIO27 | GPIO25 / GPIO26 / GPIO27 |
| Common ground | GND | GND |

Read and preserve the actual application slots and data partitions. On the November 13 test boards, COM8 Slave used OTA0 at `0x100000` and COM10 Master used app0 at `0x10000`. Other boards may differ. Build with `pio run -e CONDUCTED_BENCH` and write only the verified active app partition. Boot Slave first. Confirm the bench banner, 15 s holdoff, zero transmissions during holdoff, and the public register reply `C1 00 06 00 00 62 03 10 83` on each board.

The bench profile is CH 16 (866.125 MHz), 10 dBm, 2.4k, transparent mode, packet RSSI enabled, ambient RSSI and LBT disabled. For readback, use M0=M1=1, 9600 8N1, and `C1 00 06`. The firmware allows three retries for an empty response. After an authorized write, wait for an AUX rising edge and at least 500 ms before reading back. The crypt key is write-only. Web radio setup is read-only. A bench USB request `{"op":"bench_radio_channel","channel":16,"confirm":"CONDUCTED_ONLY"}` is accepted only with the driver stopped or faulted. The request allows channels 15–17 and requires the read-back profile to remain at 10 dBm and 2.4k. The conducted run uses CH 16. The command keeps the driver stopped after verification. To inspect ambient noise separately, the E220 manual describes REG1 bit 5 and the local noise-register query. The bench profile leaves that bit off.

## Protocol and airtime

The Master grants addressed turns and the Slave only replies on a valid turn. The compact v3 header contains magic, version, kind, source/destination MACs and a 32-bit poll token. Payloads carry a capability bitmap (registration), a registration flag (poll), a command ID and time (PING), command ID and state (ACK), or command ID, outcome and duration (response). INA219 carries sample time, scaled bus and shunt voltage, and shunt resistance. Config carries six public E220 register bytes. The 128-bit HMAC authenticates the header and payload. COBS supplies a delimiter. The optional RSSI trailer comes from the receiving radio and lies outside the authenticated frame.

The planning model assumes SF11, 125 kHz, CR 4/8, explicit header, PHY CRC, low-data-rate optimization and an 8-symbol preamble. It counts encoded bytes and a 16-byte allowance for module overhead. Use the `[BENCH TX]` log for counts and modeled airtime. The conducted build uses a 60 s polling interval, 600 s/hour budget, 20 s per-board transmit gap and 30 transmissions per boot. The production environment uses a 30 minute poll interval, a 36 s/hour rolling limit and a boot holdoff of 60 minutes plus 15 seconds. A reboot clears the in-memory ledger.

## Gateway and recovery

Use the existing Gateway sign-in and open LoRa Mesh Monitor on the trusted LAN. Public configuration projections and received signal fields also appear in `/api/operations/overview`. The existing MQTT status envelope carries `radio_config` and `radio_signal` when reported. Existing MQTT telemetry carries the INA219 readings. Radio channel and power flags are advisory. If the device clock remains at uptime, an authorized USB operator can provide current Unix time using the existing `time` command and verify TLS certificate validation before testing a Gateway PING.

A missing ACK can occur before LoRa if the Master lacks a verified MQTT connection. Compare the Gateway command timeline with Master TX and Slave RX logs. A successful ACK is distinct from an execution result. Confirm a fresh INA219 reading by matching the Slave identity, RF frame and Gateway ingest timestamp. If the register read is empty, check AUX and mode-pin timing. If link frames arrive but auth failures increase, compare the application HMAC provisioning and examine the radio key history. If registration stalls, check fragments and the per-turn cap. Raw values FF and FE are retained as received and displayed as n/a. The cause of those values remains unconfirmed.

To restore production later, build the supported production environment for the board, verify its image fits the current application partition, back up flash and write only the verified application slot. Restore any planned RF profile separately under applicable approvals. Real RF airtime, occupied bandwidth, WPC category and type approval, over-the-air operation, and radio key persistence remain unverified.
## November 13, 2026 conducted release-candidate run (IST)

Firmware `e44501b` was flashed Slave first and Master second. Both application writes verified. Both boards returned `C1 00 06 00 00 62 03 10 83` at 19:15:50 and held TX at zero through the 15 s boot holdoff. The Master received registration at 19:16:59 (raw FF) and the new one-fragment configuration report at 19:17:39 (raw FE). The Slave measured between −32 and −34 dBm in the same session. Each Master capture had a COBS delimiter immediately before its raw RSSI byte. The invalid RSSI readings are now reported as n/a.

Command `CMD-6DDD0B4F6B2F` was dispatched at 19:16:07 while the Master clock was unsynchronized. USB time synchronization at 19:20:06 restored TLS. The Gateway stored fresh Slave INA219 via the Master at 19:20:41.305916 (4.648 V bus and 0.85 mV shunt, Master RSSI n/a, raw FF). No manual PING retry was issued. The Gateway showed no ACK or execution response in the new run. In the preceding v3 run, command `CMD-C4DD6C44470C` recorded its ACK at 18:49:58.350733 and execution response at 18:50:58.541078. Those are results from the prior image.

The follow-up Master monitoring build compiled offline. Its full web and MQTT reporting requires a fresh end-to-end verification. Node-to-node chat and per-node retained MQTT monitoring remain incomplete.


## November 14, 2026 conducted acceptance (IST)

One Gateway PING, `CMD-23F28DF0E544`, was dispatched at 00:16:48.903720 after the Master reported verified TLS and ready MQTT subscriptions. No manual retry was sent. The public register readback matched `C1 00 06 00 00 62 03 10 83` on both nodes.

| Event | Gateway timestamp (IST) | Evidence |
| --- | --- | --- |
| Slave ACK | 00:17:49.886617 | COM10 raw RSSI `00`, displayed as n/a |
| Slave execution response | 00:18:50.077066 | Gateway state COMPLETED, `ok=true`, `RADIO:ESP32-10777D842178`, verified MQTT. COM10 raw RSSI `00`, displayed as n/a |
| Fresh Slave INA219 | 00:19:50.017334 | 4.680 V bus, 0.26 mV shunt, source MQTT. COM10 raw RSSI `00`, displayed as n/a |

At 00:20:00 the Master had sent 6 fragments totaling 18.768 s modeled airtime. The Slave had sent 5 fragments totaling 11.096 s and last measured the Master at −34 dBm. Both nodes recorded zero rejects, auth failures and budget refusals at that checkpoint.

The firmware and Gateway source are published as a conducted release candidate. Authenticated node chat, full per-node retained MQTT status and the complete live monitoring view still require acceptance. Real airtime, RF bandwidth, key persistence, regulatory approval and over-the-air operation are outside this evidence.

**Known limitation: Master-side RSSI unverified.** The captured v3 ACK, response and INA219 frames each end with delimiter 00 followed by a raw RSSI byte of 00. The parser stores the second byte and reports n/a. Earlier FF and FE trailers were also visible in raw UART captures. The module-level cause remains unconfirmed.


## Authenticated node chat

Use a trusted LAN. The ESP32 chat page uses plain HTTP. Provision a different PIN on each node through physical USB using a `chat_pin` commissioning request. Use 8–24 characters, keep the PIN in your local password manager, and never put it in Git. The node stores a verifier in NVS. Open `/node-chat` at each ESP32 station address printed at boot. Enter its PIN. Each message accepts 1–24 printable ASCII bytes. The queue accepts at most four outgoing messages and one submission every five seconds. Sent indicates the radio driver accepted the turn. Delivered indicates an authenticated peer receipt. The Master selects the first known radio peer in this version.

`POST /api/node-chat/login` accepts a JSON PIN and issues a short-lived bearer token. `GET /api/node-chat/messages?since=0` reads updates and link status. `POST /api/node-chat/messages` queues text. Five failed login attempts block requests for 60 seconds. The session expires after 30 minutes idle. For physical USB testing only, `chat_send` accepts a text field and uses the same radio queue. Compact v3 adds kind 9 for a 32-bit ID, one-byte length and 24-byte ASCII slot, and kind 10 for a 32-bit peer receipt. Both use the existing authenticated header and turn token.

## Gateway retained MQTT

Retained JSON topics are under `<configured-prefix>/<gateway-id>/lora/<node-id>/` and end in `status`, `link`, `config`, `airtime`, `counters`, `alarms`, or `telemetry/ina219`. Each payload uses `schema_version: 1`. Subscribe to `<configured-prefix>/<gateway-id>/lora/availability` for the Gateway Last Will. Slaves have no broker connection, so their status is inferred from the last authenticated report. Consumers should read availability alongside the per-node topics. Missing observations are null. The Gateway MQTT account needs publish ACL for this subtree. Existing command and telemetry topics continue to use their original schema.

Gateway LoRa Mesh Monitor uses the existing Gateway sign-in and refreshes radio observations every five seconds. Measured packet RSSI excludes raw values outside the configured plausibility range. Link levels are 4 at −70 dBm or stronger, 3 at −90, 2 at −110, and 1 below −110. The monitoring projection caps poor delivery at level 2 when counters support that calculation.
