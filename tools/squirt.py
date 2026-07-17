#!/usr/bin/env python3
"""Drive the emulator over BLE, for end-to-end verification.

A development aid, not part of the firmware or the flashing path. Needs bleak,
which uv fetches on the fly -- nothing to install:

    uv run --with bleak tools/squirt.py --scan   # just find the device
    uv run --with bleak tools/squirt.py --hold 3 # hold the relay closed for 3s
    uv run --with bleak tools/squirt.py --hold 40  # trips the max-on lockout
    uv run --with bleak tools/squirt.py --once   # one command, relay ticks

Interrupting a hold is itself a test: the relay must drop on disconnect.
"""

import argparse
import asyncio
import sys
import time

try:
    from bleak import BleakClient, BleakScanner
except ImportError:
    sys.exit("bleak is not installed. Run: pip install bleak")

SERVICE_UUID = "0000ffe5-0000-1000-8000-00805f9b34fb"
WRITE_UUID = "0000ffe9-0000-1000-8000-00805f9b34fb"
DEFAULT_NAME = "Hismith Piupiu"

# Device Information Service. The emulator advertises under the Hismith name so
# hosts recognise it, and reports what it really is here.
DIS_MANUFACTURER_UUID = "00002a29-0000-1000-8000-00805f9b34fb"
DIS_MODEL_UUID = "00002a24-0000-1000-8000-00805f9b34fb"
DIS_FIRMWARE_UUID = "00002a26-0000-1000-8000-00805f9b34fb"


def frame(cmd: int, arg: int) -> bytes:
    """0xCC | cmd | arg | (cmd + arg) & 0xFF"""
    return bytes([0xCC, cmd, arg, (cmd + arg) & 0xFF])


SQUIRT = frame(0x0B, 0x01)  # cc 0b 01 0c


async def find(name: str, timeout: float):
    print(f"scanning for {name!r} ...")
    device = await BleakScanner.find_device_by_filter(
        lambda d, adv: (d.name == name)
        or (SERVICE_UUID.lower() in [u.lower() for u in adv.service_uuids]),
        timeout=timeout,
    )
    if device is None:
        sys.exit(f"no device matching {name!r} or service {SERVICE_UUID} found")
    print(f"found {device.name} @ {device.address}")
    return device


async def describe(client) -> None:
    """Read the Device Information Service, if the peer has one.

    A real Piupiu will not report these, so this is how you tell the emulator
    apart from the device it is imitating.
    """
    fields = (
        ("manufacturer", DIS_MANUFACTURER_UUID),
        ("model", DIS_MODEL_UUID),
        ("firmware", DIS_FIRMWARE_UUID),
    )
    for label, uuid in fields:
        try:
            value = await client.read_gatt_char(uuid)
            print(f"  {label}: {value.decode('utf-8', 'replace')}")
        except Exception:
            print(f"  {label}: not reported")


async def run(args) -> None:
    target = args.address or await find(args.name, args.timeout)
    if args.scan:
        return

    async with BleakClient(target) as client:
        print("device info:")
        await describe(client)
        print(f"connected; writing {SQUIRT.hex(' ')} to {WRITE_UUID}")

        if args.once:
            await client.write_gatt_char(WRITE_UUID, SQUIRT, response=False)
            print("sent 1 command")
            return

        interval = 1.0 / args.rate
        deadline = time.monotonic() + args.hold
        sent = 0
        while time.monotonic() < deadline:
            await client.write_gatt_char(WRITE_UUID, SQUIRT, response=False)
            sent += 1
            await asyncio.sleep(interval)

        print(f"sent {sent} commands over {args.hold}s at {args.rate} Hz")
        print("relay should release shortly after the last one (HOLD_MS)")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--name", default=DEFAULT_NAME, help="advertised name to look for")
    parser.add_argument("--address", help="connect straight to this BLE address")
    parser.add_argument("--scan", action="store_true", help="find the device and exit")
    parser.add_argument("--once", action="store_true", help="send a single squirt command")
    parser.add_argument("--hold", type=float, default=3.0, help="seconds to keep commanding (default: 3)")
    parser.add_argument("--rate", type=float, default=5.0, help="commands per second (default: 5)")
    parser.add_argument("--timeout", type=float, default=10.0, help="scan timeout (default: 10)")
    args = parser.parse_args()

    try:
        asyncio.run(run(args))
    except KeyboardInterrupt:
        # Dropping the connection is itself a useful test: the firmware must
        # release the relay when the host disappears.
        print("\ninterrupted; relay should drop on disconnect")


if __name__ == "__main__":
    main()
