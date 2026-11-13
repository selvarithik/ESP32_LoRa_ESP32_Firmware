# E220 reference review

The project retains its own AUX-gated, nonblocking configuration state machine. It reads public registers with C1 in configuration mode, waits for an AUX rise and a 500 ms settling interval after writes, and confirms changes with a separate read. The receive stream separates the RSSI trailer from the COBS frame and only publishes a dBm value in the project's accepted range. No code was copied from these references.

| Reference | License | Comparison |
| --- | --- | --- |
| [KrisKasprzak/EBYTE-E220](https://github.com/KrisKasprzak/EBYTE-E220) | MIT | Provides mode switching, AUX waits or timed waits, packet RSSI stripping, and separate noise queries. Its optional AUX and wireless configuration paths are outside the conducted release. |
| [xreef/EByte_LoRa_E220_Series_Library](https://github.com/xreef/EByte_LoRa_E220_Series_Library) | MIT text with a redistribution attribution request in [LICENSE.md](https://github.com/xreef/EByte_LoRa_E220_Series_Library/blob/master/LICENSE.md) | Documents the received packet's trailing RSSI byte and the separate REG1 noise enable. The project uses authenticated COBS framing and retains invalid raw values for diagnosis. |
| [Tech500/E220-WOR-with-Dual-Core-FREERTOS-System](https://github.com/Tech500/E220-WOR-with-Dual-Core-FREERTOS-System) | MIT | The E220 configurator and dual-core wake-on-radio design are useful references for future sleep work. The example targets a 30 dBm module and a different sleep architecture. Its test results do not establish behavior of this conducted 22 dBm module. |
| [Tech500/Remote-Relay-with-Battery-Monitor](https://github.com/Tech500/Remote-Relay-with-Battery-Monitor) | No repository license found | Demonstrates E220-900T30D wake and INA226 logging. Its one-way example does not exercise authenticated command ACKs, execution responses, or the project's RSSI parser. |
