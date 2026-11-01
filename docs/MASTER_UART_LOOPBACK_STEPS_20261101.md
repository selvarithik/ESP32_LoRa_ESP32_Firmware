# Master isolated UART loopback and E220 supply check

Current physical identity: Master ESP32-10777D842178. Confirm COM port by USB status after reconnecting; COM numbers are not identity.

## Actual voltage check

The last physical USB status returned INA219 READ_ERROR with no valid voltage/current/power values. No numeric E220 supply voltage was measured through software. Never treat a stale dashboard value or zero placeholder as a valid measurement.

Series load wiring: ESP32 USB-fed 5V/VIN -> INA219 VIN+; INA219 VIN- -> E220 VCC; E220 GND -> ESP32 GND. INA219's separate VCC -> ESP32 3V3, GND -> common GND, SDA ->21, SCL ->22. No direct parallel wire from ESP32 5V to E220 VCC should bypass the shunt. VIN+ and VIN- are measurement terminals, not the INA219 logic supply. Bus voltage measures VIN- to ground (the load supply). An R100 shunt is 0.1 ohm.

Reference: https://www.ti.com/lit/ds/symlink/ina219.pdf

Use a multimeter in DC-voltage mode with black probe on E220 GND and red on E220 VCC. Expect roughly 5V with the requested USB-fed 5V connection; measure the actual board rail rather than assuming its voltage. The previously verified E220 manual specifies 3.0-5.5V supply. Do not probe current by placing an ammeter across power/ground. INA219 VCC should be about 3.3V in this wiring. A multimeter/2-second INA sampling can miss transient dips.

## Prepare isolated UART test (requires new diagnostic application)

1. Unplug Master USB and any other power.
2. Disconnect the entire E220 module from the Master. This avoids driving a module output, back-powering a disconnected module, or sending RF/test bytes through it.
3. Join only ESP32 GPIO17 to GPIO16 with one jumper. Do not connect either to 5V or GND. Leave the Slave unchanged.
4. Reconnect Master USB; close serial monitors. Tell the operator its actual COM port and confirm the radio is isolated before deployment/test.
5. Build 1.1.5-uart-loopback is prepared in the existing application. It is NOT installed until an identity-confirmed, backed-up application-only upload is performed. Keep existing partition layout and settings.
6. After deployment, execute the helper from the firmware project in PowerShell:

```powershell
& 'C:\Users\Admin\.platformio\penv\Scripts\python.exe' tools\uart_loopback.py --port COM8 --confirm-device-id ESP32-10777D842178 --confirm-isolated
```

The helper verifies Master identity before invoking the physical USB-only operation. It requires a disabled idle radio, tests 16 fixed binary bytes at 9600 8N1 for 250 ms, does not return arbitrary incoming data, and releases UART pins afterward. No saved settings or radio registers are written. Unknown-operation means the new diagnostic application has not been installed; do not interpret it as hardware failure.

Expected closed-loop result: UART_LOOPBACK_PASS_RADIO_NOT_TESTED, 16 sent /16 received. This tests ESP UART2, GPIO17 TX and GPIO16 RX with the jumper, not the E220 radio. As an additional negative control, power off, remove the jumper, power on and repeat: expect UART_RX_NO_RESPONSE; a pass with the jumper removed indicates an unintended echo/short and invalidates the isolated result.

If closed-loop fails, check the jumper/actual numbered pins; an alternative-pin or external USB-UART/logic analyzer test is then needed to isolate the board UART/pad path. If it passes, power off, remove the loopback jumper, restore E220 wiring/antenna and supply, then repeat the read-only settings probe. Any further RF test requires valid compatible permitted radio configuration and pairing; UART success alone is not RF success.

## Current status

New loopback feature is prepared; physical isolated loopback and E220 voltage measurement are pending hardware preparation. No firmware deployment or loopback execution was performed by preparing this document. Wi-Fi, MQTT profiles and sensor settings were not changed.
