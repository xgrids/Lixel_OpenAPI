# Lixel OpenAPI — C++ 客户端

通过 OpenAPI 与 Lixel 设备通信的 C++ 方式。Python 方式（示例脚本、可复用客户端库）
在 [../python](../python/README.zh-CN.md)，OpenAPI-XPanel 图形界面在 [../xpanel](../xpanel/README.zh-CN.md)，
仓库总览见 [../README.zh-CN.md](../README.zh-CN.md)。
两种方式使用**同一套 MQTT 主题和同一份 protobuf 定义**
（[`../proto/openapi/aiot_msgs.proto`](../proto/openapi/aiot_msgs.proto)）。

> English version: [README.md](README.md)。

## 当前状态
**2.0.0（开发中，尚未发布），仅支持 Linux。** 最近一个已发布版本是 1.1.x。客户端库和 18 个命令行示例已实现：MQTT/MQTTS 连接、请求/响应、订阅、JSON 转换、
点云解码、扫描流程。目前没有 `install` 和 CMake 包导出，使用方式见下文的 `add_subdirectory`。

已验证与未验证。下表是 1.1.x 时留下的记录。对 2.0.0 只重复了构建和完整的 `ctest`（包括现在也覆盖
不需要 Key 的设备的 `broker_integration` 和 `examples_e2e`）；没有重做 AddressSanitizer 运行和真机测试。

| 已验证 | 方式 |
|--------|------|
| 库逻辑，91 个用例 | 假传输层的单元测试（含压力测试），同时在 AddressSanitizer 下运行过 |
| MQTT 层，明文与 TLS | 对临时启动的 Mosquitto broker 测试 |
| 18 个示例 | 对由 Python 客户端一侧扮演的假设备做端到端测试，明文与 TLS，包含退出码与 Ctrl-C |
| 真机 | 一台 Lixel L2Pro：只读示例、一次完整扫描（期间解码 42 帧点云，无错误）、一次被中断的扫描 |

| 未验证 | |
|--------|---|
| 真机上的 MQTTS | 测试用的 L2Pro 没有 MQTTS 端口 |
| 其他设备型号、Windows、macOS、ThreadSanitizer | 开发期间没有条件 |

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

需要 broker 的测试（`broker_integration`）会在 `127.0.0.1` 的空闲端口上临时启动 Mosquitto，需要 `mosquitto`、
`mosquitto_passwd` 和 `openssl`，缺任何一个就不注册该测试。仓库里不存放任何密钥或密码：证书和账号每次运行时现生成。

首次配置会从源码构建 protobuf。在验证用的机器（Ubuntu 22.04）上，20 路并行的全新构建耗时 47 秒。

### 选项

| 选项 | 取值 | 默认 | 含义 |
|------|------|------|------|
| `LIXEL_OPENAPI_PROTOBUF_PROVIDER` | `AUTO`、`SYSTEM`、`FETCH` | `AUTO` | `AUTO`：系统 protobuf 为 3.21.0 及以上时使用系统版本，否则构建固定版本。`SYSTEM`：始终用系统版本，低于 3.21.12 会给出警告，低于 3.15 会自动加 proto3 `optional` 的实验开关。`FETCH`：始终构建固定版本 |
| `LIXEL_OPENAPI_PAHO_PROVIDER` | `FETCH`、`SYSTEM` | `FETCH` | `SYSTEM` 使用 `libpaho-mqtt-dev`，其版本与设备端不同，未经测试 |
| `LIXEL_OPENAPI_SANITIZER` | 空、`address`、`thread`、`undefined` | 空 | 仅对本项目自己的目标启用 |
| `LIXEL_OPENAPI_BUILD_TESTS` | `ON`、`OFF` | 作为顶层工程时为 `ON` | 是否构建测试 |
| `LIXEL_OPENAPI_BUILD_EXAMPLES` | `ON`、`OFF` | 作为顶层工程时为 `ON` | 是否构建 18 个示例 |
| `LIXEL_E2E_PYTHON` | 路径 | 空 | 装有 `paho-mqtt`、`protobuf`、`grpcio-tools`、`lz4` 的 Python（`pip install -r python/requirements.txt lz4`）；设置后注册 `examples_e2e` 测试，它首次运行时会生成 Python 消息模块 |

Ubuntu 22.04 自带 protobuf 3.12.4，用 `-DLIXEL_OPENAPI_PROTOBUF_PROVIDER=SYSTEM` 可以构建本项目（已验证），
但 `AUTO` 会改为构建固定版本。

## 使用这个库

加入你的 CMake 工程即可（protobuf 和 paho 由本目录的 CMake 自行拉取）：

```cmake
set(LIXEL_OPENAPI_BUILD_TESTS OFF)
set(LIXEL_OPENAPI_BUILD_EXAMPLES OFF)
add_subdirectory(path/to/Lixel_OpenAPI/cpp lixel)
target_link_libraries(my_app PRIVATE LixelOpenApi::lixel_openapi)
```

连接、发请求、读回答：

```cpp
#include "lixel/openapi/client.h"
#include "openapi/aiot_msgs.pb.h"

lixel::openapi::ClientOptions options;
options.transport.host = "192.168.55.1";
// options.openapi_key = "<你的 Key>";  // 仅早期 Key 方案的设备需要
lixel::openapi::Client client(options);
if (!client.Connect().ok()) return 1;

openapi::aiot_msgs::DeviceInfoRequest request;
openapi::aiot_msgs::DeviceInfoResponse response;
lixel::openapi::Result r = client.Request("lixel/application/request/device_info", request,
                                          "lixel/application/response/device_info", &response);
// r.ok()：设备回了成功码。Errc::DeviceError：设备回了错误码（r.device_code、r.desc），
// response 已填充。Errc::RequestTimeout：没有回答。
```

| 头文件 | 提供什么 |
|--------|----------|
| `client.h` | `Client`：`Connect`、`Request`、`Publish`、`Subscribe`、`SubscribeMessage`、`RemoveSubscription` |
| `scan.h` | `RunScan`：开始扫描、等设备真正在扫、停止、等回到待机 |
| `pointcloud.h` | `DecodePointCloud`：`lio_pcl` 负载转为带缩放的点 |
| `codec.h` | 按名字取消息，protobuf 与 JSON 互转 |
| `catalog.h` | 18 个接口、34 个主题的表 |
| `codes.h`、`result.h` | 设备状态码，`Result` 与 `Errc` |

与 Python 客户端有意不同的地方：

- 响应按 `header.seq` 匹配请求（设备会把它原样带回）。Python 客户端取响应主题上的第一条消息。
- JSON 转换是严格的：字段名或枚举名写错会报错。Python 的编解码器会静默丢掉。
- MQTTS 默认校验服务端证书。设备证书是自签名的，所以要传 `ca_file`（示例里是 `--ca-file`），或关闭校验
  （`--insecure`）。Python 客户端从不校验。
- Python 目录表里的 `report/device_config` 主题没有使用：设备上没有这个主题。
- 在订阅回调里调用 `Request` 会返回 `Errc::CalledFromCallback`，而不是阻塞。

使用限制：

- 同一进程里的多个 `Client` 共用 MQTT 库的一个工作线程，该线程每秒醒一次：建立第二个连接约需 2 秒
  （实测 2.10 s）。每个示例都只有一个 `Client`，不受影响。
- 正在等待的 `Request` 无法取消，最迟在 `request_timeout` 到时结束。
- Zstd 压缩的点云返回 `Errc::Unsupported`，负的 `header.scaler` 也是。

## 示例

`examples/` 里每个 `python/example/usage/` 脚本都有一个同名、同选项的程序。随库一起构建
（`-DLIXEL_OPENAPI_BUILD_EXAMPLES=ON`，默认开），产物在 `cpp/build/examples/`。

```bash
./cpp/build/examples/heartbeat --seconds 5
./cpp/build/examples/device_info
./cpp/build/examples/modeling --project demo --watch 8
./cpp/build/examples/lio_pcl --seconds 20                       # 设备扫描期间
./cpp/build/examples/device_info --key <你的 Key>               # 仍使用早期 Key 方案的设备
```

通用选项：`--host`（默认 `192.168.55.1`）、`--port`、`--tls`、`--username`、`--password`、`--key`（可选，
设备已获得 OpenAPI 授权时不需要）、`--timeout`，TLS 另有 `--ca-file` 和 `--insecure`（Python 脚本里没有）。其余见 `--help`。退出码：0 成功，
1 连不上或没有回答，2 选项错误（`modeling`：设备不在待机），3 设备回了错误码（回答照常打印），130 被中断。
`modeling` 还有更多退出码，写在它的源码头部。

18 个示例：`control_point`、`data_unpack`、`device_config`、`device_control`、`device_info`、
`device_status`、`get_ntrip_profile`、`get_rtk_advance`、`heartbeat`、`lio_pcl`、`lio_pose`、
`measure_point`、`modeling`、`modeling_status`、`project_list`、`set_ntrip_profile`、
`set_rtk_advance`、`system_error`。其中 `device_control`、`device_config`、`set_ntrip_profile`、
`set_rtk_advance`、`control_point`、`measure_point` 会改动设备，`device_control` 可以关机或格式化磁盘。

## 测试

| 程序 | 用例数 | 覆盖内容 |
|------|-------:|----------|
| `test_smoke` | 7 | 构建链路，线上编码与 Python 客户端一致 |
| `test_catalog_codec` | 13 | 接口表、JSON 转换、状态码 |
| `test_codec_standalone` | 2 | 不包含任何生成头文件的程序也能按名字找到消息 |
| `test_client`、`test_client_stress` | 27、2 | 请求/响应、订阅、重连、线程安全 |
| `test_pointcloud` | 16 | Python 一侧生成的帧、损坏和随机输入 |
| `test_scan` | 24 | 对脚本化设备的扫描流程 |
| `test_broker`（`broker_integration`） | 19 | MQTT 层对 Mosquitto，明文与 TLS |
| `tests/e2e/run_examples.py`（`examples_e2e`） | 49 | 18 个示例对假设备 |
| `catalog_drift` | 1 | C++ 接口表与 Python 目录一致（`tools/catalog_tool.py check`） |

## 目录

```
cpp/
├── CMakeLists.txt
├── cmake/                     # 依赖解析、proto 生成、sanitizer
├── include/lixel/openapi/     # 公共头文件（命名空间 lixel::openapi）
├── src/
├── examples/                  # 18 个命令行示例及其公共辅助代码
├── tools/catalog_tool.py      # 生成并检查接口表
└── tests/                     # GoogleTest 测试；broker/ 与 e2e/ 放需要 broker 的脚本
```

protobuf 消息的 C++ 源码在构建时生成到构建目录，不入库，因为它依赖构建时所用的 protobuf 版本。
