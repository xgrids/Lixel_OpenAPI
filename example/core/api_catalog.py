"""
OpenAPI interface catalog (single source of truth).

This module manually extracts every MQTT topic actually defined in
proto/openapi/aiot_msgs.proto, together with its message type, direction and
description, serving as shared interface metadata for both the GUI control
panel and the example scripts.

Design principles:
- Only interfaces that truly exist in aiot_msgs.proto are listed; no invented
  topics.
- Each "interface" (Interface) represents one functional unit, which may contain:
    request  -- request topic and message (client -> device)
    response -- response topic and message (device -> client)
    report   -- report topic and message (device -> client, actively pushed)
- Message names map one-to-one to the classes in
  proto_gen/openapi/aiot_msgs_pb2.py.

Note: name/description/notes are written in English (the source language) and
translated to Chinese via example/i18n/openapi_zh.ts at display time in the GUI.
"""
from __future__ import annotations

from dataclasses import dataclass
from enum import Enum


class Direction(Enum):
    """Message direction."""

    REQUEST = "REQ"   # client -> device
    RESPONSE = "RSP"  # device -> client (reply to a request)
    REPORT = "RPT"    # device -> client (active push)


@dataclass(frozen=True)
class Endpoint:
    """A single MQTT endpoint: one topic + its protobuf message type."""

    direction: Direction
    topic: str
    message: str  # protobuf message class name; empty string means JSON payload


@dataclass(frozen=True)
class ApiInterface:
    """A functional interface aggregating request/response/report endpoints."""

    key: str          # stable id (English, used for example filenames / code refs)
    name: str         # display name
    group: str        # functional group
    description: str  # description
    request: Endpoint | None = None
    response: Endpoint | None = None
    report: Endpoint | None = None
    notes: str = ""    # extra notes (error codes, supported actions, etc.)

    def endpoints(self) -> list[Endpoint]:
        return [e for e in (self.request, self.response, self.report) if e is not None]


# --- Functional groups -----------------------------------------------------
GROUP_MONITOR = "Status Monitoring"
GROUP_DEVICE = "Device Management"
GROUP_SCAN = "Scan Mapping"
GROUP_POINT = "Point Management"
GROUP_PROJECT = "Project Management"
GROUP_RTK = "RTK / Ntrip"
GROUP_DATA = "Data Processing"


def _req(topic: str, msg: str) -> Endpoint:
    return Endpoint(Direction.REQUEST, topic, msg)


def _rsp(topic: str, msg: str) -> Endpoint:
    return Endpoint(Direction.RESPONSE, topic, msg)


def _rpt(topic: str, msg: str) -> Endpoint:
    return Endpoint(Direction.REPORT, topic, msg)


# --- Interface catalog (strictly matches aiot_msgs.proto) ------------------
INTERFACES: list[ApiInterface] = [
    # ==================== Status Monitoring (report only) ====================
    ApiInterface(
        key="heartbeat",
        name="Device heartbeat",
        group=GROUP_MONITOR,
        description="Device heartbeat broadcast. Payload is a JSON string (not "
                    "protobuf) with DeviceSN / Timestamp / ProtocolVersion. "
                    "Useful for device discovery.",
        report=_rpt("lixel/application/report/heartbeat", ""),
        notes="Payload is JSON text, e.g. "
              '{"DeviceSN":"A256A60000","Timestamp":1697703246.5,'
              '"ProtocolVersion":"3.0.0"}',
    ),
    ApiInterface(
        key="device_status",
        name="Device status report",
        group=GROUP_MONITOR,
        description="Device status reported at 1 Hz, including mapping state, "
                    "system resources, RTK status, etc.",
        report=_rpt("lixel/application/report/device_status", "DeviceStatusReport"),
        notes="modeling_state is a unified status code, e.g. "
              "EC_ScanReady=0x32040000+300.",
    ),
    ApiInterface(
        key="lio_pose",
        name="Realtime pose report",
        group=GROUP_MONITOR,
        description="Realtime LIO pose (position + orientation quaternion) and "
                    "accumulated distance during scanning.",
        report=_rpt("lixel/application/report/lio_pose", "LioPoseReport"),
    ),
    ApiInterface(
        key="lio_pcl",
        name="Realtime point cloud report",
        group=GROUP_MONITOR,
        description="Realtime intensity / true-color point cloud, wrapped in "
                    "MqttCompressMsg; decompresses to LioPclReport.",
        report=_rpt("lixel/application/report/lio_pcl", "MqttCompressMsg"),
        notes="compressed_data is the compressed serialized LioPclReport; "
              "compression type is in the compression field (Lz4 / Zstd).",
    ),
    ApiInterface(
        key="system_error",
        name="System error report",
        group=GROUP_MONITOR,
        description="Device actively reports system error code and description.",
        report=_rpt("lixel/application/report/system_error", "SystemErrorReport"),
    ),

    # ==================== Device Management ====================
    ApiInterface(
        key="device_info",
        name="Query device info",
        group=GROUP_DEVICE,
        description="Query device base info, camera/lidar/RTK info, working "
                    "status, network status, etc.",
        request=_req("lixel/application/request/device_info", "DeviceInfoRequest"),
        response=_rsp("lixel/application/response/device_info", "DeviceInfoResponse"),
    ),
    ApiInterface(
        key="device_config",
        name="Device config",
        group=GROUP_DEVICE,
        description="Configure device parameters (time, main storage type, "
                    "etc.). The device reports actively when parameters change.",
        request=_req("lixel/application/request/device_config", "DeviceConfigRequest"),
        response=_rsp("lixel/application/response/device_config", "DeviceConfigResponse"),
        report=_rpt("lixel/application/report/device_config", "DeviceConfigRequest"),
        notes="Error codes: EC_RequestSuccess / EC_RequestError / EC_ProtoError. "
              "RPT reuses the DeviceConfigRequest structure to report current "
              "parameters.",
    ),
    ApiInterface(
        key="device_control",
        name="Device control",
        group=GROUP_DEVICE,
        description="Device control commands (USB mass storage, ethernet, "
                    "format, power off, reboot, etc.).",
        request=_req("lixel/application/request/device_control", "DeviceControlRequest"),
        response=_rsp("lixel/application/response/device_control", "DeviceControlResponse"),
        notes="Supported ActionCmd: AC_USB_MASS, AC_USB_ETHER, AC_DISK_FORMAT, "
              "AC_POWER_OFF, AC_SYSTEM_REBOOT.",
    ),

    # ==================== Scan Mapping ====================
    ApiInterface(
        key="modeling",
        name="Scan control",
        group=GROUP_SCAN,
        description="Start / stop scan mapping and periodically report scan "
                    "status (distance, speed, duration).",
        request=_req("lixel/application/request/modeling", "ModelingRequest"),
        response=_rsp("lixel/application/response/modeling", "ModelingResponse"),
        report=_rpt("lixel/application/report/modeling", "ModelingReport"),
        notes="action supports AC_START / AC_STOP. Error codes: "
              "EC_RequestSuccess / EC_RequestError / EC_ProtoError.",
    ),
    ApiInterface(
        key="modeling_status",
        name="Scan status query",
        group=GROUP_SCAN,
        description="Query the scan status and artifact URLs (pose, control "
                    "points, measure points) of the current / latest project.",
        request=_req("lixel/application/request/modeling_status", "ModelingStatusRequest"),
        response=_rsp("lixel/application/response/modeling_status", "ModelingStatusResponse"),
    ),

    # ==================== Point Management ====================
    ApiInterface(
        key="control_point",
        name="Control point operations",
        group=GROUP_POINT,
        description="Add / remove / update control points; the device also "
                    "reports when a point is marked via a button.",
        request=_req("lixel/application/request/control_point", "ControlPointRequest"),
        response=_rsp("lixel/application/response/control_point", "ControlPointResponse"),
        report=_rpt("lixel/application/report/control_point", "ControlPointReport"),
        notes="action supports AC_ADD / AC_REMOVE / AC_UPDATE only.",
    ),
    ApiInterface(
        key="measure_point",
        name="Measure point operations",
        group=GROUP_POINT,
        description="Add / remove measure points.",
        request=_req("lixel/application/request/measure_point", "MeasurePointRequest"),
        response=_rsp("lixel/application/response/measure_point", "MeasurePointResponse"),
        notes="action supports AC_ADD / AC_REMOVE only. Note: the measure point "
              "feature is supported on some devices only.",
    ),

    # ==================== Project Management ====================
    ApiInterface(
        key="project_list",
        name="Project list operations",
        group=GROUP_PROJECT,
        description="List all projects, modify project labels, delete projects.",
        request=_req("lixel/application/request/project_list", "ProjectListRequest"),
        response=_rsp("lixel/application/response/project_list", "ProjectListResponse"),
        notes="action supports AC_LIST_ALL_PROJECTS / AC_MODIFY_PROJECT_LABELS / "
              "AC_DELETE_PROJECTS.",
    ),

    # ==================== RTK / Ntrip ====================
    ApiInterface(
        key="get_ntrip_profile",
        name="Get Ntrip profile",
        group=GROUP_RTK,
        description="Get the current Ntrip (CORS) profile: server, port, mount "
                    "point, account, etc.",
        request=_req("lixel/application/request/get_ntrip_profile", "GetNtripProfileRequest"),
        response=_rsp("lixel/application/response/get_ntrip_profile", "GetNtripProfileResponse"),
    ),
    ApiInterface(
        key="set_ntrip_profile",
        name="Set Ntrip profile",
        group=GROUP_RTK,
        description="Log in / out of Ntrip and set CORS type, coordinate frame, "
                    "server, account, etc.",
        request=_req("lixel/application/request/set_ntrip_profile", "SetNtripProfileRequest"),
        response=_rsp("lixel/application/response/set_ntrip_profile", "SetNtripProfileResponse"),
        notes="action supports AC_LOGIN / AC_LOGOUT. Error codes: "
              "EC_NtripLoginSuccess / EC_NtripLogoutSuccess / EC_NtripLogoutFailed / "
              "EC_NtripDisConnected / EC_NtripNetworkError / EC_NtripAuthError. "
              "Setting is only allowed when the RTK status is HwConnected. When the "
              "device WiFi is in AP mode (Ntrip data is forwarded over WebSocket), "
              "sending this request makes the Response return error code "
              "AppCode::EMBError::Unknown; this error can be ignored and the device "
              "Ntrip connection is not affected.",
    ),
    ApiInterface(
        key="get_rtk_advance",
        name="Get RTK advanced config",
        group=GROUP_RTK,
        description="Get the RTK type and enabled satellite systems.",
        request=_req("lixel/application/request/get_rtk_advance", "GetRtkAdvanceRequest"),
        response=_rsp("lixel/application/response/get_rtk_advance", "GetRtkAdvanceResponse"),
        notes="Supported satellite systems: GPS / BDS / GLO / GAL / QZSS / IRNSS.",
    ),
    ApiInterface(
        key="set_rtk_advance",
        name="Set RTK advanced config",
        group=GROUP_RTK,
        description="Set RTK type, satellite systems, register code, control "
                    "mode, etc.",
        request=_req("lixel/application/request/set_rtk_advance", "SetRtkAdvanceRequest"),
        response=_rsp("lixel/application/response/set_rtk_advance", "SetRtkAdvanceResponse"),
        notes="rtk_register_code / rtk_control_mode apply to the L2PS1 model only.",
    ),

    # ==================== Data Processing ====================
    ApiInterface(
        key="data_unpack",
        name="Data unpack",
        group=GROUP_DATA,
        description="Unpack the project's xbin data and periodically report the "
                    "unpack progress.",
        request=_req("lixel/application/request/data_unpack", "DataUnpackRequest"),
        response=_rsp("lixel/application/response/data_unpack", "DataUnpackResponse"),
        report=_rpt("lixel/application/report/data_unpack", "DataUnpackReport"),
        notes="data_source currently supports XBIN_DATA; if data_topics is "
              "omitted, everything is unpacked. Note: the data processing "
              "(unpack) feature is supported on some devices only.",
    ),
]


# --- Convenience queries ----------------------------------------------------
def all_interfaces() -> list[ApiInterface]:
    """Return all interfaces."""
    return list(INTERFACES)


def get_interface(key: str) -> ApiInterface | None:
    """Get an interface by key."""
    for itf in INTERFACES:
        if itf.key == key:
            return itf
    return None


def interface_for_topic(topic: str) -> ApiInterface | None:
    """Reverse-lookup the interface that owns any given topic."""
    for itf in INTERFACES:
        for ep in itf.endpoints():
            if ep.topic == topic:
                return itf
    return None


def groups() -> list[str]:
    """Return all group names in definition order (deduplicated)."""
    seen: list[str] = []
    for itf in INTERFACES:
        if itf.group not in seen:
            seen.append(itf.group)
    return seen


def interfaces_by_group(group: str) -> list[ApiInterface]:
    """Return the interfaces in the given group."""
    return [itf for itf in INTERFACES if itf.group == group]


def all_topics() -> list[str]:
    """Return all topics."""
    topics: list[str] = []
    for itf in INTERFACES:
        for ep in itf.endpoints():
            topics.append(ep.topic)
    return topics


def subscribe_topics() -> list[str]:
    """Return the topics to subscribe (response + report)."""
    topics: list[str] = []
    for itf in INTERFACES:
        for ep in itf.endpoints():
            if ep.direction in (Direction.RESPONSE, Direction.REPORT):
                topics.append(ep.topic)
    return topics


def publish_topics() -> list[str]:
    """Return the publishable topics (request)."""
    topics: list[str] = []
    for itf in INTERFACES:
        if itf.request is not None:
            topics.append(itf.request.topic)
    return topics


def request_messages() -> list[str]:
    """Return publishable request message names (deduplicated, Request only)."""
    names: list[str] = []
    for itf in INTERFACES:
        if itf.request is not None and itf.request.message:
            if itf.request.message not in names:
                names.append(itf.request.message)
    return names


def message_for_topic(topic: str) -> str:
    """Look up the protobuf message name for a topic; empty if JSON/unknown."""
    for itf in INTERFACES:
        for ep in itf.endpoints():
            if ep.topic == topic:
                return ep.message
    return ""
