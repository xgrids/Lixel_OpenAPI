# Lixel OpenAPI — C++ client

The C++ way to talk to a Lixel device over its OpenAPI. The Python way (example scripts,
reusable client library) is in [../python](../python/README.md), the OpenAPI-XPanel GUI is in
[../xpanel](../xpanel/README.md); the repository overview is in [../README.md](../README.md).
Both use the **same MQTT topics and the same protobuf definition**
([`../proto/openapi/aiot_msgs.proto`](../proto/openapi/aiot_msgs.proto)).

> 中文版见 [README.zh-CN.md](README.zh-CN.md)。

## Status
**Released: 2.0.0, Linux only.** The client library and the 19 command line examples are implemented:
connect over MQTT or MQTTS, request/response, subscriptions, JSON conversion, point cloud
decoding and the scan flow. There is no `install` or CMake package export yet: use it with
`add_subdirectory` (see below).

What has been verified, and what has not. The table below is the record made for 1.1.x. For
2.0.0 only the build and the full `ctest` run (including `broker_integration` and
`examples_e2e`, which now also cover devices that need no key) have been repeated; the
AddressSanitizer run and the real device test have not.

| Verified | How |
|----------|-----|
| Library logic, 91 cases | unit tests with a fake transport, including a stress test, also run under AddressSanitizer |
| MQTT layer, plain and TLS | tests against a throw-away Mosquitto broker |
| The 19 examples | end-to-end tests against a fake device played by the Python client side, plain and TLS, including exit codes and Ctrl-C |
| A real device | one Lixel L2Pro: read-only examples, a full scan during which 42 point cloud frames were decoded without error, and an interrupted scan |

| Not verified | |
|--------------|---|
| MQTTS against a real device | the L2Pro tested has no MQTTS port |
| Other device models, Windows, macOS, ThreadSanitizer | not available during development |

## Requirements (Linux)

| Item | Version | Note |
|------|---------|------|
| CMake | 3.16 or newer | tested with 3.22.1 |
| C++ compiler | C++14 | tested with GCC 11.4 |
| `git`, network access | | needed at configure time to fetch the pinned dependencies below |
| OpenSSL development files | | `libssl-dev` |
| lz4 development files | 1.9 or newer | `liblz4-dev`, found through `pkg-config` |
| GoogleTest | | optional, version 1.14.0 is fetched when the system one is missing |

Pinned dependencies fetched by CMake:

| Dependency | Version | Why this version |
|------------|---------|------------------|
| protobuf | v21.12 (3.21.12) | generated code and runtime must be the same release |
| paho.mqtt.c | v1.3.13 | the release the device side builds against |

## Build and test

```bash
cmake -S cpp -B cpp/build
cmake --build cpp/build -j
ctest --test-dir cpp/build --output-on-failure
```

The tests that need a broker (`broker_integration`) start a throw-away Mosquitto on free
ports of `127.0.0.1` and need `mosquitto`, `mosquitto_passwd` and `openssl`; without them that
test is not registered. No key or password is stored in the repository: the certificate and the
account are generated for each run.

The first configure builds protobuf from source. A clean build took 47 s with 20 parallel jobs on
the tested machine (Ubuntu 22.04).

### Options

| Option | Values | Default | Meaning |
|--------|--------|---------|---------|
| `LIXEL_OPENAPI_PROTOBUF_PROVIDER` | `AUTO`, `SYSTEM`, `FETCH` | `AUTO` | `AUTO` uses the system protobuf when it is 3.21.0 or newer, otherwise builds the pinned release. `SYSTEM` always uses the system one; below 3.21.12 a warning is printed, and below 3.15 the experimental proto3 `optional` switch is added automatically. `FETCH` always builds the pinned release |
| `LIXEL_OPENAPI_PAHO_PROVIDER` | `FETCH`, `SYSTEM` | `FETCH` | `SYSTEM` uses `libpaho-mqtt-dev`; its version differs from the one the device builds against and is not tested |
| `LIXEL_OPENAPI_SANITIZER` | empty, `address`, `thread`, `undefined` | empty | sanitizer for this project's own targets |
| `LIXEL_OPENAPI_BUILD_TESTS` | `ON`, `OFF` | `ON` when this is the top-level project | build the tests |
| `LIXEL_OPENAPI_BUILD_EXAMPLES` | `ON`, `OFF` | `ON` when this is the top-level project | build the 19 examples |
| `LIXEL_E2E_PYTHON` | path | empty | a Python with `paho-mqtt`, `protobuf`, `grpcio-tools` and `lz4` (`pip install -r python/requirements.txt lz4`); when set, the `examples_e2e` test is registered. It generates the Python message module on its first run |

Ubuntu 22.04 ships protobuf 3.12.4. It can build this project with
`-DLIXEL_OPENAPI_PROTOBUF_PROVIDER=SYSTEM`, which was verified, but `AUTO` builds the pinned
release instead.

## Use the library

Add it to your CMake project (nothing else is needed; protobuf and paho are fetched by this
directory's CMake):

```cmake
set(LIXEL_OPENAPI_BUILD_TESTS OFF)
set(LIXEL_OPENAPI_BUILD_EXAMPLES OFF)
add_subdirectory(path/to/Lixel_OpenAPI/cpp lixel)
target_link_libraries(my_app PRIVATE LixelOpenApi::lixel_openapi)
```

Connect, send a request, read the answer:

```cpp
#include "lixel/openapi/client.h"
#include "openapi/aiot_msgs.pb.h"

lixel::openapi::ClientOptions options;
options.transport.host = "192.168.55.1";
// options.openapi_key = "<your key>";  // only for a device on the earlier key-based scheme
lixel::openapi::Client client(options);
if (!client.Connect().ok()) return 1;

openapi::aiot_msgs::DeviceInfoRequest request;
openapi::aiot_msgs::DeviceInfoResponse response;
lixel::openapi::Result r = client.Request("lixel/application/request/device_info", request,
                                          "lixel/application/response/device_info", &response);
// r.ok(): answered with a success code. Errc::DeviceError: the device answered with an error
// code (r.device_code, r.desc), `response` is filled. Errc::RequestTimeout: no answer.
```

| Header | What it gives you |
|--------|-------------------|
| `client.h` | `Client`: `Connect`, `Request`, `Publish`, `Subscribe`, `SubscribeMessage`, `RemoveSubscription` |
| `scan.h` | `RunScan`: start a scan, wait until the device scans, stop it, wait for standby |
| `pointcloud.h` | `DecodePointCloud`: the `lio_pcl` payload to scaled points |
| `codec.h` | message by name, protobuf to and from JSON |
| `catalog.h` | the table of 19 interfaces and their 36 topics |
| `codes.h`, `result.h` | device status codes, `Result` and `Errc` |

How it differs from the Python client, on purpose:

- A response is matched to its request by `header.seq`, which the device copies into the answer.
  The Python client takes the first message on the response topic.
- JSON conversion is strict: a misspelled field or enum name is an error. The Python codec drops
  it silently.
- MQTTS verifies the server certificate by default. The device certificate is self-signed, so pass
  `ca_file` (the examples: `--ca-file`) or switch verification off (`--insecure`). The Python
  client never verifies.
- The `report/device_config` topic that the Python catalog lists is not used: the device has no such
  topic.
- Calling `Request` from inside a subscription handler returns `Errc::CalledFromCallback` instead
  of blocking.

Limits to know:

- Several `Client` objects in one process share one worker thread of the MQTT library, which wakes
  once a second: opening a second connection takes about two seconds (measured: 2.10 s).
  A process with one `Client`, like every example, is not affected.
- A `Request` that is waiting cannot be cancelled; it ends within `request_timeout`.
- Zstd compressed point clouds are reported as `Errc::Unsupported`. A negative `header.scaler` too.

## Examples

`examples/` has one program for each script in `python/example/usage/`, with the same name and
options. Build them with the library (`-DLIXEL_OPENAPI_BUILD_EXAMPLES=ON`, on by default) and find
them in `cpp/build/examples/`.

```bash
./cpp/build/examples/heartbeat --seconds 5
./cpp/build/examples/device_info
./cpp/build/examples/modeling --project demo --watch 8
./cpp/build/examples/lio_pcl --seconds 20                       # while the device scans
./cpp/build/examples/device_info --key <your key>               # device on the earlier key-based scheme
```

Common options: `--host` (default `192.168.55.1`), `--port`, `--tls`, `--username`, `--password`,
`--key` (optional; not needed when the device holds an active OpenAPI authorization), `--timeout`, and for TLS
`--ca-file` and `--insecure` (not in the Python scripts).
`--help` lists the rest. Exit status: 0 success, 1 could not connect or no answer, 2 bad option
(`modeling`: the device was not in standby), 3 the device answered with an error code (the answer is
printed anyway), 130 interrupted. `modeling` has more exit codes, listed in its source header.

The 19 examples: `control_point`, `data_unpack`, `device_config`, `device_control`, `device_info`,
`device_status`, `get_ntrip_profile`, `get_rtk_advance`, `heartbeat`, `lio_pcl`, `lio_pose`,
`measure_point`, `modeling`, `modeling_status`, `openapi_license`, `project_list`,
`set_ntrip_profile`, `set_rtk_advance`, `system_error`. `device_control`, `device_config`, `set_ntrip_profile`,
`set_rtk_advance`, `control_point` and `measure_point` change the device; `device_control` can power
it off or format the disk.

## Tests

| Program | Cases | What it covers |
|---------|------:|----------------|
| `test_smoke` | 7 | build pipeline, wire format against the Python client |
| `test_catalog_codec` | 16 | interface table, JSON conversion, status codes |
| `test_codec_standalone` | 2 | finding messages by name in a program that includes no generated header |
| `test_client`, `test_client_stress` | 27, 2 | request/response, subscriptions, reconnect, thread safety |
| `test_pointcloud` | 16 | frames made by the Python side, damaged and random input |
| `test_scan` | 24 | the scan flow against a scripted device |
| `test_broker` (as `broker_integration`) | 23 | the MQTT layer against Mosquitto, plain and TLS |
| `tests/e2e/run_examples.py` (as `examples_e2e`) | 56 | the 19 examples against a fake device |
| `catalog_drift` | 1 | the C++ table equals the Python catalog (`tools/catalog_tool.py check`) |

## Layout

```
cpp/
├── CMakeLists.txt
├── cmake/                     # dependency resolution, proto generation, sanitizers
├── include/lixel/openapi/     # public headers (namespace lixel::openapi)
├── src/
├── examples/                  # the 19 command line examples and their shared helpers
├── tools/catalog_tool.py      # generates and checks the interface table
└── tests/                     # GoogleTest tests; broker/ and e2e/ hold the scripts that need a broker
```

The C++ sources of the protobuf messages are generated at build time into the build directory.
They are never checked in, because they depend on the protobuf release they are built with.
