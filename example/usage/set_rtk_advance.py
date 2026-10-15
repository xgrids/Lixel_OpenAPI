#!/usr/bin/env python3
"""
Example: set RTK advanced configuration (set_rtk_advance)

- Request topic : lixel/application/request/set_rtk_advance  (SetRtkAdvanceRequest)
- Response topic : lixel/application/response/set_rtk_advance (SetRtkAdvanceResponse)

Sets the RTK type and satellite systems. rtk_register_code / rtk_control_mode
apply to L2PS1 only.
"""
from _common import build_arg_parser, dump, make_client


def main() -> int:
    parser = build_arg_parser("set RTK advanced configuration")
    parser.add_argument("--rtk-type", default="RTK", help="RTK / PPK / DJI")
    parser.add_argument("--systems", nargs="*",
                        default=["GPS", "BDS", "GLO", "GAL"],
                        help="satellite system list")
    args = parser.parse_args()

    with make_client(args) as client:
        rsp = client.request(
            req_topic="lixel/application/request/set_rtk_advance",
            req_message="SetRtkAdvanceRequest",
            payload={
                "rtk_type": args.rtk_type,
                "satellite_system": args.systems,
            },
            rsp_topic="lixel/application/response/set_rtk_advance",
            rsp_message="SetRtkAdvanceResponse",
        )
        dump("SetRtkAdvanceResponse", rsp)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
