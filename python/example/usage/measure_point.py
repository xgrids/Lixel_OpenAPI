#!/usr/bin/env python3
"""
Example: measure point operations (measure_point)

- Request topic : lixel/application/request/measure_point  (MeasurePointRequest)
- Response topic : lixel/application/response/measure_point (MeasurePointResponse)

action only supports AC_ADD / AC_REMOVE. Demonstrates adding a measure point.

Note: the measure point feature is only supported by some devices.
"""
from _common import build_arg_parser, dump, make_client


def main() -> int:
    parser = build_arg_parser("measure point operations")
    parser.add_argument("--action", default="AC_ADD", help="AC_ADD / AC_REMOVE")
    parser.add_argument("--id", type=int, default=1, help="measure point ID")
    parser.add_argument("--label", default="MP-1", help="measure point label")
    args = parser.parse_args()

    with make_client(args) as client:
        rsp = client.request(
            req_topic="lixel/application/request/measure_point",
            req_message="MeasurePointRequest",
            payload={"action": args.action, "id": args.id, "label": args.label},
            rsp_topic="lixel/application/response/measure_point",
            rsp_message="MeasurePointResponse",
        )
        dump("MeasurePointResponse", rsp)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
