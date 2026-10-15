# OpenAPI interface examples

Each script demonstrates how to call one interface and can be run directly, for
reference during secondary development. All are based on
`example.core.OpenApiClient` and do not depend on GUI / Qt.

## How to run

```bash
# From the project root, after activating the virtual environment
cd example/usage
python device_info.py --host 192.168.12.1 --port 1883
```

Common connection parameters (available to all scripts):

| Parameter | Description | Default |
|------|------|--------|
| `--host` | device address | 192.168.12.1 |
| `--port` | MQTT port | 1883 |
| `--tls` | use MQTTS(TLS) | off |
| `--username` | MQTT username | empty |
| `--password` | MQTT password | empty |
| `--key` | OpenAPI Key | empty |
| `--timeout` | request timeout (seconds) | 10 |

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
- `modeling.py` -- start/stop scanning + subscribe to status
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
