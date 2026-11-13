# Compact radio wire proposal

The bench image continues to use RV2. The compact reference codec and tests
are offline. A future firmware integration needs an explicit capability
exchange over RV2 before either peer sends compact frames. Unknown peers and
unsupported commands keep using RV2.

## Captured RV2 sizes

| Message | MessagePack envelope | MessagePack payload | Fragment headers (CRC included) | HMAC | COBS | Total encoded |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Registration reply | 124 | 1425 | 374 | 176 | 22 | 2121 |
| Poll requesting registration | 124 | 52 | 68 | 32 | 4 | 280 |
| Ordinary poll in Step 1 | 124 | 42 | 68 | 32 | 4 | 270 |
| Command ACK | 128 | 45 | 68 | 32 | 4 | 277 |
| PING result | 133 | 143 | 68 | 32 | 4 | 380 |

The 11-fragment registration reply length comes from the captured RV2 header
and the configured 148-byte fragment span. The full 1425-byte body was not
captured. The earlier 272-byte poll figure was an offline fixture. Captures
show the actual size varies with the payload and integer widths.

## Proposed wire sizes

| Message | Body | HMAC | COBS | Total | Model airtime |
| --- | ---: | ---: | ---: | ---: | ---: |
| Registration | 10 | 16 | 2 | 28 | 1.642496 s |
| Poll | 16 | 16 | 2 | 34 | 1.904640 s |
| Command ACK | 16 | 16 | 2 | 34 | 1.904640 s |
| PING result | 18 | 16 | 2 | 36 | 1.904640 s |
| INA219 | 15 | 16 | 2 | 33 | 1.773568 s |

Every body starts with a one-byte type, a two-byte commissioned node ID and
a four-byte poll token. Registration adds protocol version and capability
bits. The Master can complete registration in one addressed poll and one
reply. Device name, firmware version, sensor details and diagnostics can
be requested later through RV2. The HMAC also binds the full Master and Slave
MAC addresses, so the short node ID alone is insufficient to select a peer.

Poll, ACK and result use an eight-byte command handle. The Master maps it
to the Gateway command ID. Polls carry a short opcode, ACKs carry acceptance,
and PING results carry outcome and elapsed milliseconds. Text echo and
other variable results keep the RV2 path. Token and command-handle state
must reject stale or repeated replies. A production implementation must
consider token collisions across reboot and retain command deduplication.

INA219 sends sample time and raw bus/shunt counts (4 mV and signed 10 µV
per count). Units and field names are implied. Current and power can be
derived if the receiver knows the commissioned shunt resistance. Each
complete compact frame keeps the 128-bit HMAC. RV2 keeps its reassembly CRC;
the compact messages have no reassembly stage, and the HMAC covers every
body byte. The E220 PHY CRC assumption is unchanged.

Under the unverified SF11/125 kHz model and 36 s per rolling hour per node,
the conservative integer limit for these one-fragment turns is 18 per hour
with 200 seconds between starts. A complete PING needs separate ACK and
result turns. Mixed RV2 traffic remains subject to the rolling refusal limit.
