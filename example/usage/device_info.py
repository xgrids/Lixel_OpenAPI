#!/usr/bin/env python3
"""
Example: query device info (device_info)

- Request topic : lixel/application/request/device_info   (DeviceInfoRequest)
- Response topic : lixel/application/response/device_info  (DeviceInfoResponse)

Demonstrates the simplest "empty request -> return full device info" call.
"""
from _common import build_arg_parser, dump, make_client


def main() -> int:
    args = build_arg_parser("query device info").parse_args()
    with make_client(args) as client:
        rsp = client.request(
            req_topic="lixel/application/request/device_info",
            req_message="DeviceInfoRequest",
            payload={},  # only header is needed; the client auto-fills openapi_key
            rsp_topic="lixel/application/response/device_info",
            rsp_message="DeviceInfoResponse",
        )
        dump("DeviceInfoResponse", rsp)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
