#!/usr/bin/env python3
"""
Example: get Ntrip profile (get_ntrip_profile)

- Request topic : lixel/application/request/get_ntrip_profile  (GetNtripProfileRequest)
- Response topic : lixel/application/response/get_ntrip_profile (GetNtripProfileResponse)
"""
from _common import build_arg_parser, dump, make_client


def main() -> int:
    args = build_arg_parser("get Ntrip profile").parse_args()
    with make_client(args) as client:
        rsp = client.request(
            req_topic="lixel/application/request/get_ntrip_profile",
            req_message="GetNtripProfileRequest",
            payload={},
            rsp_topic="lixel/application/response/get_ntrip_profile",
            rsp_message="GetNtripProfileResponse",
        )
        dump("GetNtripProfileResponse", rsp)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
