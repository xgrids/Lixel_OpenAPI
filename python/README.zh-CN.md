# Lixel OpenAPI — Python

使用 Lixel 设备 OpenAPI 的 Python 方式：可复用的客户端库，以及每个接口一个示例脚本。仅支持 Linux。
图形界面 **OpenAPI-XPanel** 是单独的目录 [../xpanel](../xpanel/README.zh-CN.md)。C++ 方式见
[../cpp](../cpp/README.zh-CN.md)；仓库总览见 [../README.zh-CN.md](../README.zh-CN.md)。

> 中文 README。English version: [README.md](README.md)。

**本页所有命令都在仓库根目录执行**，所以每条路径都以 `python/` 开头。

## 功能特性

- **示例脚本** —— 每个接口一个可直接复制运行的最小示例
- **可复用核心** —— 与 Qt 无关的 OpenAPI/MQTT 客户端及 protobuf 编解码器
- 支持 MQTT / MQTTS (TLS) 连接设备
- 基于 JSON 模板的 protobuf 消息编解码
- 18 个开箱即用的接口示例脚本
- 在检出的仓库里用 `pip install -e python` 安装

## 目录结构

```
python/
├── pyproject.toml           # 包元数据（pip install -e python）
├── example/                 # 客户端库与示例
│   ├── core/                # 与 Qt 无关的客户端 + protobuf 编解码
│   │   ├── api_catalog.py   # 接口目录（主题、消息、元数据）
│   │   ├── openapi_client.py# 支持请求/响应的 MQTT 客户端
│   │   └── proto_codec.py   # Protobuf <-> JSON 编解码
│   └── usage/               # 18 个按接口划分的示例脚本
├── src/                     # 内部支持
│   ├── generate_proto.py    # 由 ../proto/ 生成 src/proto_gen
│   └── proto_gen/           # 生成的 protobuf 模块（请勿手动编辑）
└── requirements.txt         # 运行依赖（与 pyproject.toml 相同的列表）
```

protobuf 定义 [`../proto/openapi/aiot_msgs.proto`](../proto/openapi/aiot_msgs.proto) 与
[`../docs`](../docs/) 里的文档与 C++ 客户端共用。

## 环境要求

- Linux，Python 3.8+
- 可编辑安装需要 pip 21.3 及以上（Ubuntu 20.04 自带 pip 20.0.2，下面的命令会先升级 pip）
- `paho-mqtt`、`protobuf`（`pip install -e python` 会安装）
- 生成 protobuf 模块需要 `grpcio-tools`（`dev` 可选依赖会安装）

## 安装

在本仓库的检出目录里安装客户端库，然后生成 protobuf 模块（pip 不会生成它）：

```bash
python3 -m venv python/venv
source python/venv/bin/activate
pip install --upgrade pip
pip install -e "python[dev]"
python python/src/generate_proto.py
```

导入名与 1.x 相同：`from example.core import ClientOptions, OpenApiClient`，生成的消息位于
`src.proto_gen.openapi`。`example` 和 `src` 是通用的顶层包名，所以只支持在检出的仓库里做这种
editable 安装，不要把它装进共用的 `site-packages`。

## 生成 protobuf 代码

生成的 protobuf 模块位于 `python/src/proto_gen/openapi/`，由
`proto/openapi/aiot_msgs.proto` 生成：

```bash
python python/src/generate_proto.py
```

`proto/openapi/aiot_msgs.proto` 改动后需要重新执行。

## 示例脚本

`python/example/usage/` 下的每个脚本演示一个接口，可直接运行，且仅依赖
`example.core`（不依赖 Qt）。执行过 `pip install -e python` 后可在任意目录运行；在检出的仓库内，
只要有 `paho-mqtt` 和 `protobuf` 并且已生成 protobuf 模块，不安装也能运行。

```bash
cd python/example/usage
python device_info.py --host 192.168.55.1 --port 1883
```

通用参数：`--host`、`--port`、`--tls`、`--username`、`--password`、`--key`（可选，
设备已获得 OpenAPI 授权时不需要）、`--timeout`。
完整列表与各脚本说明见
[example/usage/README.md](example/usage/README.md)。

## OpenAPI-XPanel

图形化控制面板（发布请求、订阅上报、中英文界面）位于 [../xpanel](../xpanel/README.zh-CN.md)，
它把本库作为包依赖使用。

## 文档

- [`docs/lixel_openapi_developer_guide_en.md`](../docs/lixel_openapi_developer_guide_en.md) —— 开发者指南（英文）
- [`docs/lixel_openapi_developer_guide_zh.md`](../docs/lixel_openapi_developer_guide_zh.md) —— 开发者指南（中文）
- [`docs/lixel_openapi_protocol_reference_en.md`](../docs/lixel_openapi_protocol_reference_en.md) —— 协议参考（英文）
- [`docs/lixel_openapi_protocol_reference_zh.md`](../docs/lixel_openapi_protocol_reference_zh.md) —— 协议参考（中文）

## 版权

Copyright © XGRIDS. All rights reserved. <https://www.xgrids.com/>
