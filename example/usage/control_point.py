#!/usr/bin/env python3
"""
Example: control point operations (control_point)

- Request topic : lixel/application/request/control_point  (ControlPointRequest)
- Response topic : lixel/application/response/control_point (ControlPointResponse)
- Report topic : lixel/application/report/control_point   (ControlPointReport)

action only supports AC_ADD / AC_REMOVE / AC_UPDATE.
Demonstrates adding a control point.
"""
from _common import build_arg_parser, dump, make_client


def main() -> int:
    parser = build_arg_parser("control point operations")
    parser.add_argument("--action", default="AC_ADD",
                        help="AC_ADD / AC_REMOVE / AC_UPDATE")
    parser.add_argument("--id", type=int, default=1, help="control point ID")
    parser.add_argument("--label", default="CP-1", help="control point label")
    parser.add_argument("--height", type=int, default=1800, help="base point height (mm)")
    args = parser.parse_args()

    with make_client(args) as client:
        rsp = client.request(
            req_topic="lixel/application/request/control_point",
            req_message="ControlPointRequest",
            payload={
                "action": args.action,
                "id": args.id,
                "type": 0,  # 0 device 1 centering pole
                "label": args.label,
                "height": args.height,
            },
            rsp_topic="lixel/application/response/control_point",
            rsp_message="ControlPointResponse",
        )
        dump("ControlPointResponse", rsp)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
