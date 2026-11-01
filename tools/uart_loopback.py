"""Physical USB diagnostic: E220 disconnected; jumper between the active configured radio RX/TX pins required."""
import argparse
import json
import serial
from radio_inspect import exchange


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True)
    parser.add_argument('--confirm-device-id', required=True)
    parser.add_argument('--confirm-isolated', action='store_true', required=True,
                        help='E220 completely disconnected; ONLY the active configured RX/TX pins joined')
    args = parser.parse_args()
    port = serial.Serial(port=None, baudrate=115200, timeout=.2, write_timeout=2)
    port.dtr = False
    port.rts = False
    port.port = args.port
    try:
        port.open()
        before = exchange(port, {'op': 'status'})
        if before.get('device_id') != args.confirm_device_id or before.get('role') != 'MASTER':
            raise RuntimeError('MASTER_IDENTITY_MISMATCH_NO_TEST')
        pins = before.get('radio', {}).get('pin_levels', {})
        rx, tx = pins.get('rx_gpio', 16), pins.get('tx_gpio', 17)
        if (rx, tx) not in ((16, 17), (32, 33)):
            raise RuntimeError('UNSUPPORTED_UART_PINS_NO_TEST')
        confirmation = ('E220_TX_RX_DISCONNECTED_GPIO16_TO_GPIO17' if (rx, tx) == (16, 17)
                        else f'E220_DISCONNECTED_GPIO_RX{rx}_TX{tx}_SHORTED')
        result = exchange(port, {'op': 'uart_loopback',
                                'confirm_device_id': args.confirm_device_id,
                                'confirm': confirmation})
        print(json.dumps({'device_id': args.confirm_device_id, 'result': result}, indent=2))
        return 0 if result.get('ok') else 1
    finally:
        port.close()


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (serial.SerialException, OSError, RuntimeError) as error:
        print(str(error))
        raise SystemExit(1)
