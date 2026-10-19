#include "lixel/openapi/version.h"

#include <google/protobuf/stubs/common.h>

#include <cstdio>

namespace lixel {
namespace openapi {

const char* VersionString() noexcept { return LIXEL_OPENAPI_VERSION_STRING; }

const char* ProtobufVersionString() noexcept {
  // GOOGLE_PROTOBUF_VERSION is major * 1000000 + minor * 1000 + patch. It reflects
  // the headers this library was compiled with, not a runtime loaded later.
  static const char* const text = [] {
    static char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%d.%d.%d", GOOGLE_PROTOBUF_VERSION / 1000000,
                  (GOOGLE_PROTOBUF_VERSION / 1000) % 1000, GOOGLE_PROTOBUF_VERSION % 1000);
    return static_cast<const char*>(buffer);
  }();
  return text;
}

}  // namespace openapi
}  // namespace lixel
