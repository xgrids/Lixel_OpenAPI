"""
OpenAPI MQTT client (Qt-independent)

Encapsulates the MQTT connection, protobuf message publish/subscribe, and
provides a synchronous "request-wait-for-response" call mode, so that usage
scripts can invoke device interfaces with minimal code.

Typical usage:
    from example.core import OpenApiClient, ClientOptions

    opts = ClientOptions(host="192.168.12.1", port=1883)
    with OpenApiClient(opts) as client:
        rsp = client.request(
            req_topic="lixel/application/request/device_info",
            req_message="DeviceInfoRequest",
            payload={},
            rsp_topic="lixel/application/response/device_info",
            rsp_message="DeviceInfoResponse",
        )
        print(rsp)

Design notes:
- connect() blocks and waits for the connection to be established (or time out).
- request() first subscribes to the response topic, then publishes the request,
  and finally blocks waiting for a matching response.
- subscribe() supports registering callbacks, for subscribing to report-type
  topics (such as device status, real-time pose).
"""
from __future__ import annotations

import queue
import ssl
import threading
import time
import uuid
from dataclasses import dataclass, field

import paho.mqtt.client as mqtt

from .proto_codec import ProtoCodec


@dataclass
class ClientOptions:
    """Connection parameters

    Common model reference:
    - K1 / L2PRO  : MQTT(1883), no account/password required
    - Porcam      : MQTTS(8883), username=xgrids
    - K2 / H1 / F1: MQTTS(8883), uses long account/password
    """

    host: str = "192.168.12.1"
    port: int = 1883
    use_tls: bool = False
    username: str = ""
    password: str = ""
    client_id: str = ""
    # OpenAPI key: if set, request() will automatically fill in header.openapi_key
    openapi_key: str = ""
    device_id: str = ""
    connect_timeout: float = 10.0
    request_timeout: float = 10.0


class OpenApiClient:
    """OpenAPI MQTT client"""

    def __init__(self, options: ClientOptions | None = None,
                 codec: ProtoCodec | None = None) -> None:
        self._opts = options or ClientOptions()
        self._codec = codec or ProtoCodec()
        self._client: mqtt.Client | None = None
        self._connected = threading.Event()
        self._lock = threading.Lock()
        # Subscription callbacks: topic -> list[callable(topic, dict|bytes)]
        self._callbacks: dict[str, list] = {}
        # Raw subscription callbacks: topic_filter -> list[callable(topic, bytes)] (wildcard-aware)
        self._raw_callbacks: dict[str, list] = {}
        # Pending response queues: topic -> queue.Queue
        self._waiters: dict[str, queue.Queue] = {}
        # Topics that need to be parsed as JSON text (rather than protobuf)
        self._json_topics: set[str] = {
            "lixel/application/report/heartbeat",
        }

    # ---- Lifecycle ---------------------------------------------------------
    @property
    def codec(self) -> ProtoCodec:
        return self._codec

    @property
    def is_connected(self) -> bool:
        return self._client is not None and self._connected.is_set()

    def connect(self) -> None:
        """Connect to the broker, blocking until connected or timed out"""
        client_id = self._opts.client_id or f"openapi-example-{uuid.uuid4().hex[:8]}"
        client = mqtt.Client(
            callback_api_version=mqtt.CallbackAPIVersion.VERSION2,
            client_id=client_id,
            protocol=mqtt.MQTTv311,
        )
        client.on_connect = self._on_connect
        client.on_disconnect = self._on_disconnect
        client.on_message = self._on_message

        if self._opts.username:
            client.username_pw_set(self._opts.username, self._opts.password)

        if self._opts.use_tls:
            client.tls_set(cert_reqs=ssl.CERT_NONE, tls_version=ssl.PROTOCOL_TLSv1_2)
            client.tls_insecure_set(True)

        self._client = client
        self._connected.clear()
        client.connect(self._opts.host, self._opts.port, keepalive=60)
        client.loop_start()

        if not self._connected.wait(timeout=self._opts.connect_timeout):
            self.disconnect()
            raise TimeoutError(
                f"timed out connecting to broker: {self._opts.host}:{self._opts.port}"
            )

    def disconnect(self) -> None:
        """Disconnect"""
        if self._client is not None:
            self._client.loop_stop()
            try:
                self._client.disconnect()
            except Exception:
                pass
            self._client = None
        self._connected.clear()

    def __enter__(self) -> "OpenApiClient":
        self.connect()
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        self.disconnect()

    # ---- Publish / subscribe -----------------------------------------------
    def publish(self, topic: str, message: str, payload=None) -> None:
        """Publish a protobuf message

        Args:
            topic: MQTT topic
            message: protobuf message type name; an empty string means publish JSON/text directly
            payload: dict / JSON string / bytes
        """
        self._require_connected()
        if message:
            data = self._encode_with_header(message, payload)
        else:
            data = self._to_bytes(payload)
        self._client.publish(topic, data, qos=0)

    def subscribe(self, topic: str, message: str = "", callback=None) -> None:
        """Subscribe to a topic; decode and invoke the callback when a message arrives

        Args:
            topic: MQTT topic (wildcards supported)
            message: protobuf message type name; if empty, returns bytes/JSON text as-is
            callback: callable(topic: str, data: dict|str|bytes)
        """
        self._require_connected()
        with self._lock:
            self._callbacks.setdefault(topic, [])
            if callback is not None:
                self._callbacks[topic].append((message, callback))
        self._client.subscribe(topic, qos=0)

    def unsubscribe(self, topic: str) -> None:
        """Unsubscribe"""
        if self._client is not None:
            self._client.unsubscribe(topic)
        with self._lock:
            self._callbacks.pop(topic, None)
            self._raw_callbacks.pop(topic, None)

    def subscribe_raw(self, topic_filter: str, callback) -> None:
        """Subscribe to a topic filter; the callback receives raw bytes (supports +/# wildcards)

        Unlike subscribe(): the callback always receives (topic, payload_bytes), and
        the caller decodes it themselves; matching follows MQTT wildcard rules, which
        is suitable for filters like lixel/#.

        Args:
            topic_filter: MQTT topic filter (may contain + / #)
            callback: callable(topic: str, payload: bytes)
        """
        self._require_connected()
        with self._lock:
            self._raw_callbacks.setdefault(topic_filter, [])
            if callback is not None:
                self._raw_callbacks[topic_filter].append(callback)
        self._client.subscribe(topic_filter, qos=0)

    def request(self, req_topic: str, req_message: str, payload,
                rsp_topic: str, rsp_message: str, timeout: float | None = None) -> dict:
        """Synchronous request-response call

        First subscribes to the response topic, then publishes the request, blocks
        waiting for the first response and returns the decoded dict.

        Args:
            req_topic: request topic
            req_message: request message type name
            payload: request payload (dict / JSON string)
            rsp_topic: response topic
            rsp_message: response message type name
            timeout: timeout (seconds); defaults to options.request_timeout

        Returns:
            The decoded response dict

        Raises:
            TimeoutError: timed out waiting for the response
        """
        self._require_connected()
        timeout = timeout if timeout is not None else self._opts.request_timeout

        q: queue.Queue = queue.Queue(maxsize=1)
        with self._lock:
            self._waiters[rsp_topic] = q
        self._client.subscribe(rsp_topic, qos=0)
        # Leave a very short time window for the subscription to take effect
        time.sleep(0.05)

        try:
            self.publish(req_topic, req_message, payload)
            try:
                raw = q.get(timeout=timeout)
            except queue.Empty:
                raise TimeoutError(f"timed out waiting for response: {rsp_topic}")
            return self._codec.decode(rsp_message, raw)
        finally:
            with self._lock:
                self._waiters.pop(rsp_topic, None)

    # ---- Internal implementation -------------------------------------------
    def _encode_with_header(self, message: str, payload) -> bytes:
        """Encode the message, and auto-fill openapi_key / device_id when a header field exists"""
        data: dict = {}
        if payload is None:
            data = {}
        elif isinstance(payload, dict):
            data = dict(payload)
        elif isinstance(payload, (str, bytes, bytearray)):
            import json
            text = payload.decode("utf-8") if isinstance(payload, (bytes, bytearray)) else payload
            data = json.loads(text) if text.strip() else {}
        else:
            raise TypeError("payload must be a dict or JSON string")

        cls = self._codec.get_class(message)
        if cls is not None and "header" in [f.name for f in cls.DESCRIPTOR.fields]:
            header = dict(data.get("header") or {})
            if self._opts.openapi_key and "openapi_key" not in header:
                header["openapi_key"] = self._opts.openapi_key
            if self._opts.device_id and "device_id" not in header:
                header["device_id"] = self._opts.device_id
            if header:
                data["header"] = header
        return self._codec.encode(message, data)

    @staticmethod
    def _to_bytes(payload) -> bytes:
        if payload is None:
            return b""
        if isinstance(payload, (bytes, bytearray)):
            return bytes(payload)
        if isinstance(payload, str):
            return payload.encode("utf-8")
        import json
        return json.dumps(payload).encode("utf-8")

    def _require_connected(self) -> None:
        if not self.is_connected:
            raise RuntimeError("client is not connected, call connect() first")

    def _on_connect(self, client, userdata, flags, reason_code, properties):
        if reason_code == 0:
            self._connected.set()

    def _on_disconnect(self, client, userdata, flags, reason_code, properties):
        self._connected.clear()

    def _on_message(self, client, userdata, msg: mqtt.MQTTMessage):
        topic = msg.topic
        # Deliver to a pending request first
        with self._lock:
            waiter = self._waiters.get(topic)
            callbacks = list(self._callbacks.get(topic, []))
            raw_items = list(self._raw_callbacks.items())
        if waiter is not None:
            try:
                waiter.put_nowait(msg.payload)
            except queue.Full:
                pass
        # Then dispatch to subscription callbacks
        for message, cb in callbacks:
            try:
                if message:
                    data = self._codec.decode(message, msg.payload)
                elif topic in self._json_topics:
                    data = msg.payload.decode("utf-8", errors="replace")
                else:
                    data = msg.payload
                cb(topic, data)
            except Exception:
                # A callback exception does not affect other subscriptions
                continue
        # Dispatch to raw subscription callbacks (matched by wildcard, always passing bytes)
        for topic_filter, cbs in raw_items:
            if not self._topic_matches(topic_filter, topic):
                continue
            for cb in cbs:
                try:
                    cb(topic, msg.payload)
                except Exception:
                    continue

    @staticmethod
    def _topic_matches(topic_filter: str, topic: str) -> bool:
        """Determine whether the topic matches the filter per MQTT rules (supports + and #)"""
        if topic_filter == topic:
            return True
        fparts = topic_filter.split("/")
        tparts = topic.split("/")
        for i, fp in enumerate(fparts):
            if fp == "#":
                return True
            if i >= len(tparts):
                return False
            if fp == "+":
                continue
            if fp != tparts[i]:
                return False
        return len(fparts) == len(tparts)
