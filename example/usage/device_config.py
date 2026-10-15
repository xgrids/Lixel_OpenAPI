#!/usr/bin/env python3
"""
Example: device parameter configuration (device_config)

- Request topic : lixel/application/request/device_config  (DeviceConfigRequest)
- Response topic : lixel/application/response/device_config (DeviceConfigResponse)
- Report topic : lixel/application/report/device_config   (DeviceConfigRequest)

Demonstrates configuring the device time and time zone. Optional fields that are
not set are not sent.
"""
import time

from _common import build_arg_parser, dump, make_client


def main() -> int:
    parser = build_arg_parser("device parameter configuration")
    parser.add_argument("--tz", default="Asia/Shanghai", help="time zone")
    args = parser.parse_args()

    payload = {
        "time_config": {
            "time_secs": int(time.time()),
            "time_zone": args.tz,
        },
        # To switch the main storage, add:
        # "main_storage_config": {"storage_type": 0},  # 0 built-in 1 TF 2 USB
    }

    with make_client(args) as client:
        rsp = client.request(
            req_topic="lixel/application/request/device_config",
            req_message="DeviceConfigRequest",
            payload=payload,
            rsp_topic="lixel/application/response/device_config",
            rsp_message="DeviceConfigResponse",
        )
        dump("DeviceConfigResponse", rsp)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
