#!/usr/bin/env python3
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

Exit codes: 0 success; 1 timeout while starting or stopping; 2 the device was not
in standby; 3 the device rejected AC_START; 4 no response to AC_START or AC_STOP;
130 interrupted with Ctrl-C. Once AC_START has been sent, AC_STOP is always sent
before the script exits (except when AC_START itself got no response).
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
EC_SCAN_STOPPED = _BASE + 304    # scan finished
# EC_ScanNotAllowedInUsbMass = 0x22040000 + 318 (warning class): the device is in USB mass storage mode
EC_SCAN_NOT_ALLOWED_IN_USB_MASS = 0x22040000 + 318

# Error.code values that mean the request was accepted (EC_Success, EC_SuccessAndReboot)
_OK_CODES = (_BASE + 1, _BASE + 2)

_NAMES = {
    EC_SCAN_READY: "standby",
    EC_SCAN_STARTING: "starting",
    EC_SCAN_MODELING: "scanning",
    EC_SCAN_STOPPING: "stopping",
    EC_SCAN_STOPPED: "stopped",
    EC_SCAN_NOT_ALLOWED_IN_USB_MASS: "scan not allowed (USB mass storage mode)",
}


def _state_name(state) -> str:
    if isinstance(state, int):
        return _NAMES.get(state, hex(state))
    return str(state)


class _StateWatcher:
    """Keeps the latest DeviceStatusReport.modeling_state."""

    def __init__(self):
        self._state = None
        self._seq = 0  # number of reports received so far
        self._cond = threading.Condition()

    def on_status(self, topic, data):
        state = data.get("modeling_state")
        with self._cond:
            if state != self._state:
                print(f"  [state] {_state_name(state)}")
            self._state = state
            self._seq += 1
            self._cond.notify_all()

    def wait_for(self, target: int, timeout: float, fresh: bool = False) -> bool:
        """Wait until modeling_state == target.

        With fresh=True the state cached before this call is ignored: a report
        received after the call is required. Use it after sending a request whose
        effect may not be visible in the cached state yet (reports arrive at 1 Hz).
        """
        deadline = time.monotonic() + timeout
        with self._cond:
            mark = self._seq
            while self._state != target or (fresh and self._seq == mark):
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

        interrupted = False
        start_attempted = False
        started = False
        try:
            # The device must be in standby before a scan can be started
            if not watcher.wait_for(EC_SCAN_READY, 10.0):
                state = watcher.state
                print(f"Device is not in standby (modeling_state={_state_name(state)}); aborting.", file=sys.stderr)
                return 2

            # Start scanning. From here on the request may reach the device, so a
            # Ctrl-C must still be followed by AC_STOP.
            start_attempted = True
            try:
                start_rsp = _modeling(client, {
                    "action": "AC_START",
                    "project_name": args.project,
                    "record_mode": "RM_RECORD_CALC",
                    "scan_mode": "SM_PCL",
                    "mount_type": "MT_HAND",
                })
            except TimeoutError as exc:
                print(f"No response to AC_START ({exc}). The scan state is unknown; "
                      "check the device before retrying.", file=sys.stderr)
                return 4
            dump("ModelingResponse (start)", start_rsp)
            start_code = start_rsp.get("error", {}).get("code")
            if start_code not in _OK_CODES:
                print(f"The device rejected AC_START (error code {start_code}); aborting.", file=sys.stderr)
                return 3

            # Wait until the device is really scanning
            print(f"Waiting for the device to start scanning (up to {args.start_timeout:.0f}s) ...")
            started = watcher.wait_for(EC_SCAN_MODELING, args.start_timeout)
            if started:
                # Observe scan status reports for a while
                time.sleep(args.watch)
            else:
                print("Timed out waiting for the device to start scanning; stopping.", file=sys.stderr)
        except KeyboardInterrupt:
            interrupted = True
            print("Interrupted; stopping the scan.", file=sys.stderr)

        if not start_attempted:
            return 130 if interrupted else 1

        # Stop scanning. This also runs after Ctrl-C or a start timeout so that the
        # device is not left scanning.
        try:
            stop_rsp = _modeling(client, {"action": "AC_STOP"})
        except TimeoutError as exc:
            print(f"No response to AC_STOP ({exc}). The device may still be scanning; "
                  "check it and stop it manually.", file=sys.stderr)
            return 130 if interrupted else 4
        except KeyboardInterrupt:
            print("Interrupted again before AC_STOP was confirmed; the device may still be scanning.",
                  file=sys.stderr)
            return 130
        dump("ModelingResponse (stop)", stop_rsp)

        # fresh=True: the cached state may still be the one from before the stop
        print(f"Waiting for the device to return to standby (up to {args.stop_timeout:.0f}s) ...")
        try:
            stopped = watcher.wait_for(EC_SCAN_READY, args.stop_timeout, fresh=True)
        except KeyboardInterrupt:
            print("Interrupted while waiting for standby; AC_STOP has been sent.", file=sys.stderr)
            return 130
        if not stopped:
            print("Timed out waiting for the device to return to standby.", file=sys.stderr)
        if interrupted:
            return 130
        return 0 if (started and stopped) else 1


if __name__ == "__main__":
    raise SystemExit(main())
