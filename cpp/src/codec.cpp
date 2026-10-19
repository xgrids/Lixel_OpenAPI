#include "lixel/openapi/codec.h"

#include <google/protobuf/descriptor.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/util/json_util.h>

#include "openapi/aiot_msgs.pb.h"

namespace lixel {
namespace openapi {

namespace {

namespace pb = google::protobuf;

// The package of every message in aiot_msgs.proto.
const char kPackage[] = "openapi.aiot_msgs.";

// Generated descriptors register themselves when the object file of the generated code is
// linked in. A static library only contributes object files that something refers to, so refer
// to one generated class here: anything that uses the codec then also gets the registrations.
const pb::Message& KeepGeneratedCodeLinked() {
  return ::openapi::aiot_msgs::Header::default_instance();
}
const bool kLinked = (&KeepGeneratedCodeLinked() != nullptr);

}  // namespace

std::unique_ptr<pb::Message> NewMessage(const std::string& name) {
  (void)kLinked;
  const pb::Descriptor* desc =
      pb::DescriptorPool::generated_pool()->FindMessageTypeByName(kPackage + name);
  if (desc == nullptr) return nullptr;
  const pb::Message* prototype = pb::MessageFactory::generated_factory()->GetPrototype(desc);
  if (prototype == nullptr) return nullptr;
  return std::unique_ptr<pb::Message>(prototype->New());
}

Result JsonToMessage(const std::string& json, pb::Message* msg, bool ignore_unknown_fields) {
  if (msg == nullptr) return Result::Make(Errc::InvalidArgument, "message is null");
  pb::util::JsonParseOptions options;
  options.ignore_unknown_fields = ignore_unknown_fields;
  // Start from an empty message so that a failed parse leaves nothing half filled.
  std::unique_ptr<pb::Message> parsed(msg->New());
  const auto status = pb::util::JsonStringToMessage(json, parsed.get(), options);
  if (!status.ok()) {
    return Result::Make(Errc::DecodeFailed, std::string(status.message()));
  }
  msg->CopyFrom(*parsed);
  return Result();
}

Result MessageToJson(const pb::Message& msg, std::string* json, bool pretty) {
  if (json == nullptr) return Result::Make(Errc::InvalidArgument, "output is null");
  pb::util::JsonPrintOptions options;
  options.add_whitespace = pretty;
  options.always_print_primitive_fields = true;
  options.preserve_proto_field_names = true;
  const auto status = pb::util::MessageToJsonString(msg, json, options);
  if (!status.ok()) return Result::Make(Errc::EncodeFailed, std::string(status.message()));
  return Result();
}

Result DecodeToJson(const std::string& message_name, const std::string& payload, std::string* json,
                    bool pretty) {
  std::unique_ptr<pb::Message> msg = NewMessage(message_name);
  if (!msg) return Result::Make(Errc::InvalidArgument, "unknown message name: " + message_name);
  if (!msg->ParseFromString(payload)) {
    return Result::Make(Errc::DecodeFailed, "payload is not a valid " + message_name);
  }
  return MessageToJson(*msg, json, pretty);
}

Result DefaultJson(const std::string& message_name, std::string* json, bool pretty) {
  std::unique_ptr<pb::Message> msg = NewMessage(message_name);
  if (!msg) return Result::Make(Errc::InvalidArgument, "unknown message name: " + message_name);
  return MessageToJson(*msg, json, pretty);
}

}  // namespace openapi
}  // namespace lixel
