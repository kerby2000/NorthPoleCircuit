#!/usr/bin/env python3
"""Try to recover a CH59x/CH592 with corrupted/protected config.

This is a Windows-friendly adaptation of the public CH59x recovery gist:
https://gist.github.com/biemster/93f3fb932558063928562fd5ffb0cd1f

It polls for the native USB ISP bootloader device and can write a known
development config word block. Use only when normal WCHISPStudio / wlink
recovery paths fail.
"""

from __future__ import annotations

import argparse
import sys
import time
from typing import Iterable


CH_USB_VENDOR_IDS = (0x4348, 0x1A86)
CH_USB_PRODUCT_ID = 0x55E0
CH_USB_EP_OUT = 0x02
CH_USB_EP_IN = 0x82
CH_USB_PACKET_SIZE = 64
CH_USB_TIMEOUT_MS = 500

CH_STR_CHIP_DETECT = b"\xa1\x12\x00\x52\x11MCU ISP & WCH.CN"
CH_STR_CONFIG_READ = bytes((0xA7, 0x02, 0x00, 0x1F, 0x00))
CH_STR_REBOOT = bytes((0xA2, 0x01, 0x00, 0x01))

# Config write packet from the CH59x recovery gist.
# The final 12 bytes are:
#   reserved[4] = FF FF FF FF
#   wprotect[4] = FF FF FF FF
#   user_cfg[4] = D5 0F FF 4F -> USER_CFG 0x4FFF0FD5
# This is the development config previously observed working on this board:
#   CFG_DEBUG_EN = Enable
#   CFG_ROM_READ = Read enable
#   CFG_RESET_EN = Disable
CH_STR_CONFIG_WRITE_DEV = (
    b"\xA8\x0E\x00\x07\x00"
    + bytes((0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xD5, 0x0F, 0xFF, 0x4F))
)


def load_usb():
    try:
        import usb.core  # type: ignore
        import usb.util  # type: ignore
        import usb.backend.libusb1  # type: ignore
    except ImportError as exc:
        raise SystemExit(
            "pyusb is required. Try: python -m pip install pyusb libusb-package"
        ) from exc

    backend = usb.backend.libusb1.get_backend()
    if backend is None:
        try:
            import libusb_package  # type: ignore

            backend = libusb_package.get_libusb1_backend()
        except ImportError:
            backend = None

    if backend is None:
        raise SystemExit(
            "No libusb backend found. Try: python -m pip install libusb-package"
        )

    return usb.core, usb.util, backend


def format_bytes(data: Iterable[int]) -> str:
    return " ".join(f"{int(b) & 0xFF:02X}" for b in data)


def find_device(usb_core, backend):
    for vid in CH_USB_VENDOR_IDS:
        dev = usb_core.find(idVendor=vid, idProduct=CH_USB_PRODUCT_ID, backend=backend)
        if dev is not None:
            return dev, vid
    return None, None


def xfer(dev, packet: bytes, label: str, timeout_ms: int) -> bytes:
    dev.write(CH_USB_EP_OUT, packet, timeout=timeout_ms)
    response = bytes(dev.read(CH_USB_EP_IN, CH_USB_PACKET_SIZE, timeout_ms))
    print(f"{label}: {format_bytes(response)}")
    return response


def attempt(write_dev_config: bool, reboot: bool, timeout_ms: int) -> bool:
    usb_core, usb_util, backend = load_usb()
    dev, vid = find_device(usb_core, backend)
    if dev is None:
        return False

    print(f"Found CH59x USB ISP device VID:PID {vid:04X}:{CH_USB_PRODUCT_ID:04X}")

    try:
        dev.set_configuration()
    except Exception as exc:  # noqa: BLE001 - pyusb raises backend-specific errors
        print(f"set_configuration failed: {exc}")

    cfg = dev.get_active_configuration()
    intf = cfg[(0, 0)]
    usb_util.claim_interface(dev, intf)

    try:
        xfer(dev, CH_STR_CHIP_DETECT, "chip_detect", timeout_ms)
        xfer(dev, CH_STR_CONFIG_READ, "config_read", timeout_ms)

        if write_dev_config:
            xfer(dev, CH_STR_CONFIG_WRITE_DEV, "config_write_dev_0x4FFF0FD5", timeout_ms)

        if reboot:
            xfer(dev, CH_STR_REBOOT, "reboot", timeout_ms)
    finally:
        usb_util.release_interface(dev, intf)

    return True


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--loop", action="store_true", help="Poll until a bootloader device is caught")
    parser.add_argument("--interval", type=float, default=0.05, help="Polling interval in seconds")
    parser.add_argument("--timeout-ms", type=int, default=CH_USB_TIMEOUT_MS, help="USB transfer timeout")
    parser.add_argument("--max-seconds", type=float, default=60.0, help="Maximum loop time")
    parser.add_argument(
        "--write-dev-config",
        action="store_true",
        help="Write USER_CFG 0x4FFF0FD5 development config",
    )
    parser.add_argument("--reboot", action="store_true", help="Send reboot command after operations")
    parser.add_argument(
        "--confirm",
        default="",
        help="Must be WRITE_CONFIG when --write-dev-config is used",
    )
    args = parser.parse_args()

    if args.write_dev_config and args.confirm != "WRITE_CONFIG":
        print("Refusing to write config without --confirm WRITE_CONFIG", file=sys.stderr)
        return 2

    deadline = time.monotonic() + args.max_seconds
    while True:
        try:
            if attempt(args.write_dev_config, args.reboot, args.timeout_ms):
                return 0
        except Exception as exc:  # noqa: BLE001 - recovery tool should keep trying
            print(f"attempt failed: {exc}")

        if not args.loop or time.monotonic() >= deadline:
            print("CH59x USB ISP device not caught")
            return 1

        time.sleep(args.interval)


if __name__ == "__main__":
    raise SystemExit(main())
