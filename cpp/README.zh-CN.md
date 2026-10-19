# Lixel OpenAPI — C++ 客户端（预览版）

通过 OpenAPI 与 Lixel 设备通信的 C++ 方式。Python 方式（OpenAPI-XPanel、示例脚本、可复用客户端）
在 [../python](../python/README.zh-CN.md)，仓库总览见 [../README.zh-CN.md](../README.zh-CN.md)。
两种方式使用**同一套 MQTT 主题和同一份 protobuf 定义**
（[`../proto/openapi/aiot_msgs.proto`](../proto/openapi/aiot_msgs.proto)）。

> English version: [README.md](README.md)。

## 当前状态

**1.1.x 中的预览版，仅支持 Linux。** 目前这个目录只有构建系统，以及证明线上编码与 Python 客户端一致的测试。
客户端接口（连接、请求/响应、订阅、点云解码、扫描会话）**尚未实现**，在发布说明里宣布之前请不要基于它开发。

## 环境要求（Linux）

| 项目 | 版本 | 说明 |
|------|------|------|
| CMake | 3.16 及以上 | 已用 3.22.1 验证 |
| C++ 编译器 | C++14 | 已用 GCC 11.4 验证 |
| `git` 与网络 | | 配置阶段需要联网拉取下面固定版本的依赖 |
| OpenSSL 开发文件 | | `libssl-dev` |
| lz4 开发文件 | 1.9 及以上 | `liblz4-dev`，通过 `pkg-config` 查找 |
| GoogleTest | | 可选；系统没有时自动拉取 1.14.0 |

CMake 拉取并固定的依赖：

| 依赖 | 版本 | 选择原因 |
|------|------|----------|
| protobuf | v21.12（3.21.12） | 生成代码与运行库必须是同一个版本 |
| paho.mqtt.c | v1.3.13 | 与设备端构建所用的版本一致 |

## 构建与测试

```bash
cmake -S cpp -B cpp/build
cmake --build cpp/build -j
ctest --test-dir cpp/build --output-on-failure
```

首次配置会从源码构建 protobuf。在验证用的机器（Ubuntu 22.04）上，20 路并行的全新构建耗时 47 秒。

### 选项

| 选项 | 取值 | 默认 | 含义 |
|------|------|------|------|
| `LIXEL_OPENAPI_PROTOBUF_PROVIDER` | `AUTO`、`SYSTEM`、`FETCH` | `AUTO` | `AUTO`：系统 protobuf 为 3.21.0 及以上时使用系统版本，否则构建固定版本。`SYSTEM`：始终用系统版本，低于 3.21.12 会给出警告，低于 3.15 会自动加 proto3 `optional` 的实验开关。`FETCH`：始终构建固定版本 |
| `LIXEL_OPENAPI_PAHO_PROVIDER` | `FETCH`、`SYSTEM` | `FETCH` | `SYSTEM` 使用 `libpaho-mqtt-dev`，其版本与设备端不同，未经测试 |
| `LIXEL_OPENAPI_SANITIZER` | 空、`address`、`thread`、`undefined` | 空 | 仅对本项目自己的目标启用 |
| `LIXEL_OPENAPI_BUILD_TESTS` | `ON`、`OFF` | `ON` | 是否构建单元测试 |

Ubuntu 22.04 自带 protobuf 3.12.4，用 `-DLIXEL_OPENAPI_PROTOBUF_PROVIDER=SYSTEM` 可以构建本项目（已验证），
但 `AUTO` 会改为构建固定版本。

## 目录

```
cpp/
├── CMakeLists.txt
├── cmake/                     # 依赖解析、proto 生成、sanitizer
├── include/lixel/openapi/     # 公共头文件（命名空间 lixel::openapi）
├── src/
└── tests/                     # GoogleTest 单元测试，不需要设备和 broker
```

protobuf 消息的 C++ 源码在构建时生成到构建目录，不入库，因为它依赖构建时所用的 protobuf 版本。
