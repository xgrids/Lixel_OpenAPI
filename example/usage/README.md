# OpenAPI interface examples

Each script demonstrates how to call one interface and can be run directly, for
reference during secondary development. All are based on
`example.core.OpenApiClient` and do not depend on GUI / Qt.

## How to run

```bash
# From the project root, after activating the virtual environment
cd example/usage
python device_info.py --host 192.168.55.1 --port 1883 --key <your OpenAPI Key>
```

Requests without a valid OpenAPI Key are rejected with `EC_ProtoError` (`openapi auth error`), so always pass `--key`.

Common connection parameters (available to all scripts):

| Parameter | Description | Default |
|------|------|--------|
| `--host` | device address | 192.168.55.1 |
| `--port` | MQTT port | 1883 |
| `--tls` | use MQTTS(TLS) | off |
| `--username` | MQTT username | empty |
| `--password` | MQTT password | empty |
| `--key` | OpenAPI Key | empty |
| `--timeout` | request timeout (seconds) | 10 |

Extra parameters of `modeling.py`:

| Parameter | Description | Default |
|------|------|--------|
| `--project` | project name | demo_project |
| `--watch` | seconds to watch scan reports once the device is scanning | 5 |
| `--start-timeout` | max seconds to wait for the device to start scanning | 90 |
| `--stop-timeout` | max seconds to wait for the device to return to standby | 180 |

Exit codes of `modeling.py`: `0` success, `1` timeout while starting or stopping, `2` the device was not in standby, `3` the device rejected `AC_START`, `4` no response to `AC_START` or `AC_STOP`, `130` interrupted with Ctrl-C (`AC_STOP` is still sent). Starting takes several seconds (the device goes standby -> starting -> scanning) and stopping takes tens of seconds, so a full run takes a minute or more (71 s was measured with `--watch 10` on a Lixel L2Pro).

## Script overview

### Status monitoring (subscribe to reports)
- `heartbeat.py` -- device heartbeat (JSON payload, device discovery)
- `device_status.py` -- device status (1Hz)
- `lio_pose.py` -- real-time pose
- `lio_pcl.py` -- real-time point cloud (decompress + secondary decode, requires `pip install lz4 zstandard`)
- `system_error.py` -- system errors

### Device management (request-response)
- `device_info.py` -- query device info
- `device_config.py` -- configure device time/storage
- `device_control.py` -- device control (USB drive/Ethernet/power off/reboot, etc.)

### Scan mapping
- `modeling.py` -- start/stop scanning + subscribe to status. It waits for the device to be in standby, starts the scan, waits until the device is really scanning, watches the reports, stops the scan and waits for standby again
- `modeling_status.py` -- query scan output URLs

### Point management
- `control_point.py` -- add/remove/update control points
- `measure_point.py` -- add/remove measure points

### Project management
- `project_list.py` -- list/relabel/delete projects

### RTK / Ntrip
- `get_ntrip_profile.py` / `set_ntrip_profile.py` -- Ntrip(CORS) configuration
- `get_rtk_advance.py` / `set_rtk_advance.py` -- RTK advanced configuration

### Data processing
- `data_unpack.py` -- xbin data unpack + subscribe to progress
