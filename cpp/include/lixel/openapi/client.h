// The OpenAPI client: connect, request/response, subscribe.
//
// Thread model
//   - Connect, Disconnect, Request, Publish, Subscribe and Unsubscribe may be called from any
//     thread that is NOT one of the library's callback threads, and from several threads at once.
//   - Subscription handlers run on a thread owned by the MQTT layer, one message at a time.
//     Keep them short. Never call Request, Connect or Disconnect from a handler: Request returns
//     Errc::CalledFromCallback instead of blocking, the other two are not allowed.
//   - The client must not be destroyed from a handler.
//
// Memory
//   - The library never keeps a pointer you pass in after the call returns. The strings passed to
//     a handler are valid only for the duration of the call; copy what you need.
#pragma once

#include <google/protobuf/message.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "lixel/openapi/result.h"
#include "lixel/openapi/transport.h"

namespace lixel {
namespace openapi {

struct ClientOptions {
  TransportOptions transport;

  // Written into the header of every request that has one. Without a valid key the device
  // answers EC_ProtoError ("openapi auth error") to everything except the heartbeat.
  std::string openapi_key;
  // Written into header.device_id when not empty. Leave empty unless the device was set up to
  // check it: a device that checks answers "device auth error" on a mismatch.
  std::string device_id;

  std::chrono::milliseconds request_timeout{10000};
  // Wait for the broker to acknowledge a new subscription.
  std::chrono::milliseconds subscribe_timeout{5000};
};

// Receives the raw bytes of a message. `topic` is the topic the message arrived on.
using RawHandler = std::function<void(const std::string& topic, const std::string& payload)>;

class Client : private ITransportListener {
 public:
  // Uses the paho based transport.
  explicit Client(ClientOptions options);
  // Uses `transport`; for tests and for custom MQTT layers.
  Client(ClientOptions options, std::unique_ptr<ITransport> transport);
  // Disconnects. Not callable from a handler.
  ~Client() override;

  Client(const Client&) = delete;
  Client& operator=(const Client&) = delete;

  // Blocks until connected. Errc::ConnectFailed, Errc::ConnectTimeout on failure; the client
  // can be used for another Connect afterwards. Calling it when connected returns Ok.
  Result Connect();
  // Safe when not connected. Pending Request calls return Errc::NotConnected.
  void Disconnect();
  bool IsConnected() const;

  // Sends one request and waits for its response.
  //
  // `request` is copied; the copy gets header.seq, header.openapi_key and header.device_id. Do
  // not set header.seq yourself. `response` is overwritten when a matching response arrived,
  // whatever its device code. `timeout` of zero means options.request_timeout.
  //
  // Returns
  //   Ok                  a response arrived and its Error.code is a success code
  //   DeviceError         a response arrived with another Error.code; Result::device_code and
  //                       desc hold it, and *response is filled so you can read the rest
  //   RequestTimeout      no response with the same seq within `timeout`. The device may still
  //                       have carried the request out: ask it before assuming it did not
  //   NotConnected        not connected, or the connection was closed while waiting
  //   CalledFromCallback  called from a subscription handler
  //   InvalidArgument     a pointer is null or the topics are empty
  //   EncodeFailed / DecodeFailed / TransportError
  //
  // After any failure the client stays usable. A response that arrives late is dropped.
  Result Request(const std::string& request_topic, const google::protobuf::Message& request,
                 const std::string& response_topic, google::protobuf::Message* response,
                 std::chrono::milliseconds timeout = std::chrono::milliseconds(0));

  // Sends a message without waiting for anything. Header fields are filled as in Request.
  Result Publish(const std::string& topic, const google::protobuf::Message& message);
  // Sends raw bytes as they are, for payloads that are not protobuf.
  Result PublishRaw(const std::string& topic, const std::string& payload);

  // Subscribes to `topic_filter` (MQTT wildcards '+' and '#' allowed) and calls `handler` for
  // every matching message. Subscribing the same filter twice adds a second handler. After a
  // reconnect the subscription is restored automatically. (The response topics that Request
  // uses are not restored in advance: the next Request subscribes to its response topic again.)
  Result Subscribe(const std::string& topic_filter, RawHandler handler);

  // Subscribes and parses each payload as `message_name` before calling `handler`. A payload
  // that does not parse is dropped and counted in DroppedMessages().
  using MessageHandler =
      std::function<void(const std::string& topic, const google::protobuf::Message&)>;
  Result SubscribeMessage(const std::string& topic_filter, const std::string& message_name,
                          MessageHandler handler);

  // Removes every handler of exactly this filter and unsubscribes.
  Result Unsubscribe(const std::string& topic_filter);

  // Messages that arrived and could not be parsed since the client was created.
  std::uint64_t DroppedMessages() const;

 private:
  struct Impl;
  void OnMessage(const std::string& topic, const std::string& payload) override;
  void OnConnected(bool reconnected) override;
  void OnConnectionLost(const std::string& reason) override;

  std::unique_ptr<Impl> impl_;
};

// True when `topic` matches `filter` under the MQTT rules ('+' one level, '#' the rest).
bool TopicMatches(const std::string& filter, const std::string& topic);

}  // namespace openapi
}  // namespace lixel
