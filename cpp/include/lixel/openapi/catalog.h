// The interface table: every MQTT topic of the OpenAPI with its message type.
// It mirrors python/example/core/api_catalog.py; the catalog_drift test keeps them in step.
#pragma once

#include <cstddef>

namespace lixel {
namespace openapi {

// One functional interface. A topic or message pointer is nullptr when the interface does not
// have that endpoint. An empty message name means the payload is JSON text, not protobuf (this
// is the case for the heartbeat only). All strings have static storage duration.
struct ApiInterface {
  const char* key;            // stable id, for example "device_info"
  const char* request_topic;  // client -> device
  const char*
      request_message;  // protobuf message name without package, for example "DeviceInfoRequest"
  const char* response_topic;  // device -> client, the reply to a request
  const char* response_message;
  const char* report_topic;  // device -> client, pushed by the device
  const char* report_message;
};

// All interfaces, in the order of the Python catalog. Thread safe.
const ApiInterface* Interfaces(std::size_t* count) noexcept;

// Looks an interface up by key; nullptr when unknown.
const ApiInterface* FindInterface(const char* key) noexcept;

// Looks an interface up by any of its topics (exact match); nullptr when unknown.
const ApiInterface* FindInterfaceByTopic(const char* topic) noexcept;

// Message name of a topic, or nullptr when the topic is not in the table. An empty string
// means the payload is JSON text.
const char* MessageForTopic(const char* topic) noexcept;

}  // namespace openapi
}  // namespace lixel
