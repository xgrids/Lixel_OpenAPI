#!/usr/bin/env python3
"""
Example: scan control (modeling)

- Request topic : lixel/application/request/modeling   (ModelingRequest)
- Response topic : lixel/application/response/modeling  (ModelingResponse)
- Report topic : lixel/application/report/modeling    (ModelingReport)

Demonstrates the complete flow of "start scanning -> subscribe to scan status for
a few seconds -> stop scanning". action supports AC_START / AC_STOP.
"""
import time

from _common import build_arg_parser, dump, make_client


def main() -> int:
    parser = build_arg_parser("scan control")
    parser.add_argument("--project", default="demo_project", help="project name")
    parser.add_argument("--watch", type=float, default=5.0, help="seconds to subscribe to reports")
    args = parser.parse_args()

    with make_client(args) as client:
        # Subscribe to scan status reports
        client.subscribe(
            "lixel/application/report/modeling", "ModelingReport",
            lambda topic, data: dump("ModelingReport", data),
        )

        # Start scanning
        start_rsp = client.request(
            req_topic="lixel/application/request/modeling",
            req_message="ModelingRequest",
            payload={
                "action": "AC_START",
                "project_name": args.project,
                "record_mode": "RM_RECORD_CALC",
                "scan_mode": "SM_PCL",
                "mount_type": "MT_HAND",
            },
            rsp_topic="lixel/application/response/modeling",
            rsp_message="ModelingResponse",
        )
        dump("ModelingResponse (start)", start_rsp)

        # Observe scan status reports for a while
        time.sleep(args.watch)

        # Stop scanning
        stop_rsp = client.request(
            req_topic="lixel/application/request/modeling",
            req_message="ModelingRequest",
            payload={"action": "AC_STOP"},
            rsp_topic="lixel/application/response/modeling",
            rsp_message="ModelingResponse",
        )
        dump("ModelingResponse (stop)", stop_rsp)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
