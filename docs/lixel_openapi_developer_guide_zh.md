# Lixel OpenAPI 二次开发指南

本手册面向使用 Lixel 设备 OpenAPI（基于 MQTT + Protobuf）进行二次开发的
开发者，介绍协议架构、连接与鉴权、消息编解码、全部接口清单，以及本项目
提供的可复用客户端封装、GUI 控制面板与用例脚本。

- 协议：MQTT / MQTTS
- 负载：Protobuf 二进制（心跳除外，为 JSON 文本）
- Proto 包名：`openapi.aiot_msgs`
- Proto 源文件：`proto/openapi/aiot_msgs.proto`

> 本手册的英文版见 `docs/lixel_openapi_developer_guide_en.md`。

---

## 目录

1. [总体架构](#1-总体架构)
2. [连接与鉴权](#2-连接与鉴权)
3. [消息模型与编解码](#3-消息模型与编解码)
4. [接口清单](#4-接口清单)
5. [core 客户端封装](#5-core-客户端封装)
6. [GUI 控制面板](#6-gui-控制面板)
7. [用例脚本](#7-用例脚本)
8. [本地化（i18n）](#8-本地化i18n)
9. [常见问题](#9-常见问题)

---

## 1. 总体架构

设备与客户端之间通过 MQTT Broker（运行于设备端）通信。所有业务消息以
Protobuf 序列化后作为 MQTT 负载收发。Topic 采用三段式命名：

```
lixel/<domain>/<type>/<name>
```

- `domain`：业务域，本协议主要为 `application`
- `type`：消息方向
  - `request`  客户端 → 设备（请求）
  - `response` 设备 → 客户端（对请求的应答）
  - `report`   设备 → 客户端（主动上报）
- `name`：具体功能，如 `device_info`、`modeling`

典型交互模式有两种：

- **请求-响应**：客户端向 `.../request/xxx` 发布请求，订阅 `.../response/xxx`
  等待应答。
- **订阅上报**：客户端订阅 `.../report/xxx`，被动接收设备主动推送的数据。

```
┌────────────┐   publish request    ┌──────────────┐
│            │ ───────────────────► │              │
│  客户端     │                      │   设备        │
│ (本项目)    │ ◄─────────────────── │ (MQTT Broker)│
│            │   response / report  │              │
└────────────┘                      └──────────────┘
```

---

## 2. 连接与鉴权

### 2.1 连接参数

不同产品型号仅支持其中一种连接方式（非同时支持两种）：

| 协议 | 端口 | 用户名 | 密码 | 产品型号 |
|------|------|--------|------|----------|
| MQTT | 1883 | - | - | K1/L2PRO |
| MQTTS | 8883 | *（由厂商提供）* | *（由厂商提供）* | 除 K1/L2PRO 外的其他产品 |

> K1/L2PRO 仅支持 MQTT，其他产品仅支持 MQTTS；每个产品只支持一种协议。

> MQTTS 使用 TLS，本项目示例默认关闭证书校验（`CERT_NONE` + `tls_insecure`），
> 适配设备端自签名证书。GUI 选择 MQTTS 时需要手动填写厂商提供的用户名和密码。

### 2.2 OpenAPI Key（可选）

**自 2.0.0 起，已获得 OpenAPI 授权（`active`，已授权）的设备不需要 OpenAPI Key。** 保持
`openapi_key` 为空（默认值）即可，客户端不会发送 Key。授权由 XGRIDS 授予，本 SDK 不能授予授权。

仍使用早期 Key 方案的设备（1.x 版本所用的方案）行为不变：每个请求的 `Header` 里必须带有效的
`openapi_key`，该 Key 由 XGRIDS 提供。有效 Key 在其他各类设备上同样可用，所以同一个程序可以同时面对
这几类设备，只需对需要 Key 的设备设置 Key。

| 设备 | 不带 Key（或 Key 错误） | 带有效 Key |
|------|--------------------------|------------|
| 已授权（`active`） | 正常 | 正常（不计量） |
| 授权已用完（`expired`） | `EC_ProtoError`，`desc` 为 `openapi license exhausted` | 正常 |
| 支持授权但未授权（`none`） | `EC_ProtoError`，`desc` 为 `openapi auth error` | 正常 |
| 早期 Key 方案（心跳里没有 `Features`） | `EC_ProtoError`，`desc` 为 `openapi auth error` | 正常 |

Key 错误与不带 Key 等同。以上每种 `EC_ProtoError` 情况下，设备都会返回该错误，而不是保持沉默，所以
这不是超时。

**怎么判断设备属于哪一种？** 看心跳（`lixel/application/report/heartbeat`）。`Features` 里含
`"openapi_license"`，表示固件支持 OpenAPI 授权，此时 `OpenapiLicenseState` 为 `none`、`active` 或
`expired`（设备暂时读不到自己的授权状态时，这个字段会被省略）。心跳没有 `Features`，或者 `Features`
数组里没有 `"openapi_license"`，就是早期 Key 方案的设备。`ProtocolVersion` 仍为 `"3.0.0"`，不能用来
区分。详见协议参考 3.1 节。

- GUI：顶部连接栏的"OpenAPI Key（可选）"输入框。已获得 OpenAPI 授权的设备留空即可。发布时"加载模板"
  会把已填写的 Key 写入 `header.openapi_key`；没有填写时不写入任何内容。
- 代码：仅在需要 Key 时才为 `ClientOptions.openapi_key` 赋值，`request()` / `publish()` 会自动
  把它填入 `header.openapi_key`。

```python
from example.core import ClientOptions, OpenApiClient

# 已获得 OpenAPI 授权的设备：不需要 Key
with OpenApiClient(ClientOptions(host="192.168.55.1")) as client:
    ...

# 早期 Key 方案的设备，或授权缺失、已用完的设备：设置 Key
opts = ClientOptions(host="192.168.55.1", openapi_key="<由 XGRIDS 提供>")
with OpenApiClient(opts) as client:
    ...  # header.openapi_key 会被自动填充
```

**免 Key 的接口。** 在授权状态为 `active` 的设备上，第 4 节的 13 个请求/响应接口都不需要 Key。
Lixel L2Pro 没有 `data_unpack` 主题（带不带 Key 都没有响应），所以在 L2Pro 上免 Key 可用的是其余
12 个接口。

**计量。** 只有不带 Key 发起的录制才会计量：不带 Key 的 `modeling` 请求（`AC_START`）开始的录制，
会消耗授权的录制分钟；带有效 Key 开始的录制不消耗。每满 60 秒消耗 1 分钟，录制结束时不足 60 秒的
部分再消耗 1 分钟（150 秒的录制消耗 3 分钟）。分钟用完时，设备按正常停止流程停止这次录制，已录制的
数据照常保存，授权状态随即变为 `expired`。此后不带 Key 的请求会返回 `openapi license exhausted`，带有效
Key 的请求仍然可用。本 SDK 不能授予或安装授权；授权状态从心跳读取。

**从 1.x 升级。** 为 1.x 编写的程序无需修改：带有效 Key 的请求与 1.x 的处理完全相同，也不计量。要
免 Key，设备必须已经获得 OpenAPI 授权，然后不再传 Key 即可（去掉 `openapi_key=` / `--key`）。

### 2.3 Header 通用字段

所有请求/响应/上报消息的第一个字段通常为 `Header`：

| 字段 | 类型 | 说明 |
|------|------|------|
| `seq` | uint32 | 帧序号 |
| `stamp` | sint64 | 帧时间戳 |
| `scaler` | sint64 | 缩放系数（点云等使用） |
| `device_id` | string | 设备 ID（设备生成） |
| `session_id` | string | 会话 ID（请求/上报端生成） |
| `openapi_key` | string | API 密钥（可选；已获得 OpenAPI 授权的设备不需要） |

---

## 3. 消息模型与编解码

### 3.1 Proto 编译

使用项目自带脚本，将 `proto/openapi/aiot_msgs.proto` 编译为 Python 模块
`python/src/proto_gen/openapi/aiot_msgs_pb2.py`：

```bash
python python/src/generate_proto.py
```

> 生成使用当前环境的 `grpc_tools.protoc`，确保 gencode 版本与运行时
> protobuf 版本一致。运行时建议设置
> `PROTOCOL_BUFFERS_PYTHON_IMPLEMENTATION=python`（本项目已默认设置），
> 以避免不同环境下 upb C 扩展不兼容。

### 3.2 JSON ↔ Protobuf

本项目 core 提供 `ProtoCodec`，基于 `google.protobuf.json_format`，
在 dict/JSON 与 protobuf 二进制之间转换：

```python
from example.core import ProtoCodec

codec = ProtoCodec()

# 编码：dict → protobuf 二进制
data = codec.encode("DeviceControlRequest", {"action": "AC_SYSTEM_REBOOT"})

# 解码：protobuf 二进制 → dict
result = codec.decode("DeviceControlResponse", data)

# 生成默认模板（含所有字段）
template = codec.default_json("ModelingRequest")
```

要点：

- 枚举字段可直接用字符串名（如 `"AC_START"`）或整数值。
- `bytes` 字段在 dict 中表现为 base64 字符串。
- 字段名保持 proto 定义的 snake_case（`preserving_proto_field_name=True`）。

---

## 4. 接口清单

以下接口严格对应 `aiot_msgs.proto`。方向标记：`REQ`=请求，`RSP`=响应，
`RPT`=上报。所有 topic 前缀均为 `lixel/application/`。

### 4.1 状态监控

| 接口 | 方向 | topic（省略前缀） | 消息类型 | 说明 |
|------|------|------|----------|------|
| 设备心跳 | RPT | report/heartbeat | *(JSON)* | 设备广播心跳，用于发现 |
| 设备状态 | RPT | report/device_status | DeviceStatusReport | 1Hz 上报建图/系统/RTK 状态 |
| 实时位姿 | RPT | report/lio_pose | LioPoseReport | 扫描中的实时 LIO 位姿 |
| 实时点云 | RPT | report/lio_pcl | MqttCompressMsg | 压缩封装的实时点云 |
| 系统错误 | RPT | report/system_error | SystemErrorReport | 系统错误码上报 |

心跳负载为 JSON 文本，例如：

```json
{"DeviceSN":"A256A60000","Timestamp":1697703246.5,"ProtocolVersion":"3.0.0","Features":["openapi_license"],"OpenapiLicenseState":"active","OpenapiLicenseText":"Authorized"}
```

`Features`、`OpenapiLicenseState`、`OpenapiLicenseText` 三个字段由支持 OpenAPI 授权的固件加入，详见 2.2 节与协议参考 3.1 节。

实时点云 `MqttCompressMsg` 需二次解压解码：

1. 反序列化 `MqttCompressMsg`，得 `compression`（Lz4/Zstd）与 `compressed_data`
2. 按压缩类型解压 `compressed_data`。Lz4 为 LZ4 block 格式（不是 LZ4 frame 格式），
   解压需要原始长度，即 `compressed_size`（压缩前的数据大小）
3. 将解压结果按 `LioPclReport` 反序列化得到点云数组

### 4.2 设备管理

| 接口 | 方向 | topic（省略前缀） | 消息类型 |
|------|------|------|----------|
| 查询设备信息 | REQ / RSP | request/device_info · response/device_info | DeviceInfoRequest / DeviceInfoResponse |
| 设备参数配置 | REQ / RSP | request·response/device_config | DeviceConfigRequest / DeviceConfigResponse（proto 注释提到 `report/device_config`，但没有定义 `DeviceConfigReport` 消息，设备端也没有这个 topic，不要订阅） |
| 设备控制 | REQ / RSP | request/device_control · response/device_control | DeviceControlRequest / DeviceControlResponse |

设备控制支持的 `ActionCmd`：`AC_USB_MASS`、`AC_USB_ETHER`、`AC_DISK_FORMAT`、
`AC_POWER_OFF`、`AC_SYSTEM_REBOOT`。

### 4.3 扫描建图

| 接口 | 方向 | topic（省略前缀） | 消息类型 |
|------|------|------|----------|
| 扫描控制 | REQ / RSP / RPT | request·response·report/modeling | ModelingRequest / ModelingResponse / ModelingReport |
| 当前工程文件地址查询 | REQ / RSP | request/modeling_status · response/modeling_status | ModelingStatusRequest / ModelingStatusResponse |

扫描 `action` 支持 `AC_START` / `AC_STOP`。`ModelingRequest` 关键字段：
`project_name`、`record_mode`（RM_CALC/RM_RECORD/RM_RECORD_CALC）、
`scan_mode`（SM_PCL/SM_LCC/SM_PORTRAIT）、`mount_type`（MT_HAND/MT_CAR/MT_UAV/MT_BACKPACK）、
`assist_mode`（AM_GUIDED/AM_VISUAL）。

### 4.4 点位管理

| 接口 | 方向 | topic（省略前缀） | 消息类型 |
|------|------|------|----------|
| 控制点操作 | REQ / RSP / RPT | request·response·report/control_point | ControlPointRequest / ControlPointResponse / ControlPointReport |
| 测量点操作 | REQ / RSP | request/measure_point · response/measure_point | MeasurePointRequest / MeasurePointResponse |

控制点 `action` 支持 `AC_ADD` / `AC_REMOVE`。proto 中定义了 `AC_UPDATE`，设备目前不处理，会返回 `EC_ProtoError`（`desc` 为 `invalid action`）。
待机时设备不回复控制点请求。`AC_ADD` 忽略请求里的 `id`，响应里的 `id` 由设备分配，删除时使用这个值；
`AC_ADD` 成功时 `error.desc` 是该点的位姿（7 个浮点数，空格分隔），不是文字描述。
测量点 `action` 仅支持 `AC_ADD` / `AC_REMOVE`。

> ⚠️ **测量点功能仅部分设备支持**，请确认目标设备是否具备该能力。

### 4.5 工程管理

| 接口 | 方向 | topic（省略前缀） | 消息类型 |
|------|------|------|----------|
| 工程列表操作 | REQ / RSP | request/project_list · response/project_list | ProjectListRequest / ProjectListResponse |

`action` 支持 `AC_LIST_ALL_PROJECTS`（列出）、`AC_MODIFY_PROJECT_LABELS`
（改标签，需 `id`+`tag_name`）、`AC_DELETE_PROJECTS`（删除，需 `id_list`）。

### 4.6 RTK / Ntrip

| 接口 | 方向 | topic（省略前缀） | 消息类型 |
|------|------|------|----------|
| 获取 Ntrip 配置 | REQ / RSP | request/get_ntrip_profile · response/get_ntrip_profile | GetNtripProfileRequest / GetNtripProfileResponse |
| 设置 Ntrip 配置 | REQ / RSP | request/set_ntrip_profile · response/set_ntrip_profile | SetNtripProfileRequest / SetNtripProfileResponse |
| 获取 RTK 高级配置 | REQ / RSP | request/get_rtk_advance · response/get_rtk_advance | GetRtkAdvanceRequest / GetRtkAdvanceResponse |
| 设置 RTK 高级配置 | REQ / RSP | request/set_rtk_advance · response/set_rtk_advance | SetRtkAdvanceRequest / SetRtkAdvanceResponse |

设置 Ntrip `action` 支持 `AC_LOGIN` / `AC_LOGOUT`；CORS 类型
`CUSTOM/QIANXUN/ZHONGYI`；坐标系 `UNKNOWN/ITRF2008/WGS84/CGCS2000`。
RTK 卫星系统支持 `GPS/BDS/GLO/GAL/QZSS/IRNSS`。

> ⚠️ **`SetNtripProfileRequest` 特别说明**：仅当 RTK 状态处于 `HwConnected`
> 时才允许设置。当设备 WiFi 处于 AP 模式时（此时 Ntrip 数据通过 WebSocket
> 转发），向设备发送该请求，Response 会返回错误码 `AppCode::EMBError::Unknown`，
> 用户可忽略该错误码，设备的 Ntrip 连接不会受到任何影响。

### 4.7 数据处理

| 接口 | 方向 | topic（省略前缀） | 消息类型 |
|------|------|------|----------|
| 数据解包 | REQ / RSP / RPT | request·response·report/data_unpack | DataUnpackRequest / DataUnpackResponse / DataUnpackReport |

`data_source` 目前支持 `XBIN_DATA`；不指定 `data_topics` 则解包全部；
`report/data_unpack` 上报解包进度（0~100）。

> ⚠️ **数据处理（解包）功能仅部分设备支持**，请确认目标设备是否具备该能力。

### 4.8 错误码约定

请求-响应消息的 `Error error = 15` 字段承载错误信息（`code` + `desc`）。
常见错误码：`EC_RequestSuccess`、`EC_RequestError`、`EC_ProtoError`；
Ntrip 相关：`EC_NtripLoginSuccess`、`EC_NtripLogoutSuccess`、
`EC_NtripLogoutFailed`、`EC_NtripDisConnected`、`EC_NtripNetworkError`、
`EC_NtripAuthError`。

---

## 5. core 客户端封装

`python/example/core` 提供不依赖 Qt 的可复用封装，是二次开发的推荐入口。

### 5.1 模块组成

| 模块 | 说明 |
|------|------|
| `api_catalog.py` | 接口清单单一数据源（`ApiInterface` / `Endpoint` / `Direction`） |
| `proto_codec.py` | `ProtoCodec`：JSON ↔ protobuf 编解码、默认模板 |
| `openapi_client.py` | `OpenApiClient` / `ClientOptions`：MQTT 连接、请求-响应、订阅 |

### 5.2 ClientOptions

```python
from example.core import ClientOptions

opts = ClientOptions(
    host="192.168.55.1",
    port=1883,
    use_tls=False,          # MQTTS 时置 True
    username="",
    password="",
    openapi_key="",         # 可选；设置后自动写入 header.openapi_key
    device_id="",
    connect_timeout=10.0,
    request_timeout=10.0,
)
```

### 5.3 请求-响应

```python
from example.core import OpenApiClient, ClientOptions

with OpenApiClient(ClientOptions(host="192.168.55.1")) as client:
    rsp = client.request(
        req_topic="lixel/application/request/device_info",
        req_message="DeviceInfoRequest",
        payload={},                       # dict 或 JSON 字符串
        rsp_topic="lixel/application/response/device_info",
        rsp_message="DeviceInfoResponse",
    )
    print(rsp)                            # 解码后的 dict
```

`request()` 内部：先订阅响应 topic → 发布请求 → 阻塞等待第一条响应 →
解码返回；超时抛出 `TimeoutError`。

### 5.4 订阅上报

```python
def on_status(topic, data):   # data 已解码为 dict
    print(topic, data)

client.subscribe(
    "lixel/application/report/device_status",
    "DeviceStatusReport",
    on_status,
)
```

对 JSON 负载（心跳），`message` 传空字符串，回调收到字符串。

### 5.5 API 速览

| 方法 | 说明 |
|------|------|
| `connect()` / `disconnect()` | 连接 / 断开（支持 `with` 上下文） |
| `is_connected` | 连接状态 |
| `publish(topic, message, payload)` | 发布一条消息 |
| `subscribe(topic, message, callback)` | 订阅并注册回调 |
| `unsubscribe(topic)` | 取消订阅 |
| `request(...)` | 请求-响应同步调用 |
| `codec` | 内置 `ProtoCodec` |

---

## 6. GUI 控制面板

`python/example/main.py` 提供图形化控制面板，按功能分组用选项卡展示全部接口，
每个接口一张卡片，可编辑 JSON 负载发送请求、或订阅上报输出。

```bash
python python/example/main.py                 # 默认英文界面
python python/example/main.py --lang zh       # 启动即中文（也可在菜单实时切换）
```

界面结构：

- 菜单栏：
  - 设置 → 语言：中英文实时切换（**默认英文**），无需重启
  - 关于：显示构建时注入的程序版本号（从源码直接运行时为 `V0.0.0`）
- 顶部连接栏：地址 / 端口 / 协议(MQTT·MQTTS) / 用户名 / 密码 / OpenAPI Key（可选） / 连接
  - 默认地址为 `192.168.55.1`
  - 用户名 / 密码仅在选择 MQTTS 协议时显示，需手动填写厂商提供的账号密码
    （切回 MQTT 时隐藏并清空）
- 左侧"发布消息"面板（与 OpenAPI-XPanel一致）：
  - Topic 下拉（可编辑，预填所有可发布 request topic）
  - 消息类型下拉（仅列出 Request 类型）+ 加载模板（仅支持请求类型消息）
  - 接口说明区：展示所选 topic 的功能说明、补充说明，并提示需在右侧订阅的响应 topic
  - JSON 编辑框 + 发布按钮
  - 选择 Topic 时自动联动消息类型；"加载模板"生成默认 JSON 并注入 header
    （seq/stamp/device_id/session_id + openapi_key）
- 右侧"订阅消息"面板（与 OpenAPI-XPanel一致）：
  - Topic 下拉（可编辑）+ 订阅 / 订阅全部
  - 通配符订阅（默认 `lixel/#`）
  - 当前订阅列表 + 取消选中 / 全部取消
  - 收到的消息（摘要列表，自动滚动）+ 查看详情 / 清空，双击可看完整解码内容

> 发布仅发送请求，不阻塞界面；**响应与上报统一在右侧"订阅消息"面板查看**。
> 因此发布请求前，请先在订阅面板订阅对应的响应 topic（或用通配符 `lixel/#`）。
> 面板按 topic 自动匹配消息类型解码；未知类型或 JSON 负载（心跳）以文本展示，
> 二进制以 HEX 摘要展示，长字段自动省略。

---

## 7. 用例脚本

`python/example/usage/` 下为每个接口提供独立可运行脚本，是最直接的二次开发参考。
全部基于 `OpenApiClient`，不依赖 GUI。

```bash
cd python/example/usage
python device_info.py --host 192.168.55.1 --port 1883
python modeling.py --project demo --watch 5
python heartbeat.py --seconds 15
```

通用参数：`--host/--port/--tls/--username/--password/--key/--timeout`。`--key` 可选：已获得
OpenAPI 授权的设备不需要。其他设备会拒绝没有有效 Key 的请求并返回 `EC_ProtoError`（`openapi auth error`，
授权已用完时为 `openapi license exhausted`），对这类设备请加上 `--key <你的 OpenAPI Key>`。`modeling.py` 会等设备真正进入扫描中再开始计 `--watch`，
停止后还会等设备回到待机，所以一次运行需要一分钟以上（Lixel L2Pro 上 `--watch 10` 实测 71 秒）。
脚本清单见 `python/example/usage/README.md`。

> `lio_pcl.py` 演示点云解压，需额外安装 `pip install lz4 zstandard`；
> 未安装时脚本会打印提示并跳过点云解码。

---

## 8. 本地化（i18n）

GUI 中所有用户可见的英文源文本均通过 Qt 的 `self.tr()` 包裹，翻译存放于
`python/example/i18n/openapi_zh.ts`（源语言 en_US，目标 zh_CN）。

```bash
# 从源码提取待翻译字符串（更新 .ts）
bash python/example/i18n/build_i18n.sh update

# 编译 .ts 为 .qm
bash python/example/i18n/build_i18n.sh release
```

底层调用系统 Qt 工具 `lupdate` / `lrelease`。界面**默认英文**，可在"设置 →
语言"菜单实时切换（无需重启）；也可用 `--lang <code>` 在启动时指定：

```
python/example/i18n/openapi_<lang>.qm
```

新增语言只需：复制 `.ts` 并翻译 → `lrelease` 编译 → 在 `control_panel.py`
的 `LANGUAGES` 中登记 → 菜单即出现该语言。

---

## 9. 常见问题

**Q：连接超时？**
检查设备地址/端口、协议是否匹配（1883=MQTT，8883=MQTTS），MQTTS 需带
用户名密码。core 客户端 `connect()` 超时会抛 `TimeoutError`。

**Q：请求无响应/超时？**
确认请求 topic 与消息类型匹配、设备处于可响应状态。注意：部分请求在设备待机时不会被回复（如 `modeling_status`、`control_point`）。没有有效 OpenAPI 授权的设备在 `openapi_key` 缺失或不正确时会返回 `EC_ProtoError`，不属于超时：早期 Key 方案的设备和未授权的设备，`desc` 为 `openapi auth error`；授权的录制分钟已用完时，`desc` 为 `openapi license exhausted`。见 2.2 节。
`request()` 默认 10s 超时，可用 `--timeout` 或 `request_timeout` 调整。

**Q：protobuf 版本报错？**
运行 `python python/src/generate_proto.py` 重新生成，保持 gencode 与运行时版本一致；
本项目默认使用纯 Python 实现规避 C 扩展不兼容。

**Q：点云解码失败？**
安装解压依赖 `pip install lz4 zstandard`，并确认设备处于扫描状态才有点云上报。
Lz4 数据是 LZ4 block 格式，需用 `lz4.block.decompress(data, uncompressed_size=compressed_size)`；
用 `lz4.frame` 解压会报 `ERROR_frameType_unknown`。

**Q：枚举字段怎么填？**
可用枚举名字符串（推荐，如 `"AC_START"`）或整数值。

---

*本手册与代码同步维护，接口清单以 `proto/openapi/aiot_msgs.proto` 为准。*
