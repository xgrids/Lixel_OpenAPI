#!/usr/bin/env python3
"""
Example: subscribe to device status reports (device_status)

- Report topic : lixel/application/report/device_status  (DeviceStatusReport)

The device reports mapping status, system resources, RTK status, etc. at 1Hz.
"""
import time

from _common import build_arg_parser, dump, make_client


def main() -> int:
    parser = build_arg_parser("subscribe to device status")
    parser.add_argument("--seconds", type=float, default=10.0, help="subscription duration")
    args = parser.parse_args()

    with make_client(args) as client:
        client.subscribe(
            "lixel/application/report/device_status", "DeviceStatusReport",
            lambda topic, data: dump("DeviceStatusReport", data),
        )
        print(f"Subscribing to device status for {args.seconds}s ...")
        time.sleep(args.seconds)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
