#!/usr/bin/env python3
"""
Example: subscribe to device heartbeat (heartbeat)

- Report topic : lixel/application/report/heartbeat  (JSON text, not protobuf)

The heartbeat payload is a JSON string and can be used for device discovery. Example:
    {"DeviceSN":"A256A60000","Timestamp":1697703246.5,"ProtocolVersion":"3.0.0"}
"""
import time

from _common import build_arg_parser, dump, make_client


def main() -> int:
    parser = build_arg_parser("subscribe to device heartbeat")
    parser.add_argument("--seconds", type=float, default=15.0, help="subscription duration")
    args = parser.parse_args()

    with make_client(args) as client:
        # Passing an empty string for message means a JSON/text payload; the callback receives a string
        client.subscribe(
            "lixel/application/report/heartbeat", "",
            lambda topic, data: dump("heartbeat", data),
        )
        print(f"Subscribing to heartbeat for {args.seconds}s ... (Ctrl+C to exit)")
        time.sleep(args.seconds)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
