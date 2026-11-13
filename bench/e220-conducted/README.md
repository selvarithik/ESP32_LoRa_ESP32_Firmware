# E220 conducted bench

Build from the repository root with `pio run -d bench/e220-conducted`. This is an isolated PlatformIO project. Use a verified, application-only write to the board's existing boot slot. Do not run PlatformIO's upload target for this bench because that target also writes its generated partition table. The known hardware MACs select Master RX32/TX33 or Slave RX16/TX17. Both use M0 GPIO25, M1 GPIO26 and AUX GPIO27.

Keep both SMA ports joined by a 50-ohm cable and at least 30 dB total attenuation. Keep antennas disconnected and switch off both boards before opening the RF path. The sketch boots with no application transmissions and reads the live configuration. It refuses to send unless the public profile exactly matches CH16, 10 dBm, 2.4 kbps, address 0, transparent mode and packet RSSI enabled, and the module acknowledged the temporary key command in this boot.

USB commands at 115200 baud, newline terminated:

- `cfg read` reads the six public registers at 9600 8N1 and returns to normal mode after AUX settles.
- `cfg key0` compares the live public registers before and after the temporary key write. Its echo acknowledges the write. The key itself is write-only.
- `rx` enables timestamped raw capture. Capture is already enabled at boot. A 25 ms idle gap closes a burst and the final byte is labeled RSSI when the radio profile enables it and the burst contains at least two bytes.
- `send ping01` sends the six ASCII bytes once. There is a 16-byte limit, a 300-second wait from boot and between attempts, and a maximum of three attempts per board per boot. Keep both boards running for the entire session to retain the six-attempt limit. A UART write receipt records accepted bytes. Receiver capture is the RF evidence.
