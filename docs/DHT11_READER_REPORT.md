# Master DHT11 physical test - 2026-10-26

Status: software reader recovery implemented and uploaded; physical temperature/humidity acceptance FAILED.

Changes: existing RMT capture falls back on the next normal sampling interval to the already-installed checksum-validating Adafruit pulse reader. RX GPIO routing is restored before subsequent RMT attempts. Runtime telemetry identifies the actual reader. No saved settings mutations were sent.

Final application-only upload to Master COM8 was hash verified. Protected partition/NVS/OTA bytes were unchanged. Image: 1,307,008 bytes; linked program: 1,300,301 / 1,310,720 bytes (99.2%); RAM: 74,732 / 327,680 bytes (22.8%). Existing partition layout retained.

Production DHT decoder native tests and static cross-project contract checks passed. These do not establish sensor success.

Actual post-upload and post-user-reported power-cycle observations: RMT reports NO_RESPONSE with zero captured pulses; alternate reader reports POLLING_READ_ERROR; no valid DHT sample since boot. The reported uptime continued across the power-cycle confirmation, so complete loss of board power was not independently established.

Gateway latest_status contains the real DHT READ_ERROR and alternate-reader diagnostic. Fresh INA219 and MQ135 measurements continued arriving in the database at 20:42:20 local. Most recent DHT database values remain from 00:06:01 and are stale; they must not be described as live.

Next physical isolation: with all board power removed, replace the DHT11 DATA jumper and test a known-good 3-pin DHT11 module on the same GPIO4/3.3V/GND wiring. Check the module printed pin labels before reconnecting. If the replacement also fails, measure the start/response waveform on GPIO4 to distinguish GPIO, pull-up, wiring and module failure. Do not change Wi-Fi, MQTT or other sensor settings.

Evidence: MASTER_DHT_READER_FINAL_UPLOAD.json in firmware docs; DHT11_FINAL_PHYSICAL_TEST.json in Gateway docs.
