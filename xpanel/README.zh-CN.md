# OpenAPI-XPanel

OpenAPI-XPanel 是 Lixel OpenAPI 的图形化控制面板：交互式地向设备发布请求、订阅设备上报。
**仅支持 Linux**（Ubuntu 20.04 / 22.04 / 24.04，x86_64）。它把 [../python](../python/README.zh-CN.md)
里的 Python 客户端库作为包依赖使用。仓库总览见 [../README.zh-CN.md](../README.zh-CN.md)。

> 中文 README。English version: [README.md](README.md)。

**本页所有命令都在仓库根目录执行**，所以每条路径都以 `xpanel/` 开头。

## 功能特性

- 发布面板：主题与消息类型下拉框、JSON 模板、一键发布
- 订阅面板：主题与通配符订阅、已收消息列表及详情查看
- 连接栏里有可选的 OpenAPI Key 输入框
- 支持 MQTT / MQTTS (TLS) 连接设备
- 中英文界面，运行时可通过 **设置 → 语言** 切换
- 通过 Nuitka 打包为单文件可执行程序

## 支持的系统与运行要求

| 项目 | 要求 |
|------|------|
| 操作系统 | Ubuntu 20.04、22.04 或 24.04（x86_64） |
| 系统库 | `libxcb-cursor0`（Qt 6.5+ 的 X11 平台插件需要它）以及下面列出的 Qt 运行库 |

```bash
sudo apt install libxcb-cursor0 libxcb-icccm4 libxcb-keysyms1 libxcb-shape0 libxcb-randr0 \
    libxkbcommon0 libxkbcommon-x11-0 libglib2.0-0 libgl1 libegl1 libfontconfig1 libdbus-1-3 \
    libgssapi-krb5-2
```

可执行程序在 Ubuntu 20.04（glibc 2.31）上构建，所以也能在更新的版本上运行；它用到的最高 glibc 符号是
`GLIBC_2.29`。已在干净的 `ubuntu:20.04`、`ubuntu:22.04`、`ubuntu:24.04` 镜像上装好上面的软件包后，用
`--self-test` 分别在 `QT_QPA_PLATFORM=offscreen` 和虚拟 X 显示（xvfb）下运行通过。桌面版系统里大部分软件包已经
安装，`apt` 会跳过它们。缺少 `libxcb-cursor0` 时 X11 平台插件无法加载，程序以退出码 134 退出；
`QT_QPA_PLATFORM=offscreen` 仍可运行。

## 从源码运行

```bash
# 创建 xpanel/venv，安装 PySide6==6.6.3.1 与客户端库（pip install -e python），
# 并生成 protobuf 模块
./xpanel/build-venv.sh

# 运行 OpenAPI-XPanel（默认英文界面；使用 --lang zh 切换为中文）
source xpanel/venv/bin/activate
python xpanel/main.py
```

从源码运行需要 Python 3.8 到 3.12（PySide6 6.6.3.1 支持的范围），以及上面列出的
软件包。

`xpanel/main.py` 的命令行参数：

| 参数 | 说明 |
|------|------|
| `--lang zh` | 以中文界面启动（默认英文） |
| `--self-test` | 隐藏的检查参数：构造主窗口，打印一行 `XPANEL_SELF_TEST_OK ...`，不显示窗口，以退出码 0 退出。配合 `QT_QPA_PLATFORM=offscreen` 用来在新系统上检查打包出的可执行程序 |

## 构建可执行程序

```bash
./xpanel/build.sh V2.0.0       # 版本号可选，默认 V0.0.0
# 产出：xpanel/dist/OpenAPI-XPanel，同目录还有 LICENSE.txt 和 THIRD-PARTY-NOTICES.txt
```

`build.sh` 需要 Docker 和网络（如果你的网络访问不到 `archive.ubuntu.com`，设置 `XPANEL_APT_MIRROR`，
例如 `XPANEL_APT_MIRROR=<你的 apt 镜像地址>/ubuntu ./xpanel/build.sh V2.0.0`；该变量只在构建镜像时使用）。它在镜像 `xpanel-build-focal:latest`（Ubuntu 20.04、系统自带
Python 3.8、Nuitka、PySide6 6.6.3.1）中构建，镜像不存在时按 [Dockerfile](Dockerfile) 自动构建。
脚本处理的是 `xpanel/`、`python/`、`proto/` 和 `LICENSE.txt` 的暂存副本：版本号和构建时间写进副本里的
`openapi_xpanel/_build_info.py`，protobuf 模块在容器内生成，所以检出目录里的源文件不会被改动。

在干净的系统上检查产物：

```bash
QT_QPA_PLATFORM=offscreen xpanel/dist/OpenAPI-XPanel --self-test
```

## 为什么锁定 PySide6 6.6.3.1

[requirements.txt](requirements.txt) 把 PySide6 锁定为 `6.6.3.1`。它是最后一个支持 Python 3.8
（Ubuntu 20.04 的系统 Python）的版本，其 Linux wheel 要求 glibc 2.28 或更新。之后的版本要求
Python 3.9 或更新。

## 本地化

源文本以英文编写并用 `tr()` 包裹。中文翻译位于 `xpanel/openapi_xpanel/i18n/openapi_zh.ts`，
编译为 `.qm`。发布面板里显示的接口说明来自 `python/example/core/api_catalog.py`，所以 `.ts`
里对应的原文必须与它逐字一致。请手工编辑 `.ts`（`lupdate` 会重排整个文件），然后编译：

```bash
bash xpanel/openapi_xpanel/i18n/build_i18n.sh release   # 编译 .ts -> .qm
```

`build_i18n.sh` 需要系统的 Qt 工具 `lrelease`（`update` 命令还需要 `lupdate`）。

## 目录结构

```
xpanel/
├── main.py                  # 入口
├── openapi_xpanel/          # XPanel 包（内部使用，不是公开 API）
│   ├── _build_info.py       # APP_VERSION / APP_BUILD_TIME，由 build.sh 写入暂存副本
│   ├── gui/                 # 控制 / 发布 / 订阅面板
│   └── i18n/                # 翻译文件（.ts / .qm）+ build_i18n.sh
├── assets/                  # 应用图标（icon.ico / icon.png）
├── fonts.conf               # 字体配置，随可执行程序一起打包
├── requirements.txt         # PySide6==6.6.3.1
├── Dockerfile               # 构建镜像（Ubuntu 20.04）
├── build.sh                 # 在 Docker 里用 Nuitka 打包
├── build-venv.sh            # 创建本地开发环境
└── THIRD-PARTY-NOTICES.txt  # 随可执行程序发布的第三方声明
```

发布包包含 `OpenAPI-XPanel`、`LICENSE.txt` 和 `THIRD-PARTY-NOTICES.txt`；这两个文本文件缺任何一个，
`build.sh` 都会失败。

## 版权

Copyright © XGRIDS. All rights reserved. <https://www.xgrids.com/>
