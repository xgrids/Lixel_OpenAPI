#!/usr/bin/env python3
"""
Example: subscribe to system error reports (system_error)

- Report topic : lixel/application/report/system_error  (SystemErrorReport)

The device proactively reports system error codes and descriptions.
"""
import time

from _common import build_arg_parser, dump, make_client


def main() -> int:
    parser = build_arg_parser("subscribe to system errors")
    parser.add_argument("--seconds", type=float, default=15.0, help="subscription duration")
    args = parser.parse_args()

    with make_client(args) as client:
        client.subscribe(
            "lixel/application/report/system_error", "SystemErrorReport",
            lambda topic, data: dump("SystemErrorReport", data),
        )
        print(f"Subscribing to system errors for {args.seconds}s ...")
        time.sleep(args.seconds)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
