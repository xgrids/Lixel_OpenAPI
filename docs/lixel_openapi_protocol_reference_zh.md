# Lixel OpenAPI 协议参考

## 1. 概述

**Lixel 系列**是一系列创新型手持三维扫描设备。**Lixel OpenAPI** 是 Lixel 设备软件提供的一组通信协议接口。通过该通信接口，用户可以方便地控制 Lixel 设备并实时获取设备数据。基于该接口，用户可以开发与 Lixel 设备集成的应用程序。本文档详细描述了 Lixel OpenAPI 的通信方式与协议。

## 2. 通信方式

### 2.1 本地通信

Lixel 设备内置 MQTT broker 与 RTSP 服务。实时控制与实时点云数据通过 MQTT 传输，实时视频通过 RTSP 传输。Lixel 设备支持与其他设备的有线与无线组网：有线方式使用 USB 线缆连接，无线方式使用 Wi-Fi 组网。

> 💡 **MQTT 连接参数**
> - MQTT 版本：V3.1.1
> - MQTT 地址：`mqtt://192.168.55.1`（有线组网使用固定 IP；无线组网时通过蓝牙 GATT 服务获取 IP）
> - MQTT 端口：1883

```text
        Lixel device                              App
  +--------------------+                  +--------------------+
  |    MQTT Broker     | ---- MQTT -----> |  command / data    |
  |    RTSP-server     | ---- RTSP -----> |  real time video   |
  +--------------------+                  +--------------------+
```

#### 2.1.1 USB 连接

> 💡 运行 Linux、Windows 或 Android 的设备使用 RNDIS 与 Lixel 设备进行网络共享。
> iOS 设备使用 USB Tethering（USB 网络共享）与 Lixel 设备进行网络共享。

```text
  Lixel device (192.168.55.1)                     other device
      +---------+                                    +---------+
      |   USB   | <----- RNDIS / USB Tethering ----> |   USB   |
      +---------+                                    +---------+
```

#### 2.1.2 Wi-Fi 连接

> 💡 Lixel 设备支持三种无线连接模式：**普通模式**（其他设备作为热点）、**快连模式**（Lixel 设备作为热点）以及**桥接模式**（使用路由器）。所有模式均需通过蓝牙进行网络配置。Lixel 设备接受蓝牙连接以配置 Wi-Fi，使设备能够连接到指定的网络。
>
> Lixel 设备的蓝牙命名规则由 10 个字符组成：以固定前缀 **"XGR-"** 开头，后接 SN 码的后 6 位。例如：**"XGR-A12345"**。

```text
      Lixel device                                 other device
      +---------+                                      +---------+
      |   BT    | <----- Bluetooth (network config) -> |   BT    |
      |  WiFi   | <----- WiFi (data) ----------------> |  WiFi   |
      +---------+                                      +---------+
```

蓝牙服务定义：

```cpp
/* Android GATT Characteristic Types */
#define UUID_GATT 0x1801
#define GATT_CHARAC_WIFI_INFO_SET 0x2AB0
#define GATT_CHARAC_WIFI_STATUS_GET 0x2AB1

/* iOS GATT Characteristic Types */
#define IOS_UUID_GATT 0x7F00
#define IOS_GATT_CHARAC_WIFI_INFO_SET 0x7F01
#define IOS_GATT_CHARAC_WIFI_STATUS_GET 0x7F02

#pragma pack(1)
/***********************************************************
 * WiFi status information (device -> Android)
 * ssid_len : 0 means WiFi is disconnected
 * ssid     : WiFi access point name
 * ip_len   : 0 means WiFi has no assigned IP
 * ip       : WiFi IP address
 * auth_mode: Not used yet
 * wifi_mode: 0 - STA mode  1 - AP mode (Quick Connect mode) 255 - ignore WiFi mode
 **********************************************************/
typedef struct wifi_info_s {
    uint8 ssid_len;
    uint8 ssid[32];
    uint8 passwd_len;
    uint8 passwd[64];
    uint8 auth_mode;
    uint8 wifi_mode;        // 0: Normal mode 1: Quick Connect mode 255: Only configure country code
    uint8 country_code[2];  // Country code (ISO2)
} wifi_info_t;

typedef struct wifi_status_s {
    uint8 ssid_len;
    uint8 ssid[32];
    uint8 ip_len;
    uint8 ip[16];
    uint8 connect_state;    // 0 - Not connected 1 - Connected successfully 2 - Connecting 3 - Device scanning
    uint8 wifi_mode;        // 0: Normal mode 1: Quick Connect mode
    uint8 country_code[2];  // Country code (ISO2)
} wifi_status_t;
#pragma pack()
```

Lixel 设备与 APP 之间的 BLE 网络配置流程如下：

```text
        Lixel device                                    APP
  +----------------------+                     +----------------------+
  | BLE set to slave mode|                     | BLE set to master mode|
  +----------+-----------+                     +----------+-----------+
             |                                            |
             v                                            v
  +----------------------+                     +----------------------+
  | start broadcast BLE  |                     |      search BLE      |
  +----------+-----------+                     +----------+-----------+
             |                                            |
             v            <---- connecting... ---->       v
  +----------------------+                     +----------------------+
  |     wait connect     |                     |     BLE connect      |
  +----------+-----------+                     +----------+-----------+
             |                                            |
             |  <-------- BLE connect success --------    |
             v                                            v
  +----------------------+   BLE connect success  +----------------------+
  |   send wifi status   | ---------------------> |   get wifi status    |
  +----------+-----------+                     +----+------------+------+
             |                                      |            |
             |                        wifi connect  |            | wifi
             |                              failed   v           | connected
             |                     +----------------------+      |
             |   <---- wifi info --|    send wifi info     |     |
             v                     +----------+-----------+      |
  +----------------------+                     |                 |
  |    get wifi info     |                     v                 v
  +----------+-----------+                +----------------------+
             |                            |         end          |
             v                            +----------------------+
  +----------------------+
  |     connect wifi     |
  +----------+-----------+
             |
             v
  +----------------------+
  |         end          |
  +----------------------+
```

### 2.2 通信认证

OpenAPI 的消息只有在设备认证通过后才会被响应。认证密钥须向 XGRIDS 申请，且 MQTT 请求消息头中必须包含认证密钥（`openapi_key`）。

```protobuf
message Header {
    uint32 seq = 1;   // Frame sequence number
    sint64 stamp = 2; // Frame timestamp
    sint64 scaler = 3; // Scaling factor (positive: magnification, negative: reduction, 0: disable scaling)
    string device_id = 4;            // Device ID (generated by the device)
    string session_id = 5;           // Session ID (generated by the requester/reporter)
    optional string openapi_key = 6; // API key (must be requested from XGRIDS)
}
```

## 3. 实时控制协议（MQTT）

### 3.1 心跳上报

> ⚠️ **注意：** 心跳消息是 OpenAPI 中唯一 payload 采用 JSON 格式的消息。通过订阅该消息，可获取设备使用的协议版本，从而进行协议兼容性设计。

| 字段 | 值 |
|-------|-------|
| 功能 | 周期性发布心跳（1Hz） |
| MQTT 属性 | QoS: 0；发布者：Lixel 设备；订阅者：APP |
| Topic | `lixel/application/report/heartbeat` |

Payload（JSON）：

```json
{
    "DeviceSN":"A241A60001",
    "Timestamp":1697703246.500000,
    "ProtocolVersion":"3.0.0"
}
```

说明：
- `DeviceSN`：设备序列号
- `Timestamp`：设备当前时间
- `ProtocolVersion`：协议版本号

### 3.2 设备状态上报

| 字段 | 值 |
|-------|-------|
| 功能 | 周期性发布设备状态（1 Hz） |
| MQTT 属性 | QoS: 0；发布者：Lixel 设备；订阅者：APP |
| Topic | `lixel/application/report/device_status` |
| Payload | `openapi.aiot_msgs.DeviceStatusReport` |

```protobuf
message DeviceStatusReport {
    message DeviceStatus {
        sint32 app_sessions = 1; // Number of APP sessions
    }
    Header header = 1;
    uint32 modeling_state = 2; // Unified status code
    string device_sn = 3;      // Device serial number
    string project_id = 4;     // Project name
    string time_zone = 5;      // Current time zone
    bool init_ready = 6;       // 0: Not initialized, 1: Initialized
    SystemStatus system_status = 7;
    RtkStatus rtk_status = 8;
    DeviceStatus device_status = 9;
}
```

说明 —— `modeling_state` 枚举：`EC_UnknownState`、`EC_ScanReady`、`EC_ScanStarting`、`EC_ScanModeling`、`EC_ScanStopping`、`EC_ScanStopped`、`EC_ScanMappingAbort`、`EC_ScanNotAllowedInUsbMass`、`EC_ScanProjectTransferring`。

### 3.3 设备信息获取

**设备信息请求（Device Info Request）** —— QoS: 0；发布者：APP；订阅者：Lixel 设备
Topic：`lixel/application/request/device_info`
Payload：`openapi.aiot_msgs.DeviceInfoRequest`

```protobuf
message DeviceInfoRequest {
    Header header = 1;
}
```

**设备信息响应（Device Info Response）** —— QoS: 0；发布者：Lixel 设备；订阅者：APP
Topic：`lixel/application/response/device_info`
Payload：`openapi.aiot_msgs.DeviceInfoResponse`

```protobuf
message DeviceInfoResponse {
    Header header = 1;
    DeviceInfo device_info = 2;
    CumulativeInfo cumulative_info = 3;
    Error error = 15;
}
```

### 3.4 设备配置

**设备配置请求（Device Config Request）** —— QoS: 0；发布者：APP；订阅者：Lixel 设备
Topic：`lixel/application/request/device_config`
Payload：`openapi.aiot_msgs.DeviceConfigRequest`

```protobuf
message DeviceConfigRequest {
    message TimeConfig {
        sint64 time_secs = 1; // Timestamp (seconds precision)
        string time_zone = 2; // Time zone (e.g. "Asia/Shanghai")
    }
    message MainStorageConfig {
        sint32 storage_type = 1; // 0: internal storage, 1: TF card, 2: USB Disk
    }
    Header header = 1;
    optional TimeConfig time_config = 4;
    optional MainStorageConfig main_storage_config = 8;
}
```

**设备配置响应（Device Config Response）** —— QoS: 0；发布者：Lixel 设备；订阅者：APP
Topic：`lixel/application/response/device_config`
Payload：`openapi.aiot_msgs.DeviceConfigResponse`

```protobuf
message DeviceConfigResponse {
    Header header = 1;
    // Possible error codes: EC_RequestSuccess, EC_RequestError, EC_ProtoError
    Error error = 15;
}
```

### 3.5 设备控制

**设备控制请求（Device Control Request）** —— QoS: 0；发布者：APP；订阅者：Lixel 设备
Topic：`lixel/application/request/device_control`
Payload：`openapi.aiot_msgs.DeviceControlRequest`

```protobuf
message DeviceControlRequest {
    Header header = 1;
    Application.ActionCmd action = 2;
}
```

**设备控制响应（Device Control Response）** —— QoS: 0；发布者：Lixel 设备；订阅者：APP
Topic：`lixel/application/response/device_control`
Payload：`openapi.aiot_msgs.DeviceControlResponse`

```protobuf
message DeviceControlResponse {
    Header header = 1;
    Application.ActionCmd action = 2;
    Error error = 15;
}
```

### 3.6 扫描控制

**扫描控制请求（Scan control request）** —— QoS: 0；发布者：APP；订阅者：Lixel 设备
Topic：`lixel/application/request/modeling`
Payload：`openapi.aiot_msgs.ModelingRequest`

```protobuf
message ModelingRequest {
    Header header = 1;
    Application.ActionCmd action = 2; // Supports AC_START / AC_STOP
    string project_name = 3;          // Custom project name
    Modeling.RecordMode record_mode = 4;
    Modeling.ScanMode scan_mode = 5;
    Modeling.MountType mount_type = 6;
    string pre_project_id = 7; // Previous project ID (continued-scan feature in later versions)
    Modeling.AssistMode assist_mode = 8; // Scan assist mode
}
```

**扫描控制响应（Scan control response）** —— QoS: 0；发布者：Lixel 设备；订阅者：APP
Topic：`lixel/application/response/modeling`
Payload：`openapi.aiot_msgs.ModelingResponse`

```protobuf
message ModelingResponse {
    Header header = 1;
    Application.ActionCmd action = 2;
    Error error = 15;
}
```

**扫描状态上报（Scan status report）** —— QoS: 0；发布者：Lixel 设备；订阅者：APP
Topic：`lixel/application/report/modeling`
Payload：`openapi.aiot_msgs.ModelingReport`

```protobuf
message ModelingReport {
    message ScanStatus {
        float move_distance = 1; // Travel distance
        float move_speed = 2;    // Movement speed
        sint64 scan_time = 3;    // Scan time (unit: seconds)
    }
    Header header = 1;
    sint32 pgo_progress = 2; // Data processing progress after scan finished (0~100)
    ScanStatus scan_status = 3;
}
```

### 3.7 异常上报

**异常上报（Anomaly report）** —— QoS: 0；发布者：Lixel 设备；订阅者：APP
Topic：`lixel/application/report/system_error`
Payload：`openapi.aiot_msgs.SystemErrorReport`

```protobuf
message SystemErrorReport {
    Header header = 1;
    Error error = 15;
}
```

### 3.8 数据解包

> ⚠️ **注意：** 数据解包功能仅由**特定型号、特定固件版本的设备**支持。使用前，请通过心跳上报（`ProtocolVersion`）或设备信息请求（`DeviceInfoResponse.device_info.base_info`）查询设备固件版本，并向 XGRIDS 确认目标型号/固件是否支持该功能。在不支持的型号或固件版本上，该请求将不会生效。

**数据解包请求（Data unpack request）** —— QoS: 0；发布者：APP；订阅者：Lixel 设备
Topic：`lixel/application/request/data_unpack`
Payload：`openapi.aiot_msgs.DataUnpackRequest`

```protobuf
message DataUnpackRequest {
    enum DataSource {
        XBIN_DATA = 0; // xbin data
    }
    Header header = 1;
    string project_name = 2;            // Project name
    DataSource data_source = 3;         // Data source
    repeated string data_topics = 4;    // Data topics (if unspecified, unpack all data by default)
    optional string data_ouputdir = 5;  // Data output directory name (relative to the project directory)
}
```

**数据解包响应（Data unpack response）** —— QoS: 0；发布者：Lixel 设备；订阅者：APP
Topic：`lixel/application/response/data_unpack`
Payload：`openapi.aiot_msgs.DataUnpackResponse`

```protobuf
message DataUnpackResponse {
    Header header = 1;
    Error error = 15;
}
```

**数据解包进度上报（Data unpack progress report）** —— QoS: 0；发布者：Lixel 设备；订阅者：APP
Topic：`lixel/application/report/data_unpack`
Payload：`openapi.aiot_msgs.DataUnpackReport`

```protobuf
message DataUnpackReport {
    Header header = 1;
    sint32 progress = 2; // Unpack progress (0~100)
    Error error = 15;    // Unpack status
}
```

## 4. 实时数据传输（MQTT）

### 4.1 实时点云上传

**实时点云上传（Real-time point cloud upload）** —— QoS: 0；发布者：Lixel 设备；订阅者：APP
Topic：`lixel/application/report/lio_pcl`
Payload：`openapi.aiot_msgs.MqttCompressMsg`

```protobuf
message LioPclReport {
    message Point {
        sint64 x = 1;    // val = raw value * header.scaler
        sint64 y = 2;    // val = raw value * header.scaler
        sint64 z = 3;    // val = raw value * header.scaler
        uint32 rgbi = 4; // R + G + B + Intensity
    }
    Header header = 1;
    repeated Point points = 2; // Point cloud data
}
message MqttCompressMsg {
    enum CompressionType {
        CompressionLz4 = 0;
        CompressionZstd = 1;
    }
    Header header = 1;
    CompressionType compression = 2; // Compression type
    uint32 compressed_size = 3; // Data size before compression (serialized LioPclReport size)
    bytes compressed_data = 4; // Compressed data (compressed serialized LioPclReport)
}
```

> ⚠️ 为减小传输数据的大小，点云数据使用 LZ4 进行压缩。压缩格式为 LZ4 block（不是 LZ4 frame），
> 解压时需要原始长度，即 `compressed_size`（压缩前的数据大小）。

### 4.2 实时位姿上传

**实时位姿上传（Real-time pose upload）** —— QoS: 0；发布者：Lixel 设备；订阅者：APP
Topic：`lixel/application/report/lio_pose`
Payload：`openapi.aiot_msgs.LioPoseReport`

```protobuf
message LioPoseReport {
    Header header = 1;
    PoseStamped pose = 2;
    float distance = 3;      // Distance (meters)
    float pose_accuracy = 4; // Not used yet
}
```

## 5. 实时视频流（RTSP）

Lixel 设备内置 RTSP 服务器。当设备成功开始扫描后，会实时推送相机视频流，同一时刻最多推送一路相机视频流。视频流地址格式如下：

> **`rtsp://<lixel_device_ipaddr>:8554/live/chn_<camera_name>`**
>
> 示例：
> - `rtsp://192.168.55.1:8554/live/chn_left`
> - `rtsp://192.168.55.1:8554/live/chn_right`

| 参数 | 说明 |
|-----------|-------------|
| `lixel_device_ipaddr` | Lixel 设备的 IP 地址：有线连接固定为 `192.168.55.1`；无线连接需通过蓝牙获取 Wi-Fi 地址；快连（闪连）模式下固定为 `192.168.56.1` |
| `camera_name` | 相机名称。须通过 MQTT 发送设备信息请求（`DeviceInfoRequest`）获取：`DeviceInfoResponse.camera_info.camera_list` |

## 6. 云控功能（MQTT）

**暂未提供。**

## 7. 项目文件访问功能（Samba）

文件访问功能（Samba）默认关闭。须先通过设备控制协议（`action: AC_START_FS`）启用后方可访问。

| 参数 | 值 | 备注 |
|-----------|-------|---------|
| 服务地址 | `smb://<device_ip>/share` | `device_ip` 与 MQTT 连接地址一致 |
| 用户名 | `admin` | 默认，不可修改 |
| 密码 | `admin123` | 默认，不可修改 |

## 8. 错误码定义

```cpp
enum ErrorCode: uint32 {
        // General status codes
        EC_UnknownState = 0x12040000,       // NOLINT: Unknown state (0x12040000)
        EC_Success,                         // NOLINT: Success (0x12040000+1)
        EC_SuccessAndReboot,                // NOLINT: Success and reboot (0x12040000+2)
        EC_RequestSuccess = EC_Success,     // NOLINT: Request successful (0x12040000+1)

        // General error codes
        EC_UnknownError = 0x32040000,       // NOLINT: Unknown error (0x32040000)
        EC_ProtoError,                      // NOLINT: Protocol error (0x32040000+1)
        EC_RequestError,                    // NOLINT: Request error (0x32040000+2)

        // Upgrade status codes
        EC_UpgradeOnGoing = 0x12040000+200,     // NOLINT: Upgrade in progress (0x12040000+200)
        EC_UpgradeFileNotExist,                 // NOLINT: File does not exist (0x12040000+201)
        EC_UpgradeFileBroken,                   // NOLINT: File is corrupted (0x12040000+202)
        EC_UpgradeProtoError,                   // NOLINT: Protocol error (0x12040000+203)
        EC_UpgradeFailed,                       // NOLINT: Upgrade failed (0x12040000+204)
        EC_UpgradeSucceed,                      // NOLINT: Upgrade succeeded (0x12040000+205)
        EC_UpgradeLowBattery,                   // NOLINT: Battery level too low (0x12040000+206)
        EC_UpgradeInternalError,                // NOLINT: Internal error (0x12040000+207)

        // Activation status codes
        EC_Activated = 0x12040000+500,          // NOLINT: Activated (0x12040000+500)
        EC_Deactivated,                         // NOLINT: Not activated (0x12040000+501)

        // Ntrip status codes
        EC_NtripLoginSuccess = EC_Success,      // NOLINT: Ntrip login successful (0x12040000+1)
        EC_NtripLogoutSuccess = EC_Success,     // NOLINT: Ntrip logout successful (0x12040000+1)
        EC_NtripDisConnected = 0x12040000+401,  // NOLINT: Ntrip service disconnected (0x12040000+401)

        // Ntrip error codes
        EC_NtripLogoutFailed = EC_RequestError, // NOLINT: Ntrip logout failed (0x32040000+2)
        EC_NtripNetworkError,                   // NOLINT: Ntrip network error (0x32040000+3)
        EC_NtripAuthError,                      // NOLINT: Ntrip authentication failed (0x32040000+4)
        EC_NtripTimeoutError,                   // NOLINT: Ntrip execution timeout error (0x32040000+5)

        // Udisk error codes
        EC_UdiskNotMounted = 0x32040000+11,     // NOLINT: External storage not mounted (0x32040000+11)
        EC_UdiskStorageLow,                     // NOLINT: External storage space insufficient (0x32040000+12)
        EC_UdiskNoProject,                      // NOLINT: No project to transfer (0x32040000+13)
        EC_UdiskTransferError,                  // NOLINT: External storage transfer error (0x32040000+14)
        EC_UdiskLowBattery,                     // NOLINT: Insufficient battery for external storage transfer (0x32040000+15)

        // Device exception codes
        EC_ExBatteryLow = 0x32010000+0x1001,    // NOLINT: Low battery level (0x32010000+0x1001)
        EC_ExStorageLow,                        // NOLINT: Low storage capacity (0x32010000+0x1002)
        EC_ExImuAbnormal,                       // NOLINT: IMU abnormal (0x32010000+0x1003)
        EC_ExImuHalt,                           // NOLINT: IMU stopped working (0x32010000+0x1004)
        EC_ExEncoderAbnormal,                   // NOLINT: Encoder abnormal (0x32010000+0x1005)
        EC_ExEncoderHalt,                       // NOLINT: Encoder stopped working (0x32010000+0x1006)
        EC_ExCamera0Abnormal,                   // NOLINT: Camera 0 abnormal (0x32010000+0x1007)
        EC_ExCamera0Halt,                       // NOLINT: Camera 0 stopped working (0x32010000+0x1008)
        EC_ExCamera1Abnormal,                   // NOLINT: Camera 1 abnormal (0x32010000+0x1009)
        EC_ExCamera1Halt,                       // NOLINT: Camera 1 stopped working (0x32010000+0x100A)
        EC_ExCamera3Abnormal,                   // NOLINT: Camera 3 abnormal (0x32010000+0x100B)
        EC_ExCamera3Halt,                       // NOLINT: Camera 3 stopped working (0x32010000+0x100C)
        EC_ExCamera2Abnormal,                   // NOLINT: Camera 2 abnormal (0x32010000+0x100D)
        EC_ExCamera2Halt,                       // NOLINT: Camera 2 stopped working (0x32010000+0x100E)
        EC_ExBatteryEmpty,                      // NOLINT: Battery depleted (0x32010000+0x100F)
        EC_ExStorageEmpty,                      // NOLINT: Storage depleted (0x32010000+0x1010)
        EC_ExLidarAbnormal,                     // NOLINT: LiDAR abnormal (0x32010000+0x1011)
        EC_ExLidarHalt,                         // NOLINT: LiDAR stopped working (0x32010000+0x1012)
        EC_ExCpuTempHigh,                       // NOLINT: CPU temperature too high (0x32010000+0x1013)
        EC_ExSlipRingOverLife,                  // NOLINT: Slip ring service life expired (0x32010000+0x1014)
        EC_ExHardwareFault,                     // NOLINT: Hardware fault (0x32010000+0x1015)
        EC_ExMagneticAbnormal,                  // NOLINT: Magnetic sensor abnormal (0x32010000+0x1016)
        EC_ExMagneticHalt,                      // NOLINT: Magnetometer data stopped (0x32010000+0x1017)
        EC_ExSlamStopFailed,                    // NOLINT: Algorithm stop exception (0x32010000+0x1018)
        EC_ExBatteryTempHigh,                   // NOLINT: Battery temperature too high (0x32010000+0x1019)
        EC_ExUdiskLow,                          // NOLINT: External storage capacity low (0x32010000+0x101A)
        EC_ExUdiskEmpty,                        // NOLINT: External storage capacity depleted (0x32010000+0x101B)
        EC_ExUdiskRemove,                       // NOLINT: External storage removed (0x32010000+0x101C)

        // Scan status codes
        EC_ScanReady = 0x12040000+300,          // NOLINT: Standby (0x12040000+300)
        EC_ScanStarting,                        // NOLINT: Starting scan (0x12040000+301)
        EC_ScanModeling,                        // NOLINT: Scanning (0x12040000+302)
        EC_ScanStopping,                        // NOLINT: Stopping scan (0x12040000+303)
        EC_ScanStopped,                         // NOLINT: Scan finished (0x12040000+304)

        // Scan error codes
        EC_ScanMountError = 0x32040000+305,     // NOLINT: Mount error (0x32040000+305)
        EC_ScanStorageError,                    // NOLINT: Storage error (0x32040000+306)
        EC_ScanSlamError,                       // NOLINT: Mapping error (0x32040000+307)
        EC_ScanPreIDError,                      // NOLINT: Pre-project error (0x32040000+308)
        EC_ScanStorageEmpty,                    // NOLINT: Disk capacity empty (0x32040000+309)
        EC_ScanCameraError,                     // NOLINT: Camera error (0x32040000+310)
        EC_ScanRelocSuccess,                    // NOLINT: Relocation successful (0x32040000+311)
        EC_ScanRelocFailed,                     // NOLINT: Relocation failed (0x32040000+312)
        EC_ScanSlamAbort,                       // NOLINT: Algorithm aborted (0x32040000+313)
        EC_ScanStorageLow,                      // NOLINT: Insufficient disk space (0x32040000+314)
        EC_ScanMemoryLow,                       // NOLINT: Insufficient memory (0x32040000+315)
        EC_ScanLidarError,                      // NOLINT: LiDAR data abnormal (0x32040000+316)

        // Calibration error codes
        EC_CalibFailed = 0x32050000+1,          // NOLINT: Calibration failed (0x32050000+1)
        EC_CalibMagRtkError,                    // NOLINT: Magnetometer calibration RTK state error (0x32050000+2)
        EC_CalibMagLackDataError,               // NOLINT: Magnetometer calibration data acquisition abnormal (0x32050000+3)
        EC_CalibFileNotExists,                  // NOLINT: File does not exist
        EC_CalibFileNotPermitted,               // NOLINT: File operation not permitted

        // Scan warning codes
        EC_ScanMappingAbort=0x22040000+317,     // NOLINT: Mapping aborted, packet recording only (0x22040000+317)
        EC_ScanNotAllowedInUsbMass,             // NOLINT: Scan not allowed in USB mass storage mode (0x22040000+318)
        EC_ScanProjectTransferring,             // NOLINT: Scan not allowed during project transfer (0x22040000+319)
        EC_ScanPclRefineError,                  // NOLINT: PCL refine failed (0x22040000+320)
        EC_ScanPcbinDataError,                  // NOLINT: Pcbin data abnormal (will cause LAS file generation failure) (0x22040000+321)
        EC_ScanXbinDataError,                   // NOLINT: Xbin data abnormal (will cause post-processing failure) (0x22040000+322)
    };
```

## 9. 消息定义（proto）

`lixel_openapi/aiot_msgs.proto`

```protobuf
syntax = "proto3";
package openapi.aiot_msgs;
// clang-format off
message Header {
    uint32 seq = 1;   // frame sequence number
    sint64 stamp = 2; // frame timestamp
    sint64 scaler = 3; // scale factor (positive: magnify, negative: shrink, 0: disable scaling)
    string device_id = 4;            // device ID (generated by the device)
    string session_id = 5;           // session ID (generated by the request/report side)
    optional string openapi_key = 6; // API key (apply from XGRIDS)
}

message Error {
    uint32 code = 1; // error code
    string desc = 2; // descriptor
}

message Point {
    double x = 1;
    double y = 2;
    double z = 3;
}

message Quaternion {
    double x = 1;
    double y = 2;
    double z = 3;
    double w = 4;
}

message Pose {
    Point position = 1;
    Quaternion orientation = 2;
}

message PoseStamped {
    sint64 stamp = 1;
    Pose pose = 2;
}

message Application {
    enum ActionCmd {
        AC_UNKNOW = 0;                 // unknown
        AC_START = 1;                  // start
        AC_STOP = 2;                   // stop
        AC_PAUSE = 3;                  // pause
        AC_RESUME = 4;                 // resume
        AC_OPEN = 5;                   // open
        AC_CLOSE = 6;                  // close
        AC_START_FS = 7;               // enable file sharing
        AC_STOP_FS = 8;                // disable file sharing
        AC_ADD = 10;                   // add
        AC_REMOVE = 11;                // remove (delete)
        AC_UPDATE = 12;                // modify (update)
        AC_LOGIN = 13;                 // log in
        AC_LOGOUT = 14;                // log out
        AC_USB_MASS = 15;              // USB mass storage
        AC_USB_ETHER = 16;             // USB ethernet
        AC_DISK_FORMAT = 17;           // format disk
        AC_USER_RESET = 18;            // user reset (clear user data)
        AC_POWER_OFF = 20;             // power off
        AC_SYSTEM_REBOOT = 22;         // reboot
        AC_LIST_ALL_PROJECTS = 23;     // list all projects
        AC_MODIFY_PROJECT_LABELS = 24; // modify project labels
        AC_DELETE_PROJECTS = 25;       // delete projects
        AC_UDISK_FORMAT = 29;          // format external USB disk
    }
}

message Modeling {
    enum ScanMode {
        SM_PCL = 0;      // point cloud mode
        SM_LCC = 1;      // LCC mode
        SM_PORTRAIT = 2; // portrait mode
    }
    enum RecordMode {
        RM_CALC = 0;        // compute only (no recording)
        RM_RECORD = 1;      // record only (no algorithm)
        RM_RECORD_CALC = 2; // record + compute
    }
    enum MountType {
        MT_HAND = 0;     // default handheld
        MT_CAR = 1;      // vehicle-mounted
        MT_UAV = 2;      // airborne (DJI)
        MT_BACKPACK = 3; // backpack
    }
    enum AssistMode {   // scan assist mode
        AM_GUIDED = 0;  // guided assist mode
        AM_VISUAL = 1;  // visual (RGB) mode
    }
}

message UdiskInfo {
    enum MountedState {
        Unknown = 0;     // not mounted
        Mounted = 1;     // mounted
        FsTypeError = 2; // filesystem type error, not exfat
        UsbIdError = 3;  // USB pid/vid verification failed
        InFormat = 4;    // formatting
    }
    MountedState state = 1; // mount state
    bool rw_status = 2;     // disk read/write status
    sint64 total = 3;       // unit: KB
    sint64 used = 4;        // unit: KB
    bool project_use = 5;   // used by the current project during recording
}

message BatteryInfo{
    enum PowerSupply {
        Battery = 0; // battery power
        DCPower = 1; // external power
    }
    PowerSupply power_type = 1;   // power type
    sint32 level = 2;             // battery level: 0~100
    float temp = 3;               // battery temperature, Celsius
    float voltage = 4;            // battery voltage
}

message SystemStatus {
    enum PowerSupply {
        Battery = 0; // battery power
        DCPower = 1; // external power
    }
    repeated sint32 cpu_loading = 1; // 0~100
    sint64 mem_total = 2;            // unit: KB
    sint64 mem_used = 3;             // unit: KB
    sint64 disk_total = 4;           // unit: KB
    sint64 disk_used = 5;            // unit: KB
    PowerSupply power_type = 6;      // power type
    sint32 battery_level = 7;        // battery level: 0~100
    bool udisk_mounted = 8;          // whether the USB disk is mounted
    sint64 udisk_total = 9;          // unit: KB
    sint64 udisk_used = 10;          // unit: KB
    bool udisk_rw_status = 11;       // USB disk read/write status
    UdiskInfo udisk = 12;            // new status, backward compatible
    BatteryInfo battery_info = 13;   // new status, backward compatible
}

// Rtk attributes
message RtkAttr {
    enum Type {
        RTK = 0;
        PPK = 1;
        DJI = 2;
    }

    enum State {
        HwDisconnected = 0; // hardware not connected
        HwConnected = 1;    // hardware connected (Ntrip not connected)
        NtripConnected = 2; // Ntrip connected (hardware connected)
    }
    // Message defining RTK bracket parameters (deprecated [20250528])
    message BracketParams {
        bool customized = 1;       // whether customized
        repeated float xyz = 2;    // offset relative to the IMU
        repeated float degree = 3; // angle relative to the IMU
    }
}

message RtkStatus {
    enum NavSatState {
        Invalid = 0;     // positioning unavailable or invalid
        SinglePoint = 1; // single-point positioning
        SBAS = 2;        // pseudorange differential or SBAS positioning
        Fixed = 4;       // RTK fixed solution
        Float = 5;       // RTK float solution
        INS = 6;         // inertial navigation positioning
        UserFixed = 7;   // user-set position (FixedPosition)
    };
    RtkAttr.Type type = 1;
    RtkAttr.State state = 2;
    NavSatState navsat_state = 3; // satellite state
    sint32 navsat_nums = 4;       // number of satellites
    bool auth_expired = 5;        // RTK authorization expired (scanning forbidden after expiry, L2PS1 only)
    string register_status = 7;   // "YES": permanent auth, "NO": unregistered, "2025-01-01": expiry date
}

/******************************************************************************
 | type | topic                                | payload
 | RPT  | lixel/application/report/heartbeat   | JSON
 * notes:
 * Used for device heartbeat broadcast (payload is a JSON string)
    {
        "DeviceSN":"A256A60000",
        "Timestamp":1697703246.500000,
        "ProtocolVersion":"3.0.0"
    }
 *****************************************************************************/

/******************************************************************************
 | type | topic                                      | payload
 | RPT  | lixel/application/report/device_status     | DeviceStatusReport
 * notes:
 * Device status info (report frequency 1Hz)
 * Possible values of modeling_state:
    EC_UnknownState = 0x12040000,       // unknown state (0x12040000)
    EC_ScanReady = 0x12040000+300,      // standby (0x12040000+300)
    EC_ScanStarting,                    // starting scan (0x12040000+301)
    EC_ScanModeling,                    // scanning (0x12040000+302)
    EC_ScanStopping,                    // stopping scan (0x12040000+303)
    EC_ScanStopped,                     // scan finished (0x12040000+304)
    EC_ScanMappingAbort=0x22040000+317, // mapping aborted, record only (0x22040000+317)
    EC_ScanNotAllowedInUsbMass,         // scanning forbidden in USB mass mode (0x22040000+318)
    EC_ScanProjectTransferring,         // scanning forbidden during project transfer (0x22040000+319)
 *****************************************************************************/
message DeviceStatusReport {
    message DeviceStatus {
        sint32 app_sessions = 1; // number of APP sessions
    }
    Header header = 1;
    uint32 modeling_state = 2; // uses the unified status code
    string device_sn = 3;      // device serial number
    string project_id = 4;     // project name
    string time_zone = 5;      // current time zone
    bool init_ready = 6;       // 0: not initialized, 1: initialized
    SystemStatus system_status = 7;
    RtkStatus rtk_status = 8;
    DeviceStatus device_status = 9;
}

/******************************************************************************
 | type | topic                                    | payload
 | REQ  | lixel/application/request/device_info    | DeviceInfoRequest
 | RSP  | lixel/application/response/device_info   | DeviceInfoResponse
 * notes:
 * Query device info
 *****************************************************************************/

message DeviceInfoRequest {
    Header header = 1;
}

message DeviceInfo {
    // Activation info
    message ActivationInfo {
        string user_name = 1;
        string uuid = 2;
        string activation_time = 3;
    }
    // Base info
    message BaseInfo {
        string version_time = 1;
        string software_version = 2;
        string system_version = 3;
        string algorithm_version = 4;
        string mcu_version = 5;
        string device_model = 6;
        string device_sn = 7;
        string device_type = 8;
        sint32 motor_spins = 9;
        string agent_id = 10;
        // Index matches the ScanMode enum values: ["PCL","LCC"]; empty string for unsupported types.
        repeated string scan_mode_list = 11;
        Modeling.ScanMode scan_mode = 12;
        repeated string assist_mode_list = 13;
        Modeling.AssistMode assist_mode = 14;
        string gui_version = 15;
        string bsp_version = 16;
        string motor_version = 17;
    }
    message CameraInfo {
        sint32 bitrate_mode = 1; // 0:6Mbps 1:24Mbps 2:36Mbps 3:48Mbps 4:96Mbps
        repeated string camera_list = 2;
        repeated sint32 camera_bitrate = 3;
    }

    message LidarInfo {
        string lidar_type = 1;    // lidar type
        string lidar_version = 2; // firmware version
        sint32 return_mode = 3;   // echo mode
        string lidar_sn = 4;      // lidar serial number
    }

    message RtkInfo {
        // Index matches the RtkAttr.Type enum values: ["RTK","PPK","DJI"]; empty string for unsupported types.
        repeated string rtk_type_list = 1;
        string version = 2;         // current firmware version
        string upgrade_version = 3; // upgradable firmware version
    }

    message StatisticInfo {
        double area = 1;
        sint32 count = 2;
        double distance = 3;
        sint32 time = 4;
        sint32 lidar_time = 5;
    }

    message WorkingStatus {
        bool is_activated = 1;    // whether activated
        bool is_mag_recalib = 2;  // whether magnetometer recalibration is needed
        bool is_rtk_working = 3;  // whether RTK is working normally
        bool is_usb_massmode = 4; // whether in USB mass mode
        bool is_use_mqttssl = 5;  // whether MQTT encryption is used
    }

    message WlanStatus {
        string ip_address = 1;   // IP address
        string mac_address = 2;  // MAC address
        string connect_ssid = 3; // current SSID
        string country_code = 4; // country code
        sint32 channel_used = 5; // current channel in use
        sint32 wifi_mode = 6;    // wifi mode (0: STA mode, 1: AP mode)
    }

    message UdiskInfo {
        bool rw_enable = 1; // external USB disk read/write enable.
    }
    // All storage-related config goes here; UdiskInfo will be deprecated!!!
    message StorageInfo {
        sint32 main_storage_type = 1; // main storage type (0: internal, 1: TF card, 2: USB)
    }

    ActivationInfo activation_info = 1;
    BaseInfo base_info = 2;
    CameraInfo camera_info = 3;
    LidarInfo lidar_info = 4;
    RtkInfo rtk_info = 5;
    StatisticInfo statistic_info = 6;
    WorkingStatus working_status = 7;
    WlanStatus wlan_status = 8;
    UdiskInfo udisk_info = 9;
    StorageInfo storage_info = 10;
}

// Cumulative data
message CumulativeInfo {
    sint32 cumulative_mileage = 1;
    sint32 cumulative_time = 2;
    sint32 cumulative_area = 3;
    sint32 cumulative_count = 4;
    sint32 battery_cycle = 5;
}

message DeviceInfoResponse {
    Header header = 1;
    DeviceInfo device_info = 2;
    CumulativeInfo cumulative_info = 3;
    Error error = 15;
}

/******************************************************************************
 | type | topic                                    | payload
 | REQ  | lixel/application/request/device_config  | DeviceConfigRequest
 | RSP  | lixel/application/response/device_config | DeviceConfigResponse
 | RPT  | lixel/application/report/device_config   | DeviceConfigReport
 * notes:
 * REQ configures device parameters; when parameters change the device reports them via RPT
 *****************************************************************************/
message DeviceConfigRequest {
    message TimeConfig {
        sint64 time_secs = 1; // timestamp (second precision)
        string time_zone = 2; // time zone (e.g. "Asia/Shanghai")
    }
    message MainStorageConfig {
        sint32 storage_type = 1; // 0: internal storage, 1: TF card, 2: USB Disk
    }
    Header header = 1;
    optional TimeConfig time_config = 4;
    optional MainStorageConfig main_storage_config = 8;
}

message DeviceConfigResponse {
    Header header = 1;
    // Possible error codes: EC_RequestSuccess, EC_RequestError, EC_ProtoError
    Error error = 15;
}

/******************************************************************************
 | type | topic                                       | payload
 | REQ  | lixel/application/request/device_control    | DeviceControlRequest
 | RSP  | lixel/application/response/device_control   | DeviceControlResponse
 * notes:
 * Device control request
 * Supported ActionCmd:
 * AC_USB_MASS,AC_USB_ETHER,AC_DISK_FORMAT,AC_POWER_OFF,AC_SYSTEM_REBOOT
 * Possible DeviceControlResponse error codes:
 * EC_RequestSuccess,EC_RequestError,EC_ProtoError
 *****************************************************************************/
message DeviceControlRequest {
    Header header = 1;
    Application.ActionCmd action = 2;
}

message DeviceControlResponse {
    Header header = 1;
    Application.ActionCmd action = 2;
    Error error = 15;
}

/******************************************************************************
 | type | topic                                    | payload
 | REQ  | lixel/application/request/modeling       | ModelingRequest
 | RSP  | lixel/application/response/modeling      | ModelingResponse
 | RPT  | lixel/application/report/modeling        | ModelingReport
 * notes:
 * Scan control & scan status report
 * Possible ModelingResponse error codes:
 * EC_RequestSuccess,EC_RequestError,EC_ProtoError
 *****************************************************************************/
message ModelingRequest {
    Header header = 1;
    Application.ActionCmd action = 2; // supports AC_START/AC_STOP
    string project_name = 3;          // custom project name
    Modeling.RecordMode record_mode = 4;
    Modeling.ScanMode scan_mode = 5;
    Modeling.MountType mount_type = 6;
    string pre_project_id = 7; // preceding project ID (continued-scan feature in later versions)
    Modeling.AssistMode assist_mode = 8; // scan assist mode
}

message ModelingResponse {
    Header header = 1;
    Application.ActionCmd action = 2;
    Error error = 15;
}

message ModelingReport {
    message ScanStatus {
        float move_distance = 1; // movement distance
        float move_speed = 2;    // movement speed
        sint64 scan_time = 3;    // scan time (unit: seconds)
    }
    Header header = 1;
    sint32 pgo_progress = 2; // data processing progress at scan end (0~100)
    ScanStatus scan_status = 3;
}

/******************************************************************************
 | type | topic                                         | payload
 | REQ  | lixel/application/request/modeling_status     | ModelingStatusRequest
 | RSP  | lixel/application/response/modeling_status    | ModelingStatusResponse
 * notes:
 * Scan status request
 *****************************************************************************/
message ModelingStatusRequest {
    Header header = 1;
}

message ModelingStatusResponse {
    Header header = 1;
    string project_id = 2;         // project ID
    string poses_url = 3;          // pose data
    string control_points_url = 4; // control point data
    string measure_points_url = 5; // measure point data
    string project_name = 6;       // project directory name
    Error error = 15;
}

/******************************************************************************
 | type | topic                                | payload
 | RPT  | lixel/application/report/lio_pose    | LioPoseReport
 * notes:
 * Realtime pose report
 *****************************************************************************/
message LioPoseReport {
    Header header = 1;
    PoseStamped pose = 2;
    float distance = 3;      // distance (unit: m)
    float pose_accuracy = 4; // not used yet
}

/******************************************************************************
 | type | topic                               | payload
 | RPT  | lixel/application/report/lio_pcl    | MqttCompressMsg
 * notes:
 * Realtime intensity point cloud or true-color point cloud report
 *****************************************************************************/
message LioPclReport {
    message Point {
        sint64 x = 1;    // val = raw value * header.scaler
        sint64 y = 2;    // val = raw value * header.scaler
        sint64 z = 3;    // val = raw value * header.scaler
        uint32 rgbi = 4; // R+G+B+Intensity
    }
    Header header = 1;
    repeated Point points = 2; // point cloud data
}

message MqttCompressMsg {
    enum CompressionType {
        CompressionLz4 = 0;
        CompressionZstd = 1;
    }
    Header header = 1;
    CompressionType compression = 2;// compression type
    uint32 compressed_size = 3;     // size before compression (serialized LioPclReport size)
    bytes compressed_data = 4;      // compressed data (compressed serialized LioPclReport)
}

/******************************************************************************
 | type | topic                                       | payload
 | REQ  | lixel/application/request/control_point     | ControlPointRequest
 | RSP  | lixel/application/response/control_point    | ControlPointResponse
 | RPT  | lixel/application/report/control_point      | ControlPointReport
 * notes:
 * Control point operations
 * Possible ControlPointResponse error codes:
 * EC_RequestSuccess,EC_RequestError,EC_ProtoError
 *****************************************************************************/
message ControlPointRequest {
    Header header = 1;
    Application.ActionCmd action = 2;   // supports only: AC_ADD,AC_REMOVE,AC_UPDATE
    sint32 id = 3;                      // control point ID
    sint32 type = 4;                    // control point type (0: device, 1: centering pole, 100: legacy)
    string label = 5;                   // control point label
    sint32 height = 6;                  // base point height (unit: mm)
}

message ControlPointResponse {
    Header header = 1;
    Application.ActionCmd action = 2;   // supports only: AC_ADD,AC_REMOVE,AC_UPDATE
    sint32 id = 3;                      // control point ID
    sint32 type = 4;                    // control point type (0: device, 1: centering pole, 100: legacy)
    string label = 5;                   // control point label
    sint32 height = 6;                  // base point height (unit: mm)
    Error error = 15;
}

// Report control point update (reported to the APP when marking manually via a button)
message ControlPointReport {
    Header header = 1;
    sint32 id = 2;     // sequence number (auto-increment)
    sint32 type = 3;   // control point type (0: device, 1: centering pole, 100: legacy)
    string label = 4;  // control point label
    sint32 height = 5; // base point height (unit: mm)
}

/******************************************************************************
 | type | topic                                           | payload
 | REQ  | lixel/application/request/measure_point         | MeasurePointRequest
 | RSP  | lixel/application/response/measure_point        | MeasurePointResponse
 * notes:
 * Measure point operations
 * Possible MeasurePointResponse error codes:
 * EC_RequestSuccess,EC_RequestError,EC_ProtoError
 *****************************************************************************/
message MeasurePointRequest {
    Header header = 1;
    Application.ActionCmd action = 2; // supports only: AC_ADD,AC_REMOVE
    optional sint32 id = 3;
    optional string label = 4;
}

message MeasurePointResponse {
    Header header = 1;
    Application.ActionCmd action = 2; // supports only: AC_ADD,AC_REMOVE
    sint32 id = 3;
    optional string label = 4;
    Error error = 15;
}

/**
 * topic: lixel/application/request/project_list
 * topic: lixel/application/response/project_list
 * notes: Project file operations
 */
/******************************************************************************
 | type | topic                                           | payload
 | REQ  | lixel/application/request/project_list          | ProjectListRequest
 | RSP  | lixel/application/response/project_list         | ProjectListResponse
 * notes:
 * Project list request
 * Possible ProjectListResponse error codes:
 * EC_RequestSuccess,EC_RequestError,EC_ProtoError
 *****************************************************************************/
message ProjectListRequest {
    Header header = 1;
    // supports only: AC_LIST_ALL_PROJECTS,AC_MODIFY_PROJECT_LABELS,AC_DELETE_PROJECTS
    Application.ActionCmd action = 2;
    optional string id = 3;
    optional string tag_name = 4;
    repeated string id_list = 5; // used to delete multiple projects at once
}

message ProjectInfo {
    string id = 1; // scan project ID
    string project_name = 2;
    sint64 project_create_time = 3;
    string poses_url = 4;
    string control_points_url = 5;
    string measure_points_url = 6;
    string preview_photo_url = 7; // preview image url, empty if none
    float move_distance = 8;      // movement distance, unit: m
    sint64 scan_time = 9;         // scan time, unit: s
    bool gps_working = 10;
    string potree_url = 11; // reserved Potree point cloud, empty if none
    string xbin_url = 12;   // xbin
    Modeling.ScanMode scan_mode = 13;
    bool udisk = 14;          // project on external storage
    uint64 project_size = 15; // project size, unit: KB
    string project_tree = 16; // project directory, stored as JSON
}

message ProjectListResponse {
    Header header = 1;
    Application.ActionCmd action = 2;
    optional string id = 3;
    repeated ProjectInfo project_list = 4;
    Error error = 15;
}

/******************************************************************************
 | type | topic                                              | payload
 | REQ  | lixel/application/request/get_ntrip_profile        | GetNtripProfileRequest
 | RSP  | lixel/application/response/get_ntrip_profile       | GetNtripProfileResponse
 * notes:
 * Get ntrip profile
 *****************************************************************************/
message NtripAttr {
    enum CorsType {
        CUSTOM = 0;  // custom CORS
        QIANXUN = 1; // Qianxun CORS
        ZHONGYI = 2; // China Mobile CORS
    }
    enum CoordinateFrame {
        UNKNOWN = 0; // unknown coordinate frame
        ITRF2008 = 1;
        WGS84 = 2;
        CGCS2000 = 3;
    }
}

message GetNtripProfileRequest {
    Header header = 1;
}

message GetNtripProfileResponse {
    Header header = 1;
    NtripAttr.CorsType cors = 2;         // CORS type
    NtripAttr.CoordinateFrame frame = 3; // coordinate frame
    string server = 4;                   // server address
    sint32 port = 5;                     // port number
    string mount_point = 6;              // mount point
    string username = 7;                 // username
    string password = 8;                 // password
    Error error = 15;
}

/******************************************************************************
 | type | topic                                              | payload
 | REQ  | lixel/application/request/set_ntrip_profile        | SetNtripProfileRequest
 | RSP  | lixel/application/response/set_ntrip_profile       | SetNtripProfileResponse
 * notes:
 * Set ntrip profile
 * Possible SetNtripProfileResponse error codes:
 * EC_NtripLoginSuccess,EC_NtripLogoutSuccess,EC_NtripLogoutFailed,
 * EC_NtripDisConnected,EC_NtripNetworkError,EC_NtripAuthError
 *****************************************************************************/
message SetNtripProfileRequest {
    Header header = 1;
    Application.ActionCmd action = 2;    // AC_LOGIN/AC_LOGOUT
    NtripAttr.CorsType cors = 3;         // CORS type
    NtripAttr.CoordinateFrame frame = 4; // coordinate frame
    string server = 5;                   // server address
    sint32 port = 6;                     // port number
    string mount_point = 7;              // mount point
    string username = 8;                 // username
    string password = 9;                 // password
}

message SetNtripProfileResponse {
    Header header = 1;
    Application.ActionCmd action = 2;
    Error error = 15;
}

/******************************************************************************
 | type | topic                                            | payload
 | REQ  | lixel/application/request/get_rtk_advance       | GetRtkAdvanceRequest
 | RSP  | lixel/application/response/get_rtk_advance      | GetRtkAdvanceResponse
 * notes:
 * Get RTK advanced config
 * Possible GetRtkAdvanceResponse error codes:
 * EC_RequestSuccess,EC_RequestError,EC_ProtoError
 * Supported satellite systems: "GPS","BDS","GLO","GAL","QZSS","IRNSS"
 *****************************************************************************/
message GetRtkAdvanceRequest {
    Header header = 1;
}

message GetRtkAdvanceResponse {
    Header header = 1;
    optional RtkAttr.Type rtk_type = 2;     // RTK type
    repeated string satellite_system = 3;   // satellite systems
    Error error = 15;
}

/******************************************************************************
 | type | topic                                            | payload
 | REQ  | lixel/application/request/set_rtk_advance       | SetRtkAdvanceRequest
 | RSP  | lixel/application/response/set_rtk_advance      | SetRtkAdvanceResponse
 * notes:
 * Set RTK advanced config
 * Possible SetRtkAdvanceResponse error codes:
 * EC_RequestSuccess,EC_RequestError,EC_ProtoError
 * Supported satellite systems: "GPS","BDS","GLO","GAL","QZSS","IRNSS"
 *****************************************************************************/
message SetRtkAdvanceRequest {
    Header header = 1;
    optional RtkAttr.Type rtk_type = 2;     // RTK type
    repeated string satellite_system = 3;   // satellite systems
    optional string rtk_register_code = 7;  // register code (L2PS1 model only)
    optional sint32 rtk_control_mode = 8;   // control mode (0: default, 1: passthrough, L2PS1 model only)
}

message SetRtkAdvanceResponse {
    Header header = 1;
    Error error = 15;
}

/******************************************************************************
 | type | topic                                       | payload
 | RPT  | lixel/application/report/system_error       | SystemErrorReport
 * notes:
 * System error code report
 *****************************************************************************/
message SystemErrorReport {
    Header header = 1;
    Error error = 15;
}

/******************************************************************************
 | type | topic                                      | payload
 | REQ  | lixel/application/request/data_unpack      | DataUnpackRequest
 | RSP  | lixel/application/response/data_unpack     | DataUnpackResponse
 | RPT  | lixel/application/report/data_unpack       | DataUnpackReport
 * notes:
 * Data unpack request
 *****************************************************************************/
message DataUnpackRequest {
    enum DataSource {
        XBIN_DATA = 0; // xbin data
    }
    Header header = 1;
    string project_name = 2;    // project name
    DataSource data_source = 3; // data source
    repeated string data_topics = 4; // data topics (if unspecified, unpack all data by default)
    optional string data_ouputdir = 5; // data output directory name (relative to the project directory)
}

message DataUnpackResponse {
    Header header = 1;
    Error error = 15;
}

message DataUnpackReport {
    Header header = 1;
    sint32 progress = 2; // unpack progress (0~100)
    Error error = 15;    // unpack status
}
// clang-format on
```
