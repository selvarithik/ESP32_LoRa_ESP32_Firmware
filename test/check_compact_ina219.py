"""Offline wire proposal. No production encoder or receiver uses this format."""
import hashlib
import hmac
import struct
import unittest


def cobs(data):
    wire = bytearray([0])
    block = 0
    for byte in data:
        if byte:
            wire.append(byte)
        else:
            wire[block] = len(wire) - block
            block = len(wire)
            wire.append(0)
    wire[block] = len(wire) - block
    return bytes(wire) + b"\0"


def uncobs(wire):
    if not wire or wire[-1] or b"\0" in wire[:-1]:
        raise ValueError("Invalid COBS frame")
    end = len(wire) - 1
    data = bytearray()
    i = 0
    while i < end:
        block = i + wire[i]
        if block > end:
            raise ValueError("Invalid COBS length")
        data.extend(wire[i + 1:block])
        if block < end:
            data.append(0)
        i = block
    return bytes(data)


SAMPLE = struct.Struct(">BHIIHh")
DOMAIN = b"SELVA-INA219-v1\0"
TAG_BYTES = 16


def tag(body, key, peer):
    return hmac.new(key, DOMAIN + peer + body, hashlib.sha256).digest()[:TAG_BYTES]


def encode(key, peer, flags, node, turn, timestamp, bus_count, shunt_count):
    body = SAMPLE.pack(0x30 | flags, node, turn, timestamp, bus_count, shunt_count)
    return cobs(body + tag(body, key, peer))


def decode(wire, key, peer, expected_node, expected_turn):
    data = uncobs(wire)
    if len(data) != SAMPLE.size + TAG_BYTES:
        raise ValueError("Invalid INA219 length")
    body, actual = data[:-TAG_BYTES], data[-TAG_BYTES:]
    if not hmac.compare_digest(actual, tag(body, key, peer)):
        raise ValueError("Invalid INA219 authentication tag")
    version, node, turn, timestamp, bus, shunt = SAMPLE.unpack(body)
    if version not in (0x30, 0x31) or node != expected_node or turn != expected_turn:
        raise ValueError("Invalid INA219 type or poll")
    return version & 1, timestamp, bus, shunt


class CompactIna219(unittest.TestCase):
    key = bytes(range(32))
    peer = bytes.fromhex("7821847d7710841fe82a2f84")  # Master MAC then Slave MAC

    def test_one_fragment_and_signed_shunt(self):
        wire = encode(self.key, self.peer, 1, 9, 1234, 1794550800, 1135, -76)
        self.assertEqual(len(wire), 33)
        self.assertEqual(decode(wire, self.key, self.peer, 9, 1234), (1, 1794550800, 1135, -76))

    def test_tamper_wrong_peer_and_wrong_poll(self):
        wire = encode(self.key, self.peer, 0, 9, 1234, 0, 0, 0)
        body = bytearray(uncobs(wire))
        body[12] ^= 1
        for sample, peer, turn in ((cobs(body), self.peer, 1234),
                                   (wire, self.peer[::-1], 1234),
                                   (wire, self.peer, 1235)):
            with self.assertRaises(ValueError):
                decode(sample, self.key, peer, 9, turn)


if __name__ == "__main__":
    unittest.main()
