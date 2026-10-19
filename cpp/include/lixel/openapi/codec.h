// Convert between JSON text and protobuf messages, looking the message up by name.
// This is the C++ counterpart of the Python ProtoCodec.
//
// Message names are the names in aiot_msgs.proto without the package, for example
// "ModelingRequest". All functions are thread safe.
#pragma once

#include <google/protobuf/message.h>

#include <memory>
#include <string>

#include "lixel/openapi/result.h"

namespace lixel {
namespace openapi {

// Creates an empty message of the named type; nullptr when the name is unknown.
std::unique_ptr<google::protobuf::Message> NewMessage(const std::string& name);

// Parses `json` into `*msg`. Unknown field names and unknown enum names are errors by default,
// so a typo such as "AC_STRAT" is reported instead of being dropped silently (the Python codec
// drops them). Pass ignore_unknown_fields = true to get the lenient behaviour.
// Errc::DecodeFailed on failure, with the parser's explanation in Result::desc.
Result JsonToMessage(const std::string& json, google::protobuf::Message* msg,
                     bool ignore_unknown_fields = false);

// Writes `msg` as JSON text. Fields with default values are included and the proto field names
// are kept (snake_case), the same as the Python codec. 64-bit integers are strings, as the
// protobuf JSON mapping requires.
Result MessageToJson(const google::protobuf::Message& msg, std::string* json, bool pretty = true);

// Parses `payload` as the named message and writes it as JSON.
Result DecodeToJson(const std::string& message_name, const std::string& payload, std::string* json,
                    bool pretty = true);

// JSON of an empty message of the named type, with every field shown: a template to start from.
Result DefaultJson(const std::string& message_name, std::string* json, bool pretty = true);

}  // namespace openapi
}  // namespace lixel
