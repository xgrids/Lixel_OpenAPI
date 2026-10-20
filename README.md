# Lixel OpenAPI

Everything needed for secondary development against a Lixel device's OpenAPI, which runs over
MQTT with protobuf messages. Use it from **Python** or from **C++**.

> English README. 中文版见 [README.zh-CN.md](README.zh-CN.md)。

```
Lixel_OpenAPI/
├── python/     Python: OpenAPI-XPanel GUI, example scripts, a reusable client
├── cpp/        C++:    client library and 18 examples
├── proto/      Shared: the protobuf definition both of them use
└── docs/       Shared: developer guide and protocol reference
```

## Choose your language

| | **[Python](python/README.md)** | **[C++](cpp/README.md)** |
|---|---|---|
| What you get | OpenAPI-XPanel GUI, 18 example scripts, a reusable client | A client library and the same 18 examples as command line programs |
| Platforms | Linux, Windows | Linux |
| Status | Released (1.1.x) | 1.1.x; verified with tests and one real device, see its README |
| Start here | [python/README.md](python/README.md) | [cpp/README.md](cpp/README.md) |

Both talk to the device with the same MQTT topics and the same protobuf definition
([`proto/openapi/aiot_msgs.proto`](proto/openapi/aiot_msgs.proto)), so what the protocol
reference describes applies to both.

## Documentation (shared)

- [`docs/lixel_openapi_developer_guide_en.md`](docs/lixel_openapi_developer_guide_en.md) — developer guide (English)
- [`docs/lixel_openapi_developer_guide_zh.md`](docs/lixel_openapi_developer_guide_zh.md) — developer guide (Chinese)
- [`docs/lixel_openapi_protocol_reference_en.md`](docs/lixel_openapi_protocol_reference_en.md) — protocol reference (English)
- [`docs/lixel_openapi_protocol_reference_zh.md`](docs/lixel_openapi_protocol_reference_zh.md) — protocol reference (Chinese)

## Upgrading from 1.0.x

The Python files moved into `python/` so that the two languages sit side by side. The code
is the same; only the scripts that locate `proto/` or the repository root changed. Prefix the
old paths with `python/`:

| 1.0.x | now |
|-------|-----|
| `example/main.py` | `python/example/main.py` |
| `example/usage/` | `python/example/usage/` |
| `src/generate_proto.py` | `python/src/generate_proto.py` |
| `build-venv.sh`, `build-wenv.cmd` | `python/build-venv.sh`, `python/build-wenv.cmd` |
| `build.sh`, `build.bat` | `python/build.sh`, `python/build.bat` |
| `requirements*.txt` | `python/requirements*.txt` |

## License

Copyright © XGRIDS. All rights reserved. <https://www.xgrids.com/>
