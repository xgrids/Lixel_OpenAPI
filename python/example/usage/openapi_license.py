#!/usr/bin/env python3
"""
Example: query the OpenAPI authorization (openapi_license)
- Request topic : lixel/application/request/openapi_license  (OpenapiLicenseRequest)
- Response topic : lixel/application/response/openapi_license (OpenapiLicenseResponse)
Only AC_GET_CONFIG is sent. No OpenAPI key is needed. The response carries the
state (none / active / expired) and the remaining minutes (-1 means permanent).
Only devices whose heartbeat Features contains "openapi_license" reply.
"""
from _common import build_arg_parser, dump, make_client


def main() -> int:
    args = build_arg_parser("query the OpenAPI authorization").parse_args()
    with make_client(args) as client:
        rsp = client.request(
            req_topic="lixel/application/request/openapi_license",
            req_message="OpenapiLicenseRequest",
            payload={"action": "AC_GET_CONFIG"},
            rsp_topic="lixel/application/response/openapi_license",
            rsp_message="OpenapiLicenseResponse",
        )
        dump("OpenapiLicenseResponse", rsp)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
