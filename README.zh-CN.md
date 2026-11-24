# Lixel OpenAPI

基于 Lixel 设备 OpenAPI 进行二次开发所需的全部内容。OpenAPI 通过 MQTT 传输 protobuf 消息，
可以用 **Python** 或 **C++** 使用。

> 中文 README。English version: [README.md](README.md)。

```
Lixel_OpenAPI/
├── python/     Python：OpenAPI-XPanel 图形界面、示例脚本、可复用客户端
├── cpp/        C++：   客户端库与 18 个示例
├── proto/      共用：  两种方式都使用的 protobuf 定义
└── docs/       共用：  开发者指南与协议参考
```

## 选择你的语言

| | **[Python](python/README.zh-CN.md)** | **[C++](cpp/README.zh-CN.md)** |
|---|---|---|
| 提供内容 | OpenAPI-XPanel 图形界面、18 个示例脚本、可复用客户端 | 客户端库，以及同样的 18 个示例（命令行程序） |
| 平台 | Linux、Windows | Linux |
| 状态 | 已发布：1.1.x。2.0.0 开发中，尚未发布 | 已发布：1.1.x（经测试和一台真机验证，见其 README）。2.0.0 开发中，尚未发布 |
| 从这里开始 | [python/README.zh-CN.md](python/README.zh-CN.md) | [cpp/README.zh-CN.md](cpp/README.zh-CN.md) |

两种方式使用同一套 MQTT 主题和同一份 protobuf 定义
（[`proto/openapi/aiot_msgs.proto`](proto/openapi/aiot_msgs.proto)），协议参考里描述的内容对两者都适用。

## 文档（共用）

- [`docs/lixel_openapi_developer_guide_en.md`](docs/lixel_openapi_developer_guide_en.md) —— 开发者指南（英文）
- [`docs/lixel_openapi_developer_guide_zh.md`](docs/lixel_openapi_developer_guide_zh.md) —— 开发者指南（中文）
- [`docs/lixel_openapi_protocol_reference_en.md`](docs/lixel_openapi_protocol_reference_en.md) —— 协议参考（英文）
- [`docs/lixel_openapi_protocol_reference_zh.md`](docs/lixel_openapi_protocol_reference_zh.md) —— 协议参考（中文）

## 2.0.0 的变化

已获得 OpenAPI 授权的设备接受不带 OpenAPI Key 的请求；保持 `openapi_key` 为空（默认值），客户端就不会
发送 Key。未授权或授权已用完的设备，会以 `EC_ProtoError` 拒绝没有有效 Key 的请求。仍使用早期 Key 方案的
设备行为与 1.x 相同。有效 Key 在每一类设备上都可用，所以同一个程序可以同时面对这几类设备：只对需要 Key
的设备设置 Key。为 1.x 编写的程序无需修改。线格式没有变化（`Header.openapi_key` 仍是第 6 号字段）。

授权由 XGRIDS 授予，本 SDK 不能授予授权。详见开发指南 2.2 节
（[English](docs/lixel_openapi_developer_guide_en.md)、[中文](docs/lixel_openapi_developer_guide_zh.md)）。

2.0.0 尚未在真机上验证。

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
