# Lixel OpenAPI — Python

The Python way to use a Lixel device's OpenAPI: the **OpenAPI-XPanel** GUI, one example script
per interface, and a reusable client. The C++ way is in [../cpp](../cpp/README.md); the
repository overview is in [../README.md](../README.md).

> English README. 中文版见 [README.zh-CN.md](README.zh-CN.md)。

**All commands on this page are run from the repository root**, so every path starts with
`python/`.

## Features

- **OpenAPI-XPanel** — a cross-platform GUI control panel to publish requests and
  subscribe to reports interactively
- **Usage scripts** — minimal, copy-pasteable examples for each interface
- **Reusable core** — a Qt-independent OpenAPI/MQTT client and protobuf codec
- MQTT / MQTTS (TLS) connection to the device
- Protobuf message encode/decode with JSON templates
- Interactive publish/subscribe GUI (English / Chinese, switchable at runtime)
- 18 ready-to-run interface example scripts
- One-file executable packaging via Nuitka for Linux and Windows

## Directory structure

```
python/
├── example/                 # Application and examples
│   ├── main.py              # OpenAPI-XPanel entry point
│   ├── core/                # Qt-independent client + protobuf codec
│   │   ├── api_catalog.py   # Interface catalog (topics, messages, metadata)
│   │   ├── openapi_client.py# MQTT client with request/response support
│   │   └── proto_codec.py   # Protobuf <-> JSON codec
│   ├── gui/                 # PySide GUI (control / publish / subscribe panels)
│   ├── usage/               # 18 per-interface example scripts
│   └── i18n/                # Qt translations (.ts / .qm) + build_i18n.sh
├── src/                     # Internal support
│   ├── qt_compat.py         # PySide2 / PySide6 binding shim
│   ├── generate_proto.py    # Generates src/proto_gen from ../proto/
│   └── proto_gen/           # Generated protobuf modules (do not edit)
├── assets/                  # Application icon (icon.ico / icon.png)
├── build.sh / build.bat     # Nuitka packaging (Linux / Windows)
├── build-venv.sh / build-wenv.cmd  # Create local dev virtual environments
└── requirements*.txt        # Runtime / packaging dependencies
```

The protobuf definition [`../proto/openapi/aiot_msgs.proto`](../proto/openapi/aiot_msgs.proto)
and the documents in [`../docs`](../docs/) are shared with the C++ client.

## Requirements

- Python 3.8+ (the packaged build targets Python 3.8 for glibc compatibility)
- Qt binding: **PySide2** on Linux, **PySide6** on Windows
- `paho-mqtt`, `protobuf`, `grpcio-tools`

Dependency files:

| File | Purpose |
|------|---------|
| `python/requirements.txt` | Common runtime deps (no GUI binding / packaging tools) |
| `python/requirements-linux.txt` | Linux run + packaging (PySide2 + nuitka) |
| `python/requirements-windows.txt` | Windows run + packaging (PySide6 + nuitka) |

## Quick start

### Linux

```bash
# Create a virtual environment and install dependencies
./python/build-venv.sh

# Run OpenAPI-XPanel (English UI by default; use --lang zh for Chinese)
source python/venv/bin/activate
python python/example/main.py
```

### Windows

```bat
rem Create a virtual environment and install dependencies
python\build-wenv.cmd

rem Run OpenAPI-XPanel
python\wenv\Scripts\activate
python python\example\main.py
```

The same steps in Git Bash (MINGW64) — note the `source`, otherwise the
environment is not activated in the current shell and the system Python is used
instead, which fails with `No supported Qt binding found`:

```bash
./python/build-wenv.cmd
source python/wenv/Scripts/activate
python python/example/main.py
```

The GUI language can also be switched at runtime via **Settings → Language**.

## Generating protobuf code

The generated protobuf modules live under `python/src/proto_gen/openapi/` and are
produced from `proto/openapi/aiot_msgs.proto`:

```bash
python python/src/generate_proto.py
```

The dev-environment scripts (`build-venv.sh` / `build-wenv.cmd`) and the build
scripts run this automatically.

## Building executables

Nuitka produces a single-file executable named **OpenAPI-XPanel**, using
`python/assets/icon` as the application icon.

### Linux (Docker, PySide2)

Builds inside the `ubuntu18-amd64:latest` image for older-glibc compatibility. The script mounts
the whole repository into the container, because the protobuf definition lives in `proto/`
outside this directory:

```bash
./python/build.sh V1.0.0       # version is optional, defaults to V0.0.0
# Output: python/dist/OpenAPI-XPanel
```

### Windows (Nuitka + MSVC, PySide6)

```bat
python\build.bat V1.0.0        rem version is optional, defaults to V0.0.0
rem Output: python\output\OpenAPI-XPanel.exe
```

The build scripts stamp `APP_VERSION` / `APP_BUILD_TIME` into `python/example/main.py`
before compiling and restore them afterwards.

## Example scripts

Each script in `python/example/usage/` demonstrates a single interface and can be run
directly. They depend only on `example.core` (no Qt).

```bash
cd python/example/usage
python device_info.py --host 192.168.55.1 --port 1883 --key <your OpenAPI Key>
```

Common parameters: `--host`, `--port`, `--tls`, `--username`, `--password`,
`--key`, `--timeout`. See [example/usage/README.md](example/usage/README.md)
for the full list and per-script descriptions.

## Localization

Source strings are written in English and wrapped with `tr()`. The Chinese
translation lives in `python/example/i18n/openapi_zh.ts` and is compiled to `.qm`:

```bash
bash python/example/i18n/build_i18n.sh release   # compile .ts -> .qm
```

## Documentation

- [`docs/lixel_openapi_developer_guide_en.md`](../docs/lixel_openapi_developer_guide_en.md) — developer guide (English)
- [`docs/lixel_openapi_developer_guide_zh.md`](../docs/lixel_openapi_developer_guide_zh.md) — developer guide (Chinese)
- [`docs/lixel_openapi_protocol_reference_en.md`](../docs/lixel_openapi_protocol_reference_en.md) — protocol reference (English)
- [`docs/lixel_openapi_protocol_reference_zh.md`](../docs/lixel_openapi_protocol_reference_zh.md) — protocol reference (Chinese)

## License

Copyright © XGRIDS. All rights reserved. <https://www.xgrids.com/>
