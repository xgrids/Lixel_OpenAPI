#!/usr/bin/env python3
"""
Example: get RTK advanced configuration (get_rtk_advance)

- Request topic : lixel/application/request/get_rtk_advance  (GetRtkAdvanceRequest)
- Response topic : lixel/application/response/get_rtk_advance (GetRtkAdvanceResponse)

Returns the RTK type and the enabled satellite systems (GPS/BDS/GLO/GAL/QZSS/IRNSS).
"""
from _common import build_arg_parser, dump, make_client


def main() -> int:
    args = build_arg_parser("get RTK advanced configuration").parse_args()
    with make_client(args) as client:
        rsp = client.request(
            req_topic="lixel/application/request/get_rtk_advance",
            req_message="GetRtkAdvanceRequest",
            payload={},
            rsp_topic="lixel/application/response/get_rtk_advance",
            rsp_message="GetRtkAdvanceResponse",
        )
        dump("GetRtkAdvanceResponse", rsp)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
