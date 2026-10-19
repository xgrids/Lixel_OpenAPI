#!/usr/bin/env python3
"""End-to-end test of the 18 C++ examples against a fake device (fake_device.py).

Needs a broker (start it with ../broker/run_with_broker.sh, which sets the LIXEL_TEST_* variables)
and a Python with paho-mqtt, protobuf and lz4; this script itself uses the standard library only,
and starts fake_device.py with the same interpreter.

    bash tests/broker/run_with_broker.sh <python> tests/e2e/run_examples.py \
        --examples-dir build/examples --python-dir ../python

What is checked: the exit status and the output of every example, and the requests that arrived at
the device, decoded by the Python side. A request field that the C++ side writes wrongly shows up
as a difference there.
"""
import argparse
import json
import os
import signal
import subprocess
import sys
import tempfile
import threading
import time
import unittest

KEY = "e2e-key"
EXAMPLES = [
    "control_point", "data_unpack", "device_config", "device_control", "device_info",
    "device_status", "get_ntrip_profile", "get_rtk_advance", "heartbeat", "lio_pcl", "lio_pose",
    "measure_point", "modeling", "modeling_status", "project_list", "set_ntrip_profile",
    "set_rtk_advance", "system_error",
]
CONFIG = {}


def env_int(name):
    value = os.environ.get(name)
    if not value:
        raise SystemExit(f"{name} is not set: run this script through run_with_broker.sh")
    return int(value)


class DeviceCase(unittest.TestCase):
    """Starts one fake device per test class; `scenario` selects its behaviour."""

    scenario = "normal"

    @classmethod
    def setUpClass(cls):
        cls.dir = tempfile.mkdtemp(prefix="lixel-e2e.")
        cls.log_path = os.path.join(cls.dir, "device.log")
        cls.ready_path = os.path.join(cls.dir, "ready")
        env = dict(os.environ, PYTHONPATH=CONFIG["python_dir"])
        here = os.path.dirname(os.path.abspath(__file__))
        cls.device = subprocess.Popen(
            [sys.executable, os.path.join(here, "fake_device.py"),
             "--port", str(env_int("LIXEL_TEST_PLAIN_PORT")), "--key", KEY, "--log", cls.log_path,
             "--ready-file", cls.ready_path, "--scenario", cls.scenario],
            cwd=CONFIG["python_dir"], env=env, stderr=subprocess.PIPE)
        deadline = time.time() + 20
        while not os.path.exists(cls.ready_path):
            if cls.device.poll() is not None:
                raise RuntimeError("the fake device exited: " + cls.device.stderr.read().decode())
            if time.time() > deadline:
                cls.device.kill()
                raise RuntimeError("the fake device did not become ready")
            time.sleep(0.1)

    @classmethod
    def tearDownClass(cls):
        cls.device.send_signal(signal.SIGINT)
        try:
            cls.device.wait(timeout=10)
        except subprocess.TimeoutExpired:
            cls.device.kill()
        cls.device.stderr.close()

    # ---- helpers ----
    def base_args(self, tls=False):
        port = env_int("LIXEL_TEST_TLS_PORT" if tls else "LIXEL_TEST_PLAIN_PORT")
        return ["--host", "127.0.0.1", "--port", str(port), "--key", KEY, "--timeout", "5"]

    def run_example(self, name, *args, base=True, timeout=60, tls=False):
        command = [os.path.join(CONFIG["examples_dir"], name)]
        if base:
            command += self.base_args(tls)
        command += list(args)
        done = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                              timeout=timeout)
        return done.returncode, done.stdout.decode(), done.stderr.decode()

    def log_entries(self):
        if not os.path.exists(self.log_path):
            return []
        with open(self.log_path) as f:
            return [json.loads(line) for line in f if line.strip()]

    def requests(self, name):
        return [e["request"] for e in self.log_entries()
                if e.get("topic") == name and "request" in e]

    def last_request(self, name):
        found = self.requests(name)
        self.assertTrue(found, f"the device never received a {name} request; "
                               f"log: {self.log_entries()}")
        return found[-1]

    def assertOk(self, result, expected=0):
        rc, out, err = result
        self.assertEqual(rc, expected, f"stdout:\n{out}\nstderr:\n{err}")
        return out, err


class NormalDevice(DeviceCase):
    scenario = "normal"

    # ---- the plain request/response examples ----
    def test_device_info_prints_the_response_and_stamps_the_header(self):
        out, _ = self.assertOk(self.run_example("device_info"))
        self.assertIn("===== DeviceInfoResponse =====", out)
        self.assertIn("FAKE-0001", out)
        header = self.last_request("device_info")["header"]
        self.assertEqual(header["openapi_key"], KEY)
        self.assertNotEqual(int(header["seq"]), 0)

    def test_a_wrong_key_is_a_device_error_with_exit_status_3(self):
        rc, out, err = self.run_example("device_info", "--key", "wrong")
        self.assertEqual(rc, 3, err)
        self.assertIn("===== DeviceInfoResponse =====", out)
        self.assertIn("openapi auth error", err)
        self.assertIn("0x32040001", err)

    def test_no_key_at_all_is_refused_too(self):
        rc, _, err = self.run_example("device_info", "--key", "")
        self.assertEqual(rc, 3, err)

    def test_the_empty_request_examples(self):
        for name in ("get_ntrip_profile", "get_rtk_advance", "modeling_status"):
            with self.subTest(name):
                out, _ = self.assertOk(self.run_example(name))
                self.assertIn("Response =====", out)
                self.last_request(name)

    def test_device_control_default_and_explicit_action(self):
        self.assertOk(self.run_example("device_control"))
        self.assertEqual(self.last_request("device_control")["action"], "AC_USB_ETHER")
        self.assertOk(self.run_example("device_control", "--action", "AC_USB_MASS"))
        self.assertEqual(self.last_request("device_control")["action"], "AC_USB_MASS")

    def test_an_unknown_action_is_refused_before_anything_is_sent(self):
        before = len(self.requests("device_control"))
        rc, _, err = self.run_example("device_control", "--action", "AC_TYPO")
        self.assertEqual(rc, 2)
        self.assertIn("AC_TYPO", err)
        self.assertEqual(len(self.requests("device_control")), before)

    def test_control_point_default_values(self):
        self.assertOk(self.run_example("control_point"))
        req = self.last_request("control_point")
        self.assertEqual((req["action"], req["id"], req["type"], req["label"], req["height"]),
                         ("AC_ADD", 1, 0, "CP-1", 1800))

    def test_control_point_explicit_values(self):
        self.assertOk(self.run_example("control_point", "--action", "AC_REMOVE", "--id", "7",
                                       "--label", "Q-9", "--height", "2000"))
        req = self.last_request("control_point")
        self.assertEqual((req["action"], req["id"], req["label"], req["height"]),
                         ("AC_REMOVE", 7, "Q-9", 2000))

    def test_measure_point(self):
        self.assertOk(self.run_example("measure_point", "--id", "4", "--label", "M-4"))
        req = self.last_request("measure_point")
        self.assertEqual((req["action"], req["id"], req["label"]), ("AC_ADD", 4, "M-4"))

    def test_project_list(self):
        self.assertOk(self.run_example("project_list"))
        self.assertEqual(self.last_request("project_list")["action"], "AC_LIST_ALL_PROJECTS")

    def test_device_config_sends_the_time_and_the_zone(self):
        before = int(time.time())
        self.assertOk(self.run_example("device_config", "--tz", "Asia/Tokyo"))
        cfg = self.last_request("device_config")["time_config"]
        self.assertEqual(cfg["time_zone"], "Asia/Tokyo")
        self.assertTrue(before - 5 <= int(cfg["time_secs"]) <= int(time.time()) + 5, cfg)

    def test_set_ntrip_profile_keeps_quotes_and_backslashes_in_values(self):
        password = 'p"a\\ss \u00fc'
        self.assertOk(self.run_example(
            "set_ntrip_profile", "--server", "10.0.0.5", "--port2", "2101", "--mount", "MP/1",
            "--ntrip-user", "u'ser", "--ntrip-pass", password))
        req = self.last_request("set_ntrip_profile")
        self.assertEqual(req["action"], "AC_LOGIN")
        self.assertEqual((req["cors"], req["frame"]), ("CUSTOM", "CGCS2000"))
        self.assertEqual((req["server"], req["port"], req["mount_point"]),
                         ("10.0.0.5", 2101, "MP/1"))
        self.assertEqual((req["username"], req["password"]), ("u'ser", password))

    def test_set_rtk_advance_type_and_systems(self):
        self.assertOk(self.run_example("set_rtk_advance", "--rtk-type", "PPK", "--systems",
                                       "GPS", "BDS"))
        req = self.last_request("set_rtk_advance")
        self.assertEqual(req["rtk_type"], "PPK")
        self.assertEqual(req["satellite_system"], ["GPS", "BDS"])

    def test_set_rtk_advance_defaults(self):
        self.assertOk(self.run_example("set_rtk_advance"))
        req = self.last_request("set_rtk_advance")
        self.assertEqual((req["rtk_type"], req["satellite_system"]),
                         ("RTK", ["GPS", "BDS", "GLO", "GAL"]))

    def test_data_unpack_prints_the_progress_up_to_100(self):
        out, _ = self.assertOk(self.run_example("data_unpack", "--project", "p1", "--watch", "10"))
        self.assertIn("===== DataUnpackResponse =====", out)
        self.assertIn('"progress": 100', out)
        self.assertEqual(self.last_request("data_unpack")["project_name"], "p1")

    def test_data_unpack_needs_a_project(self):
        rc, _, err = self.run_example("data_unpack")
        self.assertEqual(rc, 2)
        self.assertIn("--project", err)

    # ---- the subscribe examples ----
    def test_device_status(self):
        out, _ = self.assertOk(self.run_example("device_status", "--seconds", "1"))
        self.assertIn("Subscribing to device status for 1s", out)
        self.assertIn("===== DeviceStatusReport =====", out)
        self.assertIn("FAKE-0001", out)

    def test_lio_pose(self):
        out, _ = self.assertOk(self.run_example("lio_pose", "--seconds", "1"))
        self.assertIn("===== LioPoseReport =====", out)

    def test_system_error(self):
        out, _ = self.assertOk(self.run_example("system_error", "--seconds", "1"))
        self.assertIn("===== SystemErrorReport =====", out)
        self.assertIn("fake error", out)

    def test_heartbeat_is_printed_as_the_text_that_was_sent(self):
        out, _ = self.assertOk(self.run_example("heartbeat", "--seconds", "1"))
        self.assertIn("===== heartbeat =====", out)
        self.assertIn('"DeviceSN": "FAKE-0001"', out)
        self.assertIn('"ProtocolVersion": "3.0.0"', out)

    def test_lio_pcl_decodes_the_lz4_block_the_python_side_made(self):
        out, _ = self.assertOk(self.run_example("lio_pcl", "--seconds", "1"))
        self.assertIn("compression: CompressionLz4", out)
        self.assertIn("decoded point cloud: 100 points (scaler=1000)", out)
        self.assertIn("first point: x=0.000 y=0.000 z=1.000 rgbi=0x10203040", out)
        self.assertNotIn("[error]", out)

    # ---- scan ----
    def test_modeling_runs_the_whole_flow(self):
        out, _ = self.assertOk(self.run_example("modeling", "--watch", "0.5", "--start-timeout",
                                                "20", "--stop-timeout", "20"))
        for part in ("[state] standby", "[state] scanning", "===== ModelingResponse (start) =====",
                     "===== ModelingResponse (stop) =====", "===== ModelingReport ====="):
            self.assertIn(part, out)
        reqs = self.requests("modeling")
        self.assertEqual([r["action"] for r in reqs][-2:], ["AC_START", "AC_STOP"])
        start = reqs[-2]
        self.assertEqual((start["project_name"], start["record_mode"], start["scan_mode"],
                          start["mount_type"]),
                         ("demo_project", "RM_RECORD_CALC", "SM_PCL", "MT_HAND"))

    def test_modeling_uses_the_project_name_it_is_given(self):
        self.assertOk(self.run_example("modeling", "--project", "site-7", "--watch", "0.2",
                                       "--start-timeout", "20", "--stop-timeout", "20"))
        starts = [r for r in self.requests("modeling") if r["action"] == "AC_START"]
        self.assertEqual(starts[-1]["project_name"], "site-7")

    def test_ctrl_c_while_scanning_stops_the_scan_and_exits_with_130(self):
        command = [os.path.join(CONFIG["examples_dir"], "modeling")] + self.base_args() + [
            "--watch", "60", "--start-timeout", "20", "--stop-timeout", "20"]
        proc = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        seen = threading.Event()
        lines = []

        def read():
            for raw in proc.stdout:
                lines.append(raw.decode())
                if "[state] scanning" in lines[-1]:
                    seen.set()

        reader = threading.Thread(target=read, daemon=True)
        reader.start()
        try:
            self.assertTrue(seen.wait(30), "the device never reached scanning:\n" + "".join(lines))
            proc.send_signal(signal.SIGINT)
            proc.wait(timeout=30)
        finally:
            if proc.poll() is None:
                proc.kill()
        reader.join(5)
        err = proc.stderr.read().decode()
        proc.stdout.close()
        proc.stderr.close()
        self.assertEqual(proc.returncode, 130, "".join(lines) + err)
        self.assertIn("interrupted", "".join(lines))
        actions = [r["action"] for r in self.requests("modeling")]
        self.assertEqual(actions[-2:], ["AC_START", "AC_STOP"])
        states = [e["state"] for e in self.log_entries() if "state" in e]
        self.assertEqual(states[-1], 0x1204012C, "the device is back in standby")

    # ---- TLS ----
    def tls_account(self):
        return ["--tls", "--username", os.environ["LIXEL_TEST_USER"],
                "--password", os.environ["LIXEL_TEST_PASSWORD"]]

    def test_tls_with_the_certificate_trusted(self):
        out, _ = self.assertOk(self.run_example(
            "device_info", *self.tls_account(), "--ca-file", os.environ["LIXEL_TEST_CA_FILE"],
            tls=True))
        self.assertIn("FAKE-0001", out)

    def test_tls_refuses_an_untrusted_certificate_and_says_what_to_do(self):
        rc, _, err = self.run_example("device_info", *self.tls_account(), tls=True)
        self.assertEqual(rc, 1)
        self.assertIn("cannot connect", err)
        self.assertIn("--ca-file", err)
        self.assertIn("--insecure", err)

    def test_tls_insecure_accepts_it(self):
        self.assertOk(self.run_example("device_info", *self.tls_account(), "--insecure", tls=True))

    def test_tls_with_a_wrong_password_fails(self):
        rc, _, err = self.run_example(
            "device_info", "--tls", "--insecure", "--username", os.environ["LIXEL_TEST_USER"],
            "--password", "nope", tls=True)
        self.assertEqual(rc, 1)
        self.assertIn("cannot connect", err)

    # ---- command line ----
    def test_every_example_has_help_and_the_connection_options(self):
        for name in EXAMPLES:
            with self.subTest(name):
                out, _ = self.assertOk(self.run_example(name, "--help", base=False))
                for option in ("--host", "--port", "--tls", "--username", "--password", "--key",
                               "--timeout"):
                    self.assertIn(option, out)

    def test_nothing_listening_is_a_clean_failure(self):
        rc, _, err = self.run_example("device_info", "--host", "127.0.0.1", "--port", "1",
                                      "--timeout", "2", base=False)
        self.assertEqual(rc, 1)
        self.assertIn("cannot connect to 127.0.0.1:1", err)

    def test_the_example_list_matches_the_python_scripts(self):
        usage = os.path.join(CONFIG["python_dir"], "example", "usage")
        python_names = sorted(f[:-3] for f in os.listdir(usage)
                              if f.endswith(".py") and not f.startswith("_"))
        self.assertEqual(python_names, sorted(EXAMPLES))
        for name in EXAMPLES:
            self.assertTrue(os.access(os.path.join(CONFIG["examples_dir"], name), os.X_OK), name)


class BusyDevice(DeviceCase):
    scenario = "busy"

    def test_a_device_that_is_scanning_is_left_alone(self):
        rc, _, err = self.run_example("modeling")
        self.assertEqual(rc, 2, err)
        self.assertIn("not in standby", err)
        self.assertEqual(self.requests("modeling"), [])


class RejectingDevice(DeviceCase):
    scenario = "reject-start"

    def test_a_rejected_start_is_exit_status_3_and_nothing_is_stopped(self):
        rc, out, err = self.run_example("modeling")
        self.assertEqual(rc, 3, out + err)
        self.assertIn("rejected AC_START", err)
        self.assertEqual([r["action"] for r in self.requests("modeling")], ["AC_START"])


class DeafToStart(DeviceCase):
    scenario = "silent-start"

    def test_no_answer_to_start_is_exit_status_4_and_nothing_is_stopped(self):
        rc, _, err = self.run_example("modeling", "--timeout", "1")
        self.assertEqual(rc, 4, err)
        self.assertIn("no response to ac_start", err.lower())
        self.assertEqual([r["action"] for r in self.requests("modeling")], ["AC_START"])


class DeafToStop(DeviceCase):
    scenario = "silent-stop"

    def test_no_answer_to_stop_is_exit_status_4_with_a_warning(self):
        rc, _, err = self.run_example("modeling", "--timeout", "1", "--watch", "0.2",
                                      "--start-timeout", "20")
        self.assertEqual(rc, 4, err)
        self.assertIn("may still be scanning", err)
        self.assertEqual([r["action"] for r in self.requests("modeling")],
                         ["AC_START", "AC_STOP"])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--examples-dir", required=True, help="directory with the built examples")
    parser.add_argument("--python-dir", required=True, help="the repository's python/ directory")
    args, rest = parser.parse_known_args()
    CONFIG["examples_dir"] = os.path.abspath(args.examples_dir)
    CONFIG["python_dir"] = os.path.abspath(args.python_dir)
    unittest.main(argv=[sys.argv[0]] + rest, verbosity=2)


if __name__ == "__main__":
    main()
