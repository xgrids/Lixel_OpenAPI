"""
example.core -- reusable OpenAPI client and interface metadata

Independent of Qt, shared by usage scripts and the GUI.

Typical usage:
    from example.core import OpenApiClient, ClientOptions

    client = OpenApiClient(ClientOptions(host="192.168.12.1"))
    client.connect()
    rsp = client.request(
        "lixel/application/request/device_info", "DeviceInfoRequest", {},
        "lixel/application/response/device_info", "DeviceInfoResponse",
    )
    print(rsp)
    client.disconnect()
"""
from .api_catalog import (
    ApiInterface,
    Direction,
    Endpoint,
    all_interfaces,
    all_topics,
    get_interface,
    groups,
    interface_for_topic,
    interfaces_by_group,
    message_for_topic,
    publish_topics,
    request_messages,
    subscribe_topics,
)
from .openapi_client import ClientOptions, OpenApiClient
from .proto_codec import ProtoCodec

__all__ = [
    "ApiInterface",
    "Direction",
    "Endpoint",
    "all_interfaces",
    "all_topics",
    "get_interface",
    "groups",
    "interface_for_topic",
    "interfaces_by_group",
    "message_for_topic",
    "publish_topics",
    "request_messages",
    "subscribe_topics",
    "ClientOptions",
    "OpenApiClient",
    "ProtoCodec",
]
