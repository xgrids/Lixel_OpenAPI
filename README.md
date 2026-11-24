# Lixel OpenAPI

Everything needed for secondary development against a Lixel device's OpenAPI, which runs over
MQTT with protobuf messages. Use it from **Python** or from **C++**.

> English README. 中文版见 [README.zh-CN.md](README.zh-CN.md)。

```
Lixel_OpenAPI/
├── python/     Python: example scripts and a reusable client library
├── xpanel/     OpenAPI-XPanel GUI (Linux only)
├── cpp/        C++:    client library and 18 examples
├── proto/      Shared: the protobuf definition both of them use
└── docs/       Shared: developer guide and protocol reference
```

## Choose your language

| | **[Python](python/README.md)** | **[C++](cpp/README.md)** |
|---|---|---|
| What you get | 18 example scripts, a reusable client library | A client library and the same 18 examples as command line programs |
| Platforms | Linux | Linux |
| Status | Released: 1.1.x. 2.0.0 is in development and not released yet | Released: 1.1.x (verified with tests and one real device, see its README). 2.0.0 is in development and not released yet |
| Start here | [python/README.md](python/README.md) | [cpp/README.md](cpp/README.md) |

Both talk to the device with the same MQTT topics and the same protobuf definition
([`proto/openapi/aiot_msgs.proto`](proto/openapi/aiot_msgs.proto)), so what the protocol
reference describes applies to both.

The graphical control panel **OpenAPI-XPanel** is in [`xpanel/`](xpanel/README.md). It runs on
Ubuntu 20.04 / 22.04 / 24.04 (x86_64) and uses the Python client library as a package dependency.
The system libraries it needs (`libxcb-cursor0` and other Qt runtime libraries) are listed in
[`xpanel/README.md`](xpanel/README.md).

## Documentation (shared)

- [`docs/lixel_openapi_developer_guide_en.md`](docs/lixel_openapi_developer_guide_en.md) — developer guide (English)
- [`docs/lixel_openapi_developer_guide_zh.md`](docs/lixel_openapi_developer_guide_zh.md) — developer guide (Chinese)
- [`docs/lixel_openapi_protocol_reference_en.md`](docs/lixel_openapi_protocol_reference_en.md) — protocol reference (English)
- [`docs/lixel_openapi_protocol_reference_zh.md`](docs/lixel_openapi_protocol_reference_zh.md) — protocol reference (Chinese)

## What changes in 2.0.0

A device that has been given an OpenAPI authorization accepts requests with no OpenAPI Key; leave
`openapi_key` empty (the default) and the clients send none. A device that is not authorized, or
whose authorization is used up, rejects a request without a valid key with `EC_ProtoError`. A
device that is still on the earlier key-based scheme works as in 1.x. A valid key works on every
kind of device, so the same program can talk to all of them: set the key only for a device that
needs it. Programs written for 1.x need no change. The wire format is unchanged
(`Header.openapi_key` keeps field number 6).

The authorization is granted by XGRIDS; this SDK cannot grant it. See section 2.2 of the developer
guide ([English](docs/lixel_openapi_developer_guide_en.md), [中文](docs/lixel_openapi_developer_guide_zh.md)).

From 2.0.0 the Python client and OpenAPI-XPanel support Linux only: the Windows build scripts are
removed, OpenAPI-XPanel moved to `xpanel/`, and its Qt binding is PySide6 only.

Version 2.0.0 has not been verified on a real device yet.

## Upgrading from 1.0.x

The Python files moved into `python/` so that the two languages sit side by side, and the
graphical control panel moved out of `python/` into `xpanel/`. The client code is the same; only
the scripts that locate `proto/` or the repository root changed. Install the client library with
`pip install -e python`. Where the old paths went:

| 1.0.x | now |
|-------|-----|
| `example/main.py` | `xpanel/main.py` |
| `example/usage/` | `python/example/usage/` |
| `src/generate_proto.py` | `python/src/generate_proto.py` |
| `build-venv.sh` | `xpanel/build-venv.sh` |
| `build.sh` | `xpanel/build.sh` |
| `requirements.txt` | `python/requirements.txt` |
| `requirements-linux.txt` | `xpanel/requirements.txt` |
| the Windows build scripts and Windows requirements file | removed (2.0.0 supports Linux only) |

## License

Copyright © XGRIDS. All rights reserved. <https://www.xgrids.com/>
