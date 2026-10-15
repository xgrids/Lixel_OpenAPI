# lixel_openapi

Lixel 设备 OpenAPI 例程与调试工具（**OpenAPI-XPanel**）。

本仓库提供基于 MQTT 对 Lixel 设备 OpenAPI 进行二次开发所需的全部内容：

- **OpenAPI-XPanel** —— 跨平台图形化控制面板，可交互式发布请求、订阅上报。
- **示例脚本** —— 每个接口一个可直接复制运行的最小示例。
- **可复用核心** —— 与 Qt 无关的 OpenAPI/MQTT 客户端及 protobuf 编解码器。

> 中文 README。English version: [README.md](README.md)。

## 功能特性

- 支持 MQTT / MQTTS (TLS) 连接设备
- 基于 JSON 模板的 protobuf 消息编解码
- 交互式发布/订阅图形界面（中英文，运行时可切换）
- 18 个开箱即用的接口示例脚本
- 通过 Nuitka 打包为 Linux 与 Windows 单文件可执行程序

## 项目结构

```
lixel_openapi/
├── example/                 # 应用与示例
│   ├── main.py              # OpenAPI-XPanel 入口
│   ├── core/                # 与 Qt 无关的客户端 + protobuf 编解码
│   │   ├── api_catalog.py   # 接口目录（主题、消息、元数据）
│   │   ├── openapi_client.py# 支持请求/响应的 MQTT 客户端
│   │   └── proto_codec.py   # Protobuf <-> JSON 编解码
│   ├── gui/                 # PySide 图形界面（控制 / 发布 / 订阅面板）
│   ├── usage/               # 18 个按接口划分的示例脚本
│   └── i18n/                # Qt 翻译文件（.ts / .qm）+ build_i18n.sh
├── proto/openapi/           # Protobuf 定义（aiot_msgs.proto）
├── src/                     # 内部支持
│   ├── qt_compat.py         # PySide2 / PySide6 绑定兼容层
│   ├── generate_proto.py    # 由 proto/ 生成 src/proto_gen
│   └── proto_gen/           # 生成的 protobuf 模块（请勿手动编辑）
├── assets/                  # 应用图标（icon.ico / icon.png）
├── docs/                    # 开发者指南 + 协议参考
├── build.sh / build.bat     # Nuitka 打包（Linux / Windows）
├── build-venv.sh / build-wenv.cmd  # 创建本地开发虚拟环境
└── requirements*.txt        # 运行 / 打包依赖
```

## 环境要求

- Python 3.8+（打包目标为 Python 3.8，以兼容较旧的 glibc）
- Qt 绑定：Linux 使用 **PySide2**，Windows 使用 **PySide6**
- `paho-mqtt`、`protobuf`、`grpcio-tools`

依赖文件：

| 文件 | 用途 |
|------|------|
| `requirements.txt` | 通用运行依赖（不含 GUI 绑定 / 打包工具） |
| `requirements-linux.txt` | Linux 运行 + 打包（PySide2 + nuitka） |
| `requirements-windows.txt` | Windows 运行 + 打包（PySide6 + nuitka） |

## 快速开始

### Linux

```bash
# 创建虚拟环境并安装依赖
./build-venv.sh

# 运行 OpenAPI-XPanel（默认英文界面；使用 --lang zh 切换为中文）
source venv/bin/activate
python example/main.py
```

### Windows

```bat
rem 创建虚拟环境并安装依赖
build-wenv.cmd

rem 运行 OpenAPI-XPanel
wenv\Scripts\activate
python example\main.py
```

在 Git Bash（MINGW64）下同样步骤 —— 注意必须加 `source`，否则环境不会在当前
shell 中生效，实际使用的仍是系统 Python，会报 `No supported Qt binding found`：

```bash
./build-wenv.cmd
source wenv/Scripts/activate
python example/main.py
```

界面语言也可在运行时通过 **设置 → 语言** 切换。

## 生成 protobuf 代码

生成的 protobuf 模块位于 `src/proto_gen/openapi/`，由
`proto/openapi/aiot_msgs.proto` 生成：

```bash
python src/generate_proto.py
```

开发环境脚本（`build-venv.sh` / `build-wenv.cmd`）与构建脚本会自动执行该步骤。

## 构建可执行程序

Nuitka 会生成名为 **OpenAPI-XPanel** 的单文件可执行程序，并使用
`assets/icon` 作为应用图标。

### Linux（Docker，PySide2）

在 `ubuntu18-amd64:latest` 镜像中构建，以兼容较旧的 glibc：

```bash
./build.sh V1.0.0       # 版本号可选，默认 V0.0.0
# 产出：dist/OpenAPI-XPanel
```

### Windows（Nuitka + MSVC，PySide6）

```bat
build.bat V1.0.0        rem 版本号可选，默认 V0.0.0
rem 产出：output\OpenAPI-XPanel.exe
```

构建脚本会在编译前将 `APP_VERSION` / `APP_BUILD_TIME` 写入
`example/main.py`，并在编译后还原。

## 示例脚本

`example/usage/` 下的每个脚本演示一个接口，可直接运行，且仅依赖
`example.core`（不依赖 Qt）。

```bash
cd example/usage
python device_info.py --host 192.168.55.1 --port 1883 --key <你的 OpenAPI Key>
```

通用参数：`--host`、`--port`、`--tls`、`--username`、`--password`、`--key`、
`--timeout`。完整列表与各脚本说明见
[example/usage/README.md](example/usage/README.md)。

## 本地化

源文本以英文编写并用 `tr()` 包裹。中文翻译位于
`example/i18n/openapi_zh.ts`，编译为 `.qm`：

```bash
bash example/i18n/build_i18n.sh release   # 编译 .ts -> .qm
```

## 文档

- `docs/lixel_openapi_developer_guide_en.md` —— 开发者指南（英文）
- `docs/lixel_openapi_developer_guide_zh.md` —— 开发者指南（中文）
- `docs/lixel_openapi_protocol_reference_en.md` —— 协议参考（英文）
- `docs/lixel_openapi_protocol_reference_zh.md` —— 协议参考（中文）

## 版权

Copyright © XGRIDS. All rights reserved. <https://www.xgrids.com/>
