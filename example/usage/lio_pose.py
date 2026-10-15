#!/usr/bin/env python3
"""
Example: subscribe to real-time pose reports (lio_pose)

- Report topic : lixel/application/report/lio_pose  (LioPoseReport)

During scanning, reports the real-time LIO pose (position + attitude quaternion)
and the accumulated distance.
"""
import time

from _common import build_arg_parser, dump, make_client


def main() -> int:
    parser = build_arg_parser("subscribe to real-time pose")
    parser.add_argument("--seconds", type=float, default=10.0, help="subscription duration")
    args = parser.parse_args()

    with make_client(args) as client:
        client.subscribe(
            "lixel/application/report/lio_pose", "LioPoseReport",
            lambda topic, data: dump("LioPoseReport", data),
        )
        print(f"Subscribing to real-time pose for {args.seconds}s ... (device must be scanning)")
        time.sleep(args.seconds)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
