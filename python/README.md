# Lixel OpenAPI — Python

The Python way to use a Lixel device's OpenAPI: a reusable client library and one example
script per interface. Linux only. The graphical control panel **OpenAPI-XPanel** is a separate
directory, [../xpanel](../xpanel/README.md). The C++ way is in [../cpp](../cpp/README.md); the
repository overview is in [../README.md](../README.md).

> English README. 中文版见 [README.zh-CN.md](README.zh-CN.md)。

**All commands on this page are run from the repository root**, so every path starts with
`python/`.

## Features

- **Usage scripts** — minimal, copy-pasteable examples for each interface
- **Reusable core** — a Qt-independent OpenAPI/MQTT client and protobuf codec
- MQTT / MQTTS (TLS) connection to the device
- Protobuf message encode/decode with JSON templates
- 19 ready-to-run interface example scripts
- Installable from the checkout with `pip install -e python`

## Directory structure

```
python/
├── pyproject.toml           # Package metadata (pip install -e python)
├── example/                 # Client library and examples
│   ├── core/                # Qt-independent client + protobuf codec
│   │   ├── api_catalog.py   # Interface catalog (topics, messages, metadata)
│   │   ├── openapi_client.py# MQTT client with request/response support
│   │   └── proto_codec.py   # Protobuf <-> JSON codec
│   └── usage/               # 19 per-interface example scripts
├── src/                     # Internal support
│   ├── generate_proto.py    # Generates src/proto_gen from ../proto/
│   └── proto_gen/           # Generated protobuf modules (do not edit)
└── requirements.txt         # Runtime dependencies (same list as pyproject.toml)
```

The protobuf definition [`../proto/openapi/aiot_msgs.proto`](../proto/openapi/aiot_msgs.proto)
and the documents in [`../docs`](../docs/) are shared with the C++ client.

## Requirements

- Linux, Python 3.8+
- pip 21.3 or newer for the editable install (Ubuntu 20.04 ships pip 20.0.2; the commands below upgrade it)
- `paho-mqtt`, `protobuf` (installed by `pip install -e python`)
- `grpcio-tools` to generate the protobuf module (installed by the `dev` extra)

## Installation

Install the client library from a checkout of this repository, then generate the protobuf
module (pip does not generate it):

```bash
python3 -m venv python/venv
source python/venv/bin/activate
pip install --upgrade pip
pip install -e "python[dev]"
python python/src/generate_proto.py
```

The import names are unchanged from 1.x: `from example.core import ClientOptions, OpenApiClient`,
and the generated messages are under `src.proto_gen.openapi`. Because `example` and `src` are
generic top-level names, only this editable install from the checkout is supported; do not
install the package into a shared `site-packages`.

## Generating protobuf code

The generated protobuf modules live under `python/src/proto_gen/openapi/` and are
produced from `proto/openapi/aiot_msgs.proto`:

```bash
python python/src/generate_proto.py
```

Run it again whenever `proto/openapi/aiot_msgs.proto` changes.

## Example scripts

Each script in `python/example/usage/` demonstrates a single interface and can be run
directly. They depend only on `example.core` (no Qt). After `pip install -e python` they
run from any directory; inside the checkout they also run without installing, provided
`paho-mqtt` and `protobuf` are available and the protobuf module has been generated.

```bash
cd python/example/usage
python device_info.py --host 192.168.55.1 --port 1883
```

Common parameters: `--host`, `--port`, `--tls`, `--username`, `--password`,
`--key` (optional; not needed when the device holds an active OpenAPI authorization),
`--timeout`.
See [example/usage/README.md](example/usage/README.md)
for the full list and per-script descriptions.

## OpenAPI-XPanel

The graphical control panel (publish requests, subscribe to reports, English / Chinese UI)
lives in [../xpanel](../xpanel/README.md). It uses this library as a package dependency.

## Documentation

- [`docs/lixel_openapi_developer_guide_en.md`](../docs/lixel_openapi_developer_guide_en.md) — developer guide (English)
- [`docs/lixel_openapi_developer_guide_zh.md`](../docs/lixel_openapi_developer_guide_zh.md) — developer guide (Chinese)
- [`docs/lixel_openapi_protocol_reference_en.md`](../docs/lixel_openapi_protocol_reference_en.md) — protocol reference (English)
- [`docs/lixel_openapi_protocol_reference_zh.md`](../docs/lixel_openapi_protocol_reference_zh.md) — protocol reference (Chinese)

## License

Copyright © XGRIDS. All rights reserved. <https://www.xgrids.com/>
