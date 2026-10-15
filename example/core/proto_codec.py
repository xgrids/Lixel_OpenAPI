"""
Protobuf codec (Qt-independent)

Responsible for converting between JSON (dict / str) and protobuf binary, and
can generate default JSON templates for messages. Message types come from the
project-generated proto_gen/openapi/aiot_msgs_pb2.py.

To allow usage scripts to run directly from any working directory, this module
automatically adds the project root to sys.path, so that generated code is
imported through the unified `src.proto_gen.openapi` package path.
"""
from __future__ import annotations

import os
import sys

# Pure Python implementation, to avoid upb C extension incompatibility across environments
os.environ.setdefault("PROTOCOL_BUFFERS_PYTHON_IMPLEMENTATION", "python")

from google.protobuf import json_format  # noqa: E402
from google.protobuf.message import Message  # noqa: E402


def _ensure_project_root_on_path() -> None:
    """Add the project root (the directory containing src/) to sys.path"""
    # This file: <root>/example/core/proto_codec.py
    root = os.path.dirname(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    )
    if root not in sys.path:
        sys.path.insert(0, root)


def _import_pb2():
    """Import the generated protobuf module, compatible with multiple search roots"""
    _ensure_project_root_on_path()
    try:
        from src.proto_gen.openapi import aiot_msgs_pb2
    except ImportError:
        # Fallback: use src as the search root
        from proto_gen.openapi import aiot_msgs_pb2  # type: ignore
    return aiot_msgs_pb2


class ProtoCodec:
    """protobuf message codec"""

    def __init__(self) -> None:
        self._pb2 = _import_pb2()
        self._classes: dict[str, type] = {}
        for name in dir(self._pb2):
            obj = getattr(self._pb2, name)
            if (
                isinstance(obj, type)
                and issubclass(obj, Message)
                and obj is not Message
            ):
                self._classes[name] = obj

    def get_class(self, message: str) -> type | None:
        """Get the protobuf class by message name"""
        return self._classes.get(message)

    def message_names(self) -> list[str]:
        """Return the names of all loaded message types"""
        return sorted(self._classes.keys())

    def encode(self, message: str, data) -> bytes:
        """Encode a dict or JSON string into protobuf binary

        Args:
            message: message type name (e.g. "DeviceInfoRequest")
            data: dict or JSON string
        """
        cls = self._require(message)
        msg = cls()
        if isinstance(data, (bytes, bytearray)):
            data = data.decode("utf-8")
        if isinstance(data, str):
            json_format.Parse(data, msg, ignore_unknown_fields=True)
        elif isinstance(data, dict):
            json_format.ParseDict(data, msg, ignore_unknown_fields=True)
        else:
            raise TypeError("data must be a dict or JSON string")
        return msg.SerializeToString()

    def decode(self, message: str, payload: bytes) -> dict:
        """Decode protobuf binary into a dict"""
        cls = self._require(message)
        msg = cls()
        msg.ParseFromString(payload)
        return json_format.MessageToDict(
            msg,
            preserving_proto_field_name=True,
            always_print_fields_with_no_presence=True,
        )

    def decode_to_json(self, message: str, payload: bytes, indent: int = 2) -> str:
        """Decode protobuf binary into a JSON string"""
        cls = self._require(message)
        msg = cls()
        msg.ParseFromString(payload)
        return json_format.MessageToJson(
            msg,
            preserving_proto_field_name=True,
            always_print_fields_with_no_presence=True,
            indent=indent,
        )

    def default_dict(self, message: str) -> dict:
        """Return the default field template (dict) for the message type"""
        cls = self._require(message)
        return json_format.MessageToDict(
            cls(),
            preserving_proto_field_name=True,
            always_print_fields_with_no_presence=True,
        )

    def default_json(self, message: str, indent: int = 2) -> str:
        """Return the default field template (JSON string) for the message type"""
        cls = self._require(message)
        return json_format.MessageToJson(
            cls(),
            preserving_proto_field_name=True,
            always_print_fields_with_no_presence=True,
            indent=indent,
        )

    def _require(self, message: str) -> type:
        cls = self._classes.get(message)
        if cls is None:
            raise ValueError(f"unknown message type: {message}")
        return cls
