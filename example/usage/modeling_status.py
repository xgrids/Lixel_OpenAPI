#!/usr/bin/env python3
"""
Example: query scan status (modeling_status)

- Request topic : lixel/application/request/modeling_status  (ModelingStatusRequest)
- Response topic : lixel/application/response/modeling_status (ModelingStatusResponse)

Query the output URLs (pose, control points, measure points) of the current/most
recent project.
"""
from _common import build_arg_parser, dump, make_client


def main() -> int:
    args = build_arg_parser("query scan status").parse_args()
    with make_client(args) as client:
        rsp = client.request(
            req_topic="lixel/application/request/modeling_status",
            req_message="ModelingStatusRequest",
            payload={},
            rsp_topic="lixel/application/response/modeling_status",
            rsp_message="ModelingStatusResponse",
        )
        dump("ModelingStatusResponse", rsp)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
