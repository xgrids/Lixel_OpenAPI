"""
Controller: adapter layer between the GUI and example.core.OpenApiClient.

Centrally manages a single client connection and offers a unified calling entry
point to the panels, so widgets do not depend on MQTT details directly.
"""
from __future__ import annotations

try:
    from example.core import ClientOptions, OpenApiClient, ProtoCodec
except ImportError:  # pragma: no cover
    from core import ClientOptions, OpenApiClient, ProtoCodec  # type: ignore


class PanelController:
    """Wraps connection and requests for the GUI panels."""

    def __init__(self) -> None:
        self._codec = ProtoCodec()
        self._client: OpenApiClient | None = None
        self._openapi_key: str = ""

    # ---- Connection management --------------------------------------------
    def connect(self, options: ClientOptions) -> None:
        self.disconnect()
        self._openapi_key = options.openapi_key
        client = OpenApiClient(options, codec=self._codec)
        client.connect()
        self._client = client

    def set_openapi_key(self, key: str) -> None:
        """Update the current OpenAPI Key (for template injection; can be set before connecting)."""
        self._openapi_key = key

    def openapi_key(self) -> str:
        return self._openapi_key

    def disconnect(self) -> None:
        if self._client is not None:
            self._client.disconnect()
            self._client = None

    def is_connected(self) -> bool:
        return self._client is not None and self._client.is_connected

    # ---- Call forwarding ---------------------------------------------------
    def publish(self, topic, message, payload) -> None:
        """Publish a request message (does not wait for a response)."""
        if self._client is None:
            raise RuntimeError("Not connected")
        self._client.publish(topic, message, payload)

    def subscribe_raw(self, topic_filter, callback) -> None:
        """Subscribe (raw bytes, wildcard-aware); callback is (topic, payload_bytes)."""
        if self._client is None:
            raise RuntimeError("Not connected")
        self._client.subscribe_raw(topic_filter, callback)

    def unsubscribe(self, topic) -> None:
        if self._client is not None:
            self._client.unsubscribe(topic)

    # ---- Decode ------------------------------------------------------------
    def decode(self, message: str, payload: bytes) -> dict:
        return self._codec.decode(message, payload)

    # ---- Template ----------------------------------------------------------
    def default_json(self, message: str) -> str:
        return self._codec.default_json(message)
