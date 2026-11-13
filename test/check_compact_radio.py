"""Offline model for a negotiated compact radio protocol."""
import hmac
import struct
import unittest

from check_compact_ina219 import cobs, uncobs, encode as encode_ina219, decode as decode_ina219

DOMAIN = b"SELVA-COMPACT-RADIO-v1\0"
FORMATS = {
    0x40: struct.Struct(">BHIBH"),   # registration: token, version, capabilities
    0x41: struct.Struct(">BHIBQ"),   # poll: token, opcode, command handle
    0x42: struct.Struct(">BHIQB"),   # ACK: token, command handle, acceptance
    0x43: struct.Struct(">BHIQBH"),  # PING response: token, handle, outcome, elapsed ms
}
TAG_BYTES = 16


def encode(key, peer, kind, node, turn, *fields):
    body = FORMATS[kind].pack(kind, node, turn, *fields)
    return cobs(body + hmac.digest(key, DOMAIN + peer + body, "sha256")[:TAG_BYTES])


def receive(wire, key, peer, kind, node, turn):
    raw = uncobs(wire)
    if len(raw) != FORMATS[kind].size + TAG_BYTES:
        raise ValueError("Invalid compact frame size")
    body, auth = raw[:-TAG_BYTES], raw[-TAG_BYTES:]
    if not hmac.compare_digest(auth, hmac.digest(key, DOMAIN + peer + body, "sha256")[:TAG_BYTES]):
        raise ValueError("Invalid compact frame tag")
    observed = FORMATS[kind].unpack(body)
    if observed[:3] != (kind, node, turn):
        raise ValueError("Unexpected message, peer or poll")
    return observed[3:]


def airtime_us(encoded_bytes):
    blocks = (8 * (encoded_bytes + 16) + 35) // 36
    return (49 + 4 * (8 + 8 * blocks)) * 4096


class CompactRadio(unittest.TestCase):
    key = bytes(range(32))
    peer = bytes.fromhex("7821847d7710841fe82a2f84")

    def test_one_fragment_per_message(self):
        fixtures = [(0x40, (2, 0x0001), 28), (0x41, (1, 123), 34),
                    (0x42, (123, 1), 34), (0x43, (123, 1, 12), 36)]
        for kind, fields, size in fixtures:
            wire = encode(self.key, self.peer, kind, 9, 99, *fields)
            self.assertEqual(len(wire), size)
            self.assertEqual(receive(wire, self.key, self.peer, kind, 9, 99), fields)
        wire = encode_ina219(self.key, self.peer, 1, 9, 99, 1794550800, 1135, -76)
        self.assertEqual(len(wire), 33)
        self.assertEqual(decode_ina219(wire, self.key, self.peer, 9, 99), (1, 1794550800, 1135, -76))

    def test_reject_wrong_turn_and_tamper(self):
        wire = encode(self.key, self.peer, 0x42, 9, 99, 123, 1)
        with self.assertRaises(ValueError):
            receive(wire, self.key, self.peer, 0x42, 9, 100)
        altered = bytearray(uncobs(wire))
        altered[7] ^= 1
        with self.assertRaises(ValueError):
            receive(cobs(altered), self.key, self.peer, 0x42, 9, 99)

    def test_production_model(self):
        worst = max(airtime_us(n) for n in (28, 34, 34, 36, 33))
        turns = 36_000_000 // worst
        self.assertEqual(turns, 18)
        self.assertEqual(3_600_000 // turns, 200_000)


if __name__ == "__main__":
    unittest.main()
