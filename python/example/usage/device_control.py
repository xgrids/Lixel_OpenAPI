#!/usr/bin/env python3
"""
Example: device control (device_control)

- Request topic : lixel/application/request/device_control  (DeviceControlRequest)
- Response topic : lixel/application/response/device_control (DeviceControlResponse)

Supported actions: AC_USB_MASS / AC_USB_ETHER / AC_DISK_FORMAT /
AC_POWER_OFF / AC_SYSTEM_REBOOT.

Note: AC_POWER_OFF, AC_SYSTEM_REBOOT and AC_DISK_FORMAT are high-risk operations;
run them carefully on real devices. The relatively safe AC_USB_ETHER is used by
default for demonstration.
"""
from _common import build_arg_parser, dump, make_client


def main() -> int:
    parser = build_arg_parser("device control")
    parser.add_argument(
        "--action", default="AC_USB_ETHER",
        help="control action, e.g. AC_USB_MASS/AC_USB_ETHER/AC_DISK_FORMAT/"
             "AC_POWER_OFF/AC_SYSTEM_REBOOT",
    )
    args = parser.parse_args()

    with make_client(args) as client:
        rsp = client.request(
            req_topic="lixel/application/request/device_control",
            req_message="DeviceControlRequest",
            payload={"action": args.action},
            rsp_topic="lixel/application/response/device_control",
            rsp_message="DeviceControlResponse",
        )
        dump("DeviceControlResponse", rsp)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
