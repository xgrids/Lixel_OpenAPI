#!/usr/bin/env python3
"""A fake OpenAPI device for the end-to-end test of the C++ examples (run_examples.py).

It is an independent implementation on purpose: the Python client side of this repository (the
ProtoCodec and paho-mqtt) plays the device, the C++ examples play the user. So a mistake that the
C++ code makes on both ends of a conversation cannot cancel itself out.

What it does
  - answers every request on lixel/application/request/<name> on the matching response topic,
    echoing the request header, and appends the decoded request to --log (one JSON per line)
  - decides each request in the order of the protocol reference, section 2.2: a header.openapi_key
    equal to --key is always served; any other key counts as no key, and then
      with --license active    the request is served
      with --license expired   it is answered with error 0x32040001 "openapi license exhausted"
      with --license none      it is answered with error 0x32040001 "openapi auth error"
      with --key only          (a device on the earlier key-based scheme) the same error as none
    --key and --license can be given together; one of them is required. Metering is not modelled.
  - reports device_status (modeling_state), heartbeat (JSON text), lio_pose, system_error and
    lio_pcl (LZ4 block in a MqttCompressMsg, 100 points) every 0.2 s; with --license the
    heartbeat carries Features, OpenapiLicenseState and OpenapiLicenseText, with --key only it
    does not
  - runs the scan state machine: AC_START -> starting -> scanning, AC_STOP -> stopping -> standby
  - publishes DataUnpackReport progress 50 and 100 after an accepted data_unpack request
It is not a model of a real device: timings are short and lio_pcl/lio_pose are sent all the time.

Scenarios (--scenario)
  normal         everything works
  busy           the device is scanning already and stays so
  reject-start   AC_START is answered with an error code
  silent-start   AC_START is not answered
  silent-stop    AC_STOP is not answered
Run with the repository's python/ directory as the working directory and on PYTHONPATH, and with
paho-mqtt, protobuf and lz4 installed.
"""
import argparse
import base64
import json
import os
import sys
import threading
import time

import lz4.block
import paho.mqtt.client as mqtt

from example.core import ProtoCodec

SUCCESS = 0x12040001
AUTH_ERROR = 0x32040001
REJECTED = 0x32040002
READY, STARTING, SCANNING, STOPPING = 0x1204012C, 0x1204012D, 0x1204012E, 0x1204012F
LICENSE_TEXT = {"none": "Not authorized", "active": "Authorized", "expired": "Expired"}
REQ = "lixel/application/request/"
RSP = "lixel/application/response/"
RPT = "lixel/application/report/"


def camel(name):
    return "".join(part.capitalize() for part in name.split("_"))


class FakeDevice:
    def __init__(self, args):
        self.args = args
        self.codec = ProtoCodec()
        self.state = SCANNING if args.scenario == "busy" else READY
        self.lock = threading.Lock()
        self.log_lock = threading.Lock()
        self.stop = threading.Event()
        self.client = mqtt.Client(
            callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
            client_id=f"fake-device-{os.getpid()}",
            protocol=mqtt.MQTTv311,
        )
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        self.client.on_subscribe = self.on_subscribe

    # ---- MQTT ----
    def on_connect(self, client, userdata, flags, reason_code, properties):
        client.subscribe(REQ + "#", qos=0)

    def on_subscribe(self, client, userdata, mid, reason_codes, properties):
        with open(self.args.ready_file, "w") as f:
            f.write("ready\n")

    def on_message(self, client, userdata, msg):
        if not msg.topic.startswith(REQ):
            return
        try:
            self.handle(msg.topic[len(REQ):], msg.payload)
        except Exception as exc:  # report it in the log; the test fails on the missing answer
            self.log({"error": f"{type(exc).__name__}: {exc}", "topic": msg.topic})

    def send(self, topic, message, data):
        payload = self.codec.encode(message, data) if message else data
        self.client.publish(topic, payload, qos=0)

    def log(self, entry):
        with self.log_lock:
            with open(self.args.log, "a") as f:
                f.write(json.dumps(entry) + "\n")

    # ---- requests ----
    def handle(self, name, payload):
        request_message = camel(name) + "Request"
        response_message = camel(name) + "Response"
        if self.codec.get_class(request_message) is None:
            self.log({"error": "unknown request", "topic": name})
            return
        req = self.codec.decode(request_message, payload)
        self.log({"topic": name, "request": req})
        header = req.get("header", {})
        rsp = {"header": header, "error": {"code": SUCCESS, "desc": "success"}}
        refusal = self.refusal(header.get("openapi_key"))
        if refusal is not None:
            rsp["error"] = {"code": AUTH_ERROR, "desc": refusal}
            return self.send(RSP + name, response_message, rsp)
        if name == "device_info":
            rsp["device_info"] = {"base_info": {"device_sn": "FAKE-0001"}}
        elif name == "modeling":
            if not self.handle_modeling(req, rsp):
                return
        elif name == "data_unpack":
            threading.Timer(0.2, self.unpack_report, args=(50,)).start()
            threading.Timer(0.5, self.unpack_report, args=(100,)).start()
        self.send(RSP + name, response_message, rsp)

    def refusal(self, key):
        """Returns the error text when the request is refused, None when it is served."""
        if self.args.key is not None and key == self.args.key:
            return None
        if self.args.license == "active":
            return None
        if self.args.license == "expired":
            return "openapi license exhausted"
        return "openapi auth error"

    def handle_modeling(self, req, rsp):
        """Updates the state machine. Returns False when the request is not to be answered."""
        action = req.get("action")
        rsp["action"] = action
        scenario = self.args.scenario
        if action == "AC_START":
            if scenario == "silent-start":
                return False
            if scenario == "reject-start":
                rsp["error"] = {"code": REJECTED, "desc": "refused"}
            elif scenario != "busy":
                self.set_state(STARTING)
                threading.Timer(0.3, self.set_state, args=(SCANNING,)).start()
        elif action == "AC_STOP":
            if scenario == "silent-stop":
                return False
            self.set_state(STOPPING)
            threading.Timer(0.3, self.set_state, args=(READY,)).start()
        return True

    def set_state(self, state):
        with self.lock:
            self.state = state
        self.log({"state": state})

    def unpack_report(self, progress):
        self.send(RPT + "data_unpack", "DataUnpackReport",
                  {"progress": progress, "error": {"code": SUCCESS, "desc": "success"}})

    # ---- reports ----
    def pcl_message(self):
        points = [{"x": i * 10, "y": -i * 10, "z": 1000 + i, "rgbi": 0x10203040 + i}
                  for i in range(100)]
        report = self.codec.encode("LioPclReport",
                                   {"header": {"stamp": 123456, "scaler": 1000}, "points": points})
        block = lz4.block.compress(report, store_size=False)
        return self.codec.encode("MqttCompressMsg", {
            "compression": "CompressionLz4",
            "compressed_size": len(report),
            "compressed_data": base64.b64encode(block).decode(),
        })

    def report_loop(self):
        count = 0
        while not self.stop.wait(0.2):
            count += 1
            with self.lock:
                state = self.state
            self.send(RPT + "device_status", "DeviceStatusReport",
                      {"modeling_state": state, "device_sn": "FAKE-0001"})
            heartbeat = {"DeviceSN": "FAKE-0001", "Timestamp": time.time(),
                         "ProtocolVersion": "3.0.0"}
            if self.args.license is not None:
                heartbeat["Features"] = ["openapi_license"]
                heartbeat["OpenapiLicenseState"] = self.args.license
                heartbeat["OpenapiLicenseText"] = LICENSE_TEXT[self.args.license]
            self.send(RPT + "heartbeat", "", json.dumps(heartbeat).encode())
            self.send(RPT + "lio_pose", "LioPoseReport", {"distance": float(count)})
            self.send(RPT + "system_error", "SystemErrorReport",
                      {"error": {"code": 0x32010001, "desc": "fake error"}})
            self.client.publish(RPT + "lio_pcl", self.pcl_message(), qos=0)
            if state == SCANNING:
                self.send(RPT + "modeling", "ModelingReport",
                          {"pgo_progress": 0, "scan_status": {"scan_time": count}})

    def run(self):
        self.client.connect(self.args.host, self.args.port, keepalive=30)
        self.client.loop_start()
        threading.Thread(target=self.report_loop, daemon=True).start()
        try:
            while True:
                time.sleep(0.5)
        except KeyboardInterrupt:
            pass
        finally:
            self.stop.set()
            self.client.loop_stop()
            self.client.disconnect()


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--port", type=int, required=True)
    p.add_argument("--key", help="the one valid OpenAPI key; given alone, behave like a device "
                                 "on the earlier key-based scheme")
    p.add_argument("--license", choices=["none", "active", "expired"],
                   help="behave like a device with this OpenAPI authorization state")
    p.add_argument("--log", required=True)
    p.add_argument("--ready-file", required=True)
    p.add_argument("--scenario", default="normal",
                   choices=["normal", "busy", "reject-start", "silent-start", "silent-stop"])
    args = p.parse_args()
    if args.key is None and args.license is None:
        p.error("give --key, --license, or both")
    FakeDevice(args).run()


if __name__ == "__main__":
    sys.exit(main())
