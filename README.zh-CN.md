# Lixel OpenAPI

基于 Lixel 设备 OpenAPI 进行二次开发所需的全部内容。OpenAPI 通过 MQTT 传输 protobuf 消息，
可以用 **Python** 或 **C++** 使用。

> 中文 README。English version: [README.md](README.md)。

```
Lixel_OpenAPI/
├── python/     Python：示例脚本与可复用客户端库
├── xpanel/     OpenAPI-XPanel 图形界面（仅 Linux）
├── cpp/        C++：   客户端库与 18 个示例
├── proto/      共用：  两种方式都使用的 protobuf 定义
└── docs/       共用：  开发者指南与协议参考
```

## 选择你的语言

| | **[Python](python/README.zh-CN.md)** | **[C++](cpp/README.zh-CN.md)** |
|---|---|---|
| 提供内容 | 18 个示例脚本、可复用客户端库 | 客户端库，以及同样的 18 个示例（命令行程序） |
| 平台 | Linux | Linux |
| 状态 | 已发布：2.0.0 | 已发布：2.0.0（验证情况见其 README） |
| 从这里开始 | [python/README.zh-CN.md](python/README.zh-CN.md) | [cpp/README.zh-CN.md](cpp/README.zh-CN.md) |

两种方式使用同一套 MQTT 主题和同一份 protobuf 定义
（[`proto/openapi/aiot_msgs.proto`](proto/openapi/aiot_msgs.proto)），协议参考里描述的内容对两者都适用。

图形化控制面板 **OpenAPI-XPanel** 位于 [`xpanel/`](xpanel/README.zh-CN.md)。它运行在
Ubuntu 20.04 / 22.04 / 24.04（x86_64）上，并把 Python 客户端库作为包依赖使用。
它需要的系统库（`libxcb-cursor0` 和其他 Qt 运行库）列在 [`xpanel/README.zh-CN.md`](xpanel/README.zh-CN.md) 中。

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

从 2.0.0 起，Python 客户端与 OpenAPI-XPanel 只支持 Linux：Windows 构建脚本已删除，
OpenAPI-XPanel 移到了 `xpanel/`，其 Qt 绑定只用 PySide6。

2.0.0 尚未在真机上验证。

## 从 1.0.x 升级

Python 相关文件移进了 `python/`，让两种语言并排；图形化控制面板则从 `python/` 里移出，放到 `xpanel/`。
客户端代码本身不变，只有定位 `proto/` 和仓库根目录的脚本做了调整。客户端库用 `pip install -e python`
安装。旧路径的去向：

| 1.0.x | 现在 |
|-------|------|
| `example/main.py` | `xpanel/main.py` |
| `example/usage/` | `python/example/usage/` |
| `src/generate_proto.py` | `python/src/generate_proto.py` |
| `build-venv.sh` | `xpanel/build-venv.sh` |
| `build.sh` | `xpanel/build.sh` |
| `requirements.txt` | `python/requirements.txt` |
| `requirements-linux.txt` | `xpanel/requirements.txt` |
| Windows 构建脚本与 Windows 依赖文件 | 已删除（2.0.0 起仅支持 Linux） |

## 版权

Copyright © XGRIDS. All rights reserved. <https://www.xgrids.com/>
