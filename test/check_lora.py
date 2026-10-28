"""Compatibility entry point for the current RV1 E220 software checks.

The former lora_host.cpp tested a removed newline UART implementation. The
current driver uses authenticated COBS/MsgPack fragments and ESP32 APIs; it is
built by PlatformIO. This command tests its portable framing/reassembly/routes,
not GPIO, HMAC hardware integration, on-air delivery, or the full driver loop.
"""
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
subprocess.run([sys.executable, str(root / 'test/check_e220.py')], check=True)
print('Current RV1 native checks passed; physical UART/RF and full driver-loop coverage are separate.')
