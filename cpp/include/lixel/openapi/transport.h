// The MQTT layer under the client, as an interface.
//
// Client talks to the device only through ITransport. The production implementation uses
// paho.mqtt.c (MakePahoTransport); tests use a scripted fake, so the request/response logic can
// be tested without a broker or a device.
#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "lixel/openapi/result.h"

namespace lixel {
namespace openapi {

// Connection parameters of the MQTT layer.
struct TransportOptions {
  std::string host = "192.168.55.1";  // wired address of the device
  std::uint16_t port = 1883;          // 1883 for MQTT, 8883 for MQTTS
  bool use_tls = false;
  std::string username;
  std::string password;
  std::string client_id;  // empty: the implementation picks a unique one

  // TLS. The device uses a self-signed certificate. By default the server certificate is
  // verified, so a connection to a device only succeeds when ca_file holds that certificate;
  // set verify_server = false to accept any certificate (the connection is then encrypted but
  // not authenticated, and a man in the middle can read the OpenAPI Key).
  bool verify_server = true;
  std::string ca_file;  // PEM file with the trusted certificate(s); empty: system store

  std::chrono::milliseconds connect_timeout{10000};
  std::chrono::milliseconds keep_alive{60000};
};

// Events from the MQTT layer. They arrive on a thread owned by the transport.
class ITransportListener {
 public:
  virtual ~ITransportListener() = default;
  // A message arrived on `topic`.
  virtual void OnMessage(const std::string& topic, const std::string& payload) = 0;
  // The connection is up. `reconnected` is true when it was re-established after a loss.
  virtual void OnConnected(bool reconnected) = 0;
  // The connection was lost; the transport may reconnect by itself.
  virtual void OnConnectionLost(const std::string& reason) = 0;
};

class ITransport {
 public:
  virtual ~ITransport() = default;

  // The listener must outlive the transport or be reset to nullptr before it is destroyed.
  // Call before Connect.
  virtual void SetListener(ITransportListener* listener) = 0;

  // Blocks until connected, refused, or options.connect_timeout has passed.
  // Errc::ConnectFailed, Errc::ConnectTimeout on failure.
  virtual Result Connect(const TransportOptions& options) = 0;

  // Closes the connection. Safe to call when not connected. After it returns no more
  // listener calls are made.
  virtual void Disconnect() = 0;

  virtual bool IsConnected() const = 0;

  // Fire and forget, QoS 0. Errc::NotConnected, Errc::TransportError on failure.
  virtual Result Publish(const std::string& topic, const std::string& payload) = 0;

  // Blocks until the broker acknowledged the subscription or `timeout` has passed.
  // `topic_filter` may contain MQTT wildcards.
  //
  // A `timeout` of zero does not wait: the request is only queued and Ok means "queued". It is
  // what the client uses from inside OnConnected, which runs on the transport's own thread; a
  // call that waited there would wait for an acknowledgement that only this thread can deliver.
  // Errc::RequestTimeout when the acknowledgement did not arrive in time.
  virtual Result Subscribe(const std::string& topic_filter, std::chrono::milliseconds timeout) = 0;

  virtual Result Unsubscribe(const std::string& topic_filter) = 0;
};

// The production transport, based on paho.mqtt.c (MQTT 3.1.1, automatic reconnect).
std::unique_ptr<ITransport> MakePahoTransport();

}  // namespace openapi
}  // namespace lixel
