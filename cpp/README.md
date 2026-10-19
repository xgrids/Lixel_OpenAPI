# Lixel OpenAPI — C++ client (preview)

The C++ way to talk to a Lixel device over its OpenAPI. The Python way (OpenAPI-XPanel,
example scripts, reusable client) is in [../python](../python/README.md); the repository
overview is in [../README.md](../README.md).
Both use the **same MQTT topics and the same protobuf definition**
([`../proto/openapi/aiot_msgs.proto`](../proto/openapi/aiot_msgs.proto)).

> 中文版见 [README.zh-CN.md](README.zh-CN.md)。

## Status

**Preview, Linux only.** This directory currently contains the build system and the tests that
prove the wire format matches the Python client. The client API (connect, request/response,
subscribe, point cloud decoding, scan session) is **not implemented yet**. Do not build on it
before it is announced in a release.

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

The first configure builds protobuf from source. A clean build took 47 s with 20 parallel jobs on
the tested machine (Ubuntu 22.04).

### Options

| Option | Values | Default | Meaning |
|--------|--------|---------|---------|
| `LIXEL_OPENAPI_PROTOBUF_PROVIDER` | `AUTO`, `SYSTEM`, `FETCH` | `AUTO` | `AUTO` uses the system protobuf when it is 3.21.0 or newer, otherwise builds the pinned release. `SYSTEM` always uses the system one; below 3.21.12 a warning is printed, and below 3.15 the experimental proto3 `optional` switch is added automatically. `FETCH` always builds the pinned release |
| `LIXEL_OPENAPI_PAHO_PROVIDER` | `FETCH`, `SYSTEM` | `FETCH` | `SYSTEM` uses `libpaho-mqtt-dev`; its version differs from the one the device builds against and is not tested |
| `LIXEL_OPENAPI_SANITIZER` | empty, `address`, `thread`, `undefined` | empty | sanitizer for this project's own targets |
| `LIXEL_OPENAPI_BUILD_TESTS` | `ON`, `OFF` | `ON` | build the unit tests |

Ubuntu 22.04 ships protobuf 3.12.4. It can build this project with
`-DLIXEL_OPENAPI_PROTOBUF_PROVIDER=SYSTEM`, which was verified, but `AUTO` builds the pinned
release instead.

## Layout

```
cpp/
├── CMakeLists.txt
├── cmake/                     # dependency resolution, proto generation, sanitizers
├── include/lixel/openapi/     # public headers (namespace lixel::openapi)
├── src/
└── tests/                     # GoogleTest unit tests, no device or broker needed
```

The C++ sources of the protobuf messages are generated at build time into the build directory.
They are never checked in, because they depend on the protobuf release they are built with.
