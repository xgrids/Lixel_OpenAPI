<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="zh_CN" sourcelanguage="en_US">
<context>
    <name>ControlPanel</name>
    <message>
        <source>Lixel OpenAPI Control Panel</source>
        <translation>Lixel OpenAPI 控制面板</translation>
    </message>
    <message>
        <source>Settings</source>
        <translation>设置</translation>
    </message>
    <message>
        <source>Language</source>
        <translation>语言</translation>
    </message>
    <message>
        <source>About</source>
        <translation>关于</translation>
    </message>
    <message>
        <source>Version</source>
        <translation>版本</translation>
    </message>
    <message>
        <source>Website</source>
        <translation>官网</translation>
    </message>
    <message>
        <source>Build</source>
        <translation>构建时间</translation>
    </message>
    <message>
        <source>Copyright © XGRIDS. All rights reserved.</source>
        <translation>版权所有 © XGRIDS，保留所有权利。</translation>
    </message>
    <message>
        <source>Disconnected</source>
        <translation>未连接</translation>
    </message>
    <message>
        <source>Host</source>
        <translation>地址</translation>
    </message>
    <message>
        <source>Port</source>
        <translation>端口</translation>
    </message>
    <message>
        <source>Protocol</source>
        <translation>协议</translation>
    </message>
    <message>
        <source>Username</source>
        <translation>用户名</translation>
    </message>
    <message>
        <source>Password</source>
        <translation>密码</translation>
    </message>
    <message>
        <source>OpenAPI Key</source>
        <translation>OpenAPI Key</translation>
    </message>
    <message>
        <source>Connect</source>
        <translation>连接</translation>
    </message>
    <message>
        <source>Disconnect</source>
        <translation>断开</translation>
    </message>
    <message>
        <source>Connection failed</source>
        <translation>连接失败</translation>
    </message>
    <message>
        <source>Connected: </source>
        <translation>已连接: </translation>
    </message>
</context>
<context>
    <name>PublishPanel</name>
    <message>
        <source>Publish messages</source>
        <translation>发布消息</translation>
    </message>
    <message>
        <source>Publish</source>
        <translation>发布</translation>
    </message>
    <message>
        <source>Message type:</source>
        <translation>消息类型:</translation>
    </message>
    <message>
        <source>Load template</source>
        <translation>加载模板</translation>
    </message>
    <message>
        <source>Enter message content in JSON...</source>
        <translation>输入 JSON 格式的消息内容...</translation>
    </message>
    <message>
        <source>Subscribe to the response in the &quot;Subscribe&quot; panel on the right: </source>
        <translation>响应请在右侧“订阅消息”面板订阅：</translation>
    </message>
    <message>
        <source>Subscribe to the report in the &quot;Subscribe&quot; panel on the right: </source>
        <translation>上报请在右侧“订阅消息”面板订阅：</translation>
    </message>
    <message>
        <source>Load template supports Request-type messages only.</source>
        <translation>加载模板仅支持请求(Request)类型消息</translation>
    </message>
    <message>
        <source>Load template failed: </source>
        <translation>加载模板失败: </translation>
    </message>
    <message>
        <source>Device not connected. Please connect first.</source>
        <translation>未连接设备，请先连接。</translation>
    </message>
    <message>
        <source>Please enter a Topic</source>
        <translation>请输入 Topic</translation>
    </message>
    <message>
        <source>Please select a message type</source>
        <translation>请选择消息类型</translation>
    </message>
    <message>
        <source>JSON parse error: </source>
        <translation>JSON 解析失败: </translation>
    </message>
    <message>
        <source>Publish failed: </source>
        <translation>发布失败: </translation>
    </message>
    <message>
        <source>Published: </source>
        <translation>已发布: </translation>
    </message>
    <message>
        <source>Device heartbeat</source>
        <translation>设备心跳</translation>
    </message>
    <message>
        <source>Device heartbeat broadcast. Payload is a JSON string (not protobuf) with DeviceSN / Timestamp / ProtocolVersion. Useful for device discovery.</source>
        <translation>设备广播心跳，负载为 JSON 字符串（非 protobuf），包含 DeviceSN / Timestamp / ProtocolVersion。可用于设备发现。</translation>
    </message>
    <message>
        <source>Payload is JSON text, e.g. {&quot;DeviceSN&quot;:&quot;A256A60000&quot;,&quot;Timestamp&quot;:1697703246.5,&quot;ProtocolVersion&quot;:&quot;3.0.0&quot;}</source>
        <translation>负载为 JSON 文本，例如 {&quot;DeviceSN&quot;:&quot;A256A60000&quot;,&quot;Timestamp&quot;:1697703246.5,&quot;ProtocolVersion&quot;:&quot;3.0.0&quot;}</translation>
    </message>
    <message>
        <source>Device status report</source>
        <translation>设备状态上报</translation>
    </message>
    <message>
        <source>Device status reported at 1 Hz, including mapping state, system resources, RTK status, etc.</source>
        <translation>设备状态信息，1Hz 周期上报，含建图状态、系统资源、RTK 状态等。</translation>
    </message>
    <message>
        <source>modeling_state is a unified status code, e.g. EC_ScanReady=0x12040000+300.</source>
        <translation>modeling_state 为统一状态码，如 EC_ScanReady=0x12040000+300。</translation>
    </message>
    <message>
        <source>Realtime pose report</source>
        <translation>实时位姿上报</translation>
    </message>
    <message>
        <source>Realtime LIO pose (position + orientation quaternion) and accumulated distance during scanning.</source>
        <translation>扫描过程中实时上报 LIO 位姿（位置 + 姿态四元数）与累计距离。</translation>
    </message>
    <message>
        <source>Realtime point cloud report</source>
        <translation>实时点云上报</translation>
    </message>
    <message>
        <source>Realtime intensity / true-color point cloud, wrapped in MqttCompressMsg; decompresses to LioPclReport.</source>
        <translation>实时强度点云 / 真彩点云上报，使用 MqttCompressMsg 压缩封装，解压后为 LioPclReport。</translation>
    </message>
    <message>
        <source>compressed_data is the compressed serialized LioPclReport; compression type is in the compression field (Lz4 / Zstd).</source>
        <translation>compressed_data 为压缩后的 LioPclReport 序列化数据，压缩类型见 compression 字段（Lz4 / Zstd）。</translation>
    </message>
    <message>
        <source>System error report</source>
        <translation>系统错误上报</translation>
    </message>
    <message>
        <source>Device actively reports system error code and description.</source>
        <translation>设备主动上报系统错误码与描述。</translation>
    </message>
    <message>
        <source>Query device info</source>
        <translation>查询设备信息</translation>
    </message>
    <message>
        <source>Query device base info, camera/lidar/RTK info, working status, network status, etc.</source>
        <translation>查询设备基础信息、相机/雷达/RTK 信息、工作状态、网络状态等。</translation>
    </message>
    <message>
        <source>Device config</source>
        <translation>设备参数配置</translation>
    </message>
    <message>
        <source>Configure device parameters (time, main storage type, etc.). The device reports actively when parameters change.</source>
        <translation>配置设备参数（时间、主存储类型等）。参数变化时设备会主动上报。</translation>
    </message>
    <message>
        <source>Error codes: EC_RequestSuccess / EC_RequestError / EC_ProtoError. RPT reuses the DeviceConfigRequest structure to report current parameters.</source>
        <translation>错误码：EC_RequestSuccess / EC_RequestError / EC_ProtoError。RPT 复用 DeviceConfigRequest 结构上报当前参数。</translation>
    </message>
    <message>
        <source>Device control</source>
        <translation>设备控制</translation>
    </message>
    <message>
        <source>Device control commands (USB mass storage, ethernet, format, power off, reboot, etc.).</source>
        <translation>设备控制指令（U 盘模式、以太网、格式化、关机、重启等）。</translation>
    </message>
    <message>
        <source>Supported ActionCmd: AC_USB_MASS, AC_USB_ETHER, AC_DISK_FORMAT, AC_POWER_OFF, AC_SYSTEM_REBOOT.</source>
        <translation>支持的 ActionCmd：AC_USB_MASS, AC_USB_ETHER, AC_DISK_FORMAT, AC_POWER_OFF, AC_SYSTEM_REBOOT。</translation>
    </message>
    <message>
        <source>Scan control</source>
        <translation>扫描控制</translation>
    </message>
    <message>
        <source>Start / stop scan mapping and periodically report scan status (distance, speed, duration).</source>
        <translation>启动 / 停止扫描建图，并周期上报扫描状态（移动距离、速度、时长）。</translation>
    </message>
    <message>
        <source>action supports AC_START / AC_STOP. Error codes: EC_RequestSuccess / EC_RequestError / EC_ProtoError.</source>
        <translation>action 支持 AC_START / AC_STOP。错误码：EC_RequestSuccess / EC_RequestError / EC_ProtoError。</translation>
    </message>
    <message>
        <source>Current project file URLs</source>
        <translation>当前工程文件地址查询</translation>
    </message>
    <message>
        <source>Query the artifact URLs (pose, control points, measure points) of the current project. In standby the device does not reply and the request times out.</source>
        <translation>查询当前工程的产物 URL（位姿、控制点、测量点）。待机时设备不回复，请求会超时。</translation>
    </message>
    <message>
        <source>Control point operations</source>
        <translation>控制点操作</translation>
    </message>
    <message>
        <source>Add / remove control points; the device also reports when a point is marked via a button. In standby the device does not reply. It chooses the id of an added point itself.</source>
        <translation>控制点的增加 / 删除；设备按键打点时也会主动上报。待机时设备不回复，新增点的 id 由设备分配。</translation>
    </message>
    <message>
        <source>action supports AC_ADD / AC_REMOVE. AC_UPDATE is defined in the proto but the device does not handle it. The id in the AC_ADD request is ignored; use the id from the response to remove.</source>
        <translation>action 支持 AC_ADD / AC_REMOVE。proto 中定义了 AC_UPDATE，但设备不处理。AC_ADD 请求里的 id 会被忽略，删除时使用响应里的 id。</translation>
    </message>
    <message>
        <source>Measure point operations</source>
        <translation>测量点操作</translation>
    </message>
    <message>
        <source>Add / remove measure points.</source>
        <translation>测量点的增加 / 删除。</translation>
    </message>
    <message>
        <source>action supports AC_ADD / AC_REMOVE only. Note: the measure point feature is supported on some devices only.</source>
        <translation>action 仅支持 AC_ADD / AC_REMOVE。注意：测量点功能仅部分设备支持。</translation>
    </message>
    <message>
        <source>Project list operations</source>
        <translation>工程列表操作</translation>
    </message>
    <message>
        <source>List all projects, modify project labels, delete projects.</source>
        <translation>列出所有工程、修改工程标签、删除工程。</translation>
    </message>
    <message>
        <source>action supports AC_LIST_ALL_PROJECTS / AC_MODIFY_PROJECT_LABELS / AC_DELETE_PROJECTS.</source>
        <translation>action 支持 AC_LIST_ALL_PROJECTS / AC_MODIFY_PROJECT_LABELS / AC_DELETE_PROJECTS。</translation>
    </message>
    <message>
        <source>Get Ntrip profile</source>
        <translation>获取 Ntrip 配置</translation>
    </message>
    <message>
        <source>Get the current Ntrip (CORS) profile: server, port, mount point, account, etc.</source>
        <translation>获取当前 Ntrip（CORS）配置：服务地址、端口、挂载点、账号等。</translation>
    </message>
    <message>
        <source>Set Ntrip profile</source>
        <translation>设置 Ntrip 配置</translation>
    </message>
    <message>
        <source>Log in / out of Ntrip and set CORS type, coordinate frame, server, account, etc.</source>
        <translation>登录 / 登出 Ntrip，并设置 CORS 类型、坐标系、服务地址、账号等。</translation>
    </message>
    <message>
        <source>action supports AC_LOGIN / AC_LOGOUT. Error codes: EC_NtripLoginSuccess / EC_NtripLogoutSuccess / EC_NtripLogoutFailed / EC_NtripDisConnected / EC_NtripNetworkError / EC_NtripAuthError. Setting is only allowed when the RTK status is HwConnected. When the device WiFi is in AP mode (Ntrip data is forwarded over WebSocket), sending this request makes the Response return error code AppCode::EMBError::Unknown; this error can be ignored and the device Ntrip connection is not affected.</source>
        <translation>action 支持 AC_LOGIN / AC_LOGOUT。错误码：EC_NtripLoginSuccess / EC_NtripLogoutSuccess / EC_NtripLogoutFailed / EC_NtripDisConnected / EC_NtripNetworkError / EC_NtripAuthError。仅当 RTK 状态为 HwConnected 时才允许设置。当设备 WiFi 处于 AP 模式时（Ntrip 数据通过 WebSocket 转发），发送该请求会使 Response 返回错误码 AppCode::EMBError::Unknown；该错误可忽略，设备 Ntrip 连接不受影响。</translation>
    </message>
    <message>
        <source>Get RTK advanced config</source>
        <translation>获取 RTK 高级配置</translation>
    </message>
    <message>
        <source>Get the RTK type and enabled satellite systems.</source>
        <translation>获取 RTK 类型与启用的卫星系统。</translation>
    </message>
    <message>
        <source>Supported satellite systems: GPS / BDS / GLO / GAL / QZSS / IRNSS.</source>
        <translation>支持的卫星系统：GPS / BDS / GLO / GAL / QZSS / IRNSS。</translation>
    </message>
    <message>
        <source>Set RTK advanced config</source>
        <translation>设置 RTK 高级配置</translation>
    </message>
    <message>
        <source>Set RTK type, satellite systems, register code, control mode, etc.</source>
        <translation>设置 RTK 类型、卫星系统、注册码、控制模式等。</translation>
    </message>
    <message>
        <source>rtk_register_code / rtk_control_mode apply to the L2PS1 model only.</source>
        <translation>rtk_register_code / rtk_control_mode 仅 L2PS1 机型适用。</translation>
    </message>
    <message>
        <source>Data unpack</source>
        <translation>数据解包</translation>
    </message>
    <message>
        <source>Unpack the project&apos;s xbin data and periodically report the unpack progress.</source>
        <translation>对工程 xbin 数据进行解包，并周期上报解包进度。</translation>
    </message>
    <message>
        <source>data_source currently supports XBIN_DATA; if data_topics is omitted, everything is unpacked. Note: the data processing (unpack) feature is supported on some devices only.</source>
        <translation>data_source 目前支持 XBIN_DATA；未指定 data_topics 则解包全部。注意：数据处理（解包）功能仅部分设备支持。</translation>
    </message>
</context>
<context>
    <name>SubscribePanel</name>
    <message>
        <source>Subscribe messages</source>
        <translation>订阅消息</translation>
    </message>
    <message>
        <source>Subscribe</source>
        <translation>订阅</translation>
    </message>
    <message>
        <source>Subscribe all</source>
        <translation>订阅全部</translation>
    </message>
    <message>
        <source>Wildcard:</source>
        <translation>通配符:</translation>
    </message>
    <message>
        <source>Current subscriptions:</source>
        <translation>当前订阅:</translation>
    </message>
    <message>
        <source>Unsub selected</source>
        <translation>取消选中</translation>
    </message>
    <message>
        <source>Unsub all</source>
        <translation>全部取消</translation>
    </message>
    <message>
        <source>Received messages:</source>
        <translation>收到的消息:</translation>
    </message>
    <message>
        <source>Auto scroll</source>
        <translation>自动滚动</translation>
    </message>
    <message>
        <source>View detail</source>
        <translation>查看详情</translation>
    </message>
    <message>
        <source>Clear</source>
        <translation>清空</translation>
    </message>
    <message>
        <source>Device not connected. Please connect first.</source>
        <translation>未连接设备，请先连接。</translation>
    </message>
    <message>
        <source>Parse error</source>
        <translation>解析失败</translation>
    </message>
    <message>
        <source>Binary</source>
        <translation>二进制</translation>
    </message>
    <message>
        <source>...&lt;omitted, {n} chars total&gt;</source>
        <translation>...&lt;省略，共 {n} 字符&gt;</translation>
    </message>
</context>
</TS>
