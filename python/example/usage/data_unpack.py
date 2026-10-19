#!/usr/bin/env python3
"""
Example: data unpack (data_unpack)

- Request topic : lixel/application/request/data_unpack  (DataUnpackRequest)
- Response topic : lixel/application/response/data_unpack (DataUnpackResponse)
- Report topic : lixel/application/report/data_unpack   (DataUnpackReport)

Initiates unpacking of a project's xbin data and subscribes to progress reports
until completion or timeout.

Note: the data processing (unpack) feature is only supported by some devices.
"""
import time

from _common import build_arg_parser, dump, make_client


def main() -> int:
    parser = build_arg_parser("data unpack")
    parser.add_argument("--project", required=True, help="project name")
    parser.add_argument("--watch", type=float, default=30.0, help="seconds to wait for progress reports")
    args = parser.parse_args()

    done = {"flag": False}

    def on_progress(topic, data):
        dump("DataUnpackReport", data)
        if data.get("progress") == 100:
            done["flag"] = True

    with make_client(args) as client:
        client.subscribe(
            "lixel/application/report/data_unpack", "DataUnpackReport", on_progress)

        rsp = client.request(
            req_topic="lixel/application/request/data_unpack",
            req_message="DataUnpackRequest",
            payload={
                "project_name": args.project,
                "data_source": "XBIN_DATA",
                # "data_topics": ["/lidar/points"],  # if not specified, unpack all
            },
            rsp_topic="lixel/application/response/data_unpack",
            rsp_message="DataUnpackResponse",
        )
        dump("DataUnpackResponse", rsp)

        # Wait for progress reports
        deadline = time.time() + args.watch
        while not done["flag"] and time.time() < deadline:
            time.sleep(0.5)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
