Current local-IP implementation and physical acceptance: [LOCAL_IP_REPORT.md](docs/LOCAL_IP_REPORT.md). MQTT broker acceptance remains separate and pending ACL confirmation.

# ESP32 LoRa firmware

PlatformIO firmware for the ESP32 LoRa Gateway project. The companion Flask Gateway is [ESP32_LoRa_Project](https://github.com/selvarithik/ESP32_LoRa_Project).

The shared [configuration contract](CONFIG_CONTRACT.md) is version 1.0.2, wire schema 1. Both repositories must keep identical contract bytes.

## Build and checks

Use Python, Node.js and PlatformIO. Clone both repositories beside each other using their repository names so cross-project contract checks can locate the Gateway copy.

```powershell
python test/check_contract.py
python test/build_web_assets.py --check
node test/local_page.cjs
node test/mqtt_page.cjs
node test/access_page.cjs
python test/check_sessions.py
python test/check_mqtt_failure.py
python test/check_lora.py
pio run
python test/check_partitions.py
```

Building does not upload firmware. Review the board, current partition layout and backup requirements before any deployment. The existing PlatformIO application/OTA layout is retained.

## Configuration and acceptance

Credentials are provisioned locally through the existing USB commissioning workflow; do not commit private deployment profiles, credentials or flash dumps. See [commissioning](docs/COMMISSIONING.md) and the [implementation and acceptance report](docs/FINAL_ACCESS_REPORT.md).

The source includes browser login/account pages and canonical verified-TLS MQTT transport. A successful build does not establish live broker connectivity or physical delivery. The Gateway permanent worker requires verified broker authorization before enabling its ACL trust gate. The shared E220 UART driver is implemented and compiled, but RF starts disabled and the physical link is unverified. See docs/SHARED_FIRMWARE_REPORT.md.

Current sensor/BLE scope and physical acceptance: [BLE_SENSOR_REPORT.md](docs/BLE_SENSOR_REPORT.md). DHT11/INA219 and MQ135 raw telemetry are verified; MQ135 uses the owner-confirmed 10k/10k AO divider and remains uncalibrated.

Latest Master LED/TM1637 and enabled MQ135 raw telemetry: [connection indicators](docs/CONNECTION_INDICATORS.md).


Shared hardware builds (build only): `pio run -e esp32_master -e esp32_slave`.
Both environments compile this source tree. DEVICE_ROLE_MASTER / DEVICE_ROLE_SLAVE
are mutually exclusive. Current profiles inhibit TM1637; Slave inhibits INA219.
Radio defaults disabled pending read-only module settings inspection and safe physical
commissioning. Cloud runtime starts paused. No board upload or production broker test
is part of build verification. See docs/SHARED_FIRMWARE_REPORT.md and
 docs/CLOUD_PREPARATION.md for current status and the staged hardware test sequence.
