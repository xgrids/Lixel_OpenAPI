#!/usr/bin/env python3
"""
Example: set Ntrip profile (set_ntrip_profile)

- Request topic : lixel/application/request/set_ntrip_profile  (SetNtripProfileRequest)
- Response topic : lixel/application/response/set_ntrip_profile (SetNtripProfileResponse)

action supports AC_LOGIN / AC_LOGOUT. Demonstrates logging in to a custom CORS.

Note: setting is only allowed when the RTK status is HwConnected. When the
device WiFi is in AP mode (Ntrip data is forwarded over WebSocket), the Response
returns error code AppCode::EMBError::Unknown; this error can be ignored and the
device Ntrip connection is not affected.
"""
from _common import build_arg_parser, dump, make_client


def main() -> int:
    parser = build_arg_parser("set Ntrip profile")
    parser.add_argument("--server", default="203.107.45.154", help="CORS server address")
    parser.add_argument("--port2", type=int, default=8002, help="CORS port")
    parser.add_argument("--mount", default="RTCM33_GRCEJ", help="mount point")
    parser.add_argument("--ntrip-user", default="user", help="CORS username")
    parser.add_argument("--ntrip-pass", default="pass", help="CORS password")
    args = parser.parse_args()

    with make_client(args) as client:
        rsp = client.request(
            req_topic="lixel/application/request/set_ntrip_profile",
            req_message="SetNtripProfileRequest",
            payload={
                "action": "AC_LOGIN",
                "cors": "CUSTOM",       # CUSTOM / QIANXUN / ZHONGYI
                "frame": "CGCS2000",    # UNKNOWN/ITRF2008/WGS84/CGCS2000
                "server": args.server,
                "port": args.port2,
                "mount_point": args.mount,
                "username": args.ntrip_user,
                "password": args.ntrip_pass,
            },
            rsp_topic="lixel/application/response/set_ntrip_profile",
            rsp_message="SetNtripProfileResponse",
        )
        dump("SetNtripProfileResponse", rsp)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
