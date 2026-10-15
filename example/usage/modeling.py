"""
Example: scan control (modeling)

- Request topic : lixel/application/request/modeling   (ModelingRequest)
- Response topic : lixel/application/response/modeling  (ModelingResponse)
- Report topic : lixel/application/report/modeling    (ModelingReport)
- Report topic : lixel/application/report/device_status (DeviceStatusReport, 1 Hz)

Demonstrates the complete flow of "start scanning -> wait until the device is
really scanning -> watch scan reports -> stop scanning -> wait until standby".
action supports AC_START / AC_STOP.

Starting and stopping are asynchronous: the response to AC_START only means the
request was accepted. The device then goes standby -> starting -> scanning, which
takes several seconds, and a stop request sent before it is scanning is deferred
until the start sequence has finished. Watch DeviceStatusReport.modeling_state
instead of sleeping for a fixed time.
"""
import sys
import threading
import time

from _common import build_arg_parser, dump, make_client

# DeviceStatusReport.modeling_state values (see the comment of DeviceStatusReport
# in proto/openapi/aiot_msgs.proto and section "Error codes" of the protocol reference).
# Base 0x12040000: EC_ScanReady = base + 300, and so on.
_BASE = 0x12040000
EC_SCAN_READY = _BASE + 300      # standby
EC_SCAN_STARTING = _BASE + 301   # starting scan
EC_SCAN_MODELING = _BASE + 302   # scanning
EC_SCAN_STOPPING = _BASE + 303   # stopping scan

_NAMES = {
    EC_SCAN_READY: "standby",
    EC_SCAN_STARTING: "starting",
    EC_SCAN_MODELING: "scanning",
    EC_SCAN_STOPPING: "stopping",
}


class _StateWatcher:
    """Keeps the latest DeviceStatusReport.modeling_state."""

    def __init__(self):
        self._state = None
        self._cond = threading.Condition()

    def on_status(self, topic, data):
        state = data.get("modeling_state")
        with self._cond:
            if state != self._state:
                print(f"  [state] {_NAMES.get(state, hex(state) if isinstance(state, int) else state)}")
            self._state = state
            self._cond.notify_all()

    def wait_for(self, target: int, timeout: float) -> bool:
        deadline = time.monotonic() + timeout
        with self._cond:
            while self._state != target:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    return False
                self._cond.wait(remaining)
        return True

    @property
    def state(self):
        with self._cond:
            return self._state


def _modeling(client, payload):
    return client.request(
        req_topic="lixel/application/request/modeling",
        req_message="ModelingRequest",
        payload=payload,
        rsp_topic="lixel/application/response/modeling",
        rsp_message="ModelingResponse",
    )


def main() -> int:
    parser = build_arg_parser("scan control")
    parser.add_argument("--project", default="demo_project", help="project name")
    parser.add_argument("--watch", type=float, default=5.0,
                        help="seconds to watch scan reports once the device is scanning")
    parser.add_argument("--start-timeout", type=float, default=90.0,
                        help="max seconds to wait for the device to start scanning")
    parser.add_argument("--stop-timeout", type=float, default=180.0,
                        help="max seconds to wait for the device to return to standby")
    args = parser.parse_args()

    watcher = _StateWatcher()
    with make_client(args) as client:
        client.subscribe("lixel/application/report/device_status", "DeviceStatusReport",
                         watcher.on_status)
        # Subscribe to scan status reports
        client.subscribe(
            "lixel/application/report/modeling", "ModelingReport",
            lambda topic, data: dump("ModelingReport", data),
        )

        # The device must be in standby before a scan can be started
        if not watcher.wait_for(EC_SCAN_READY, 10.0):
            state = watcher.state
            print(f"Device is not in standby (modeling_state={state}); aborting.", file=sys.stderr)
            return 2

        # Start scanning
        start_rsp = _modeling(client, {
            "action": "AC_START",
            "project_name": args.project,
            "record_mode": "RM_RECORD_CALC",
            "scan_mode": "SM_PCL",
            "mount_type": "MT_HAND",
        })
        dump("ModelingResponse (start)", start_rsp)

        # Wait until the device is really scanning
        print(f"Waiting for the device to start scanning (up to {args.start_timeout:.0f}s) ...")
        started = watcher.wait_for(EC_SCAN_MODELING, args.start_timeout)
        if started:
            # Observe scan status reports for a while
            time.sleep(args.watch)
        else:
            print("Timed out waiting for the device to start scanning; stopping.", file=sys.stderr)

        # Stop scanning
        stop_rsp = _modeling(client, {"action": "AC_STOP"})
        dump("ModelingResponse (stop)", stop_rsp)

        print(f"Waiting for the device to return to standby (up to {args.stop_timeout:.0f}s) ...")
        stopped = watcher.wait_for(EC_SCAN_READY, args.stop_timeout)
        if not stopped:
            print("Timed out waiting for the device to return to standby.", file=sys.stderr)
        return 0 if (started and stopped) else 1


if __name__ == "__main__":
    raise SystemExit(main())
