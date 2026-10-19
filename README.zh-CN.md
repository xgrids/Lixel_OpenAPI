# Lixel OpenAPI

基于 Lixel 设备 OpenAPI 进行二次开发所需的全部内容。OpenAPI 通过 MQTT 传输 protobuf 消息，
可以用 **Python** 或 **C++** 使用。

> 中文 README。English version: [README.md](README.md)。

```
Lixel_OpenAPI/
├── python/     Python：OpenAPI-XPanel 图形界面、示例脚本、可复用客户端
├── cpp/        C++：   客户端库与示例（预览）
├── proto/      共用：  两种方式都使用的 protobuf 定义
└── docs/       共用：  开发者指南与协议参考
```

## 选择你的语言

| | **[Python](python/README.zh-CN.md)** | **[C++](cpp/README.zh-CN.md)** |
|---|---|---|
| 提供内容 | OpenAPI-XPanel 图形界面、18 个示例脚本、可复用客户端 | 客户端库与示例 |
| 平台 | Linux、Windows | Linux（预览） |
| 状态 | 已发布（1.1.x） | 1.1.x 中的预览版，客户端接口尚未实现 |
| 从这里开始 | [python/README.zh-CN.md](python/README.zh-CN.md) | [cpp/README.zh-CN.md](cpp/README.zh-CN.md) |

两种方式使用同一套 MQTT 主题和同一份 protobuf 定义
（[`proto/openapi/aiot_msgs.proto`](proto/openapi/aiot_msgs.proto)），协议参考里描述的内容对两者都适用。

## 文档（共用）

- [`docs/lixel_openapi_developer_guide_en.md`](docs/lixel_openapi_developer_guide_en.md) —— 开发者指南（英文）
- [`docs/lixel_openapi_developer_guide_zh.md`](docs/lixel_openapi_developer_guide_zh.md) —— 开发者指南（中文）
- [`docs/lixel_openapi_protocol_reference_en.md`](docs/lixel_openapi_protocol_reference_en.md) —— 协议参考（英文）
- [`docs/lixel_openapi_protocol_reference_zh.md`](docs/lixel_openapi_protocol_reference_zh.md) —— 协议参考（中文）

## 从 1.0.x 升级

Python 相关文件移进了 `python/`，让两种语言并排。代码本身不变，只有定位 `proto/` 和仓库根目录的
脚本做了调整。旧路径加上 `python/` 前缀即可：

| 1.0.x | 现在 |
|-------|------|
| `example/main.py` | `python/example/main.py` |
| `example/usage/` | `python/example/usage/` |
| `src/generate_proto.py` | `python/src/generate_proto.py` |
| `build-venv.sh`、`build-wenv.cmd` | `python/build-venv.sh`、`python/build-wenv.cmd` |
| `build.sh`、`build.bat` | `python/build.sh`、`python/build.bat` |
| `requirements*.txt` | `python/requirements*.txt` |

## 版权

Copyright © XGRIDS. All rights reserved. <https://www.xgrids.com/>
