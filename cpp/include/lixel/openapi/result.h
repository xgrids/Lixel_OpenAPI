// Result of a library call.
#pragma once

#include <cstdint>
#include <string>
#include <utility>

namespace lixel {
namespace openapi {

// Why a call did not succeed. Errc::Ok is the only success value.
enum class Errc {
  Ok = 0,
  NotConnected,        // the client is not connected
  ConnectFailed,       // the broker refused or could not be reached
  ConnectTimeout,      // no answer from the broker within connect_timeout
  RequestTimeout,      // no matching response within the timeout
  EncodeFailed,        // the request could not be serialized
  DecodeFailed,        // a payload could not be parsed
  TransportError,      // publish, subscribe or disconnect failed in the MQTT layer
  CalledFromCallback,  // a blocking call was made from inside a subscription callback
  DeviceError,         // the device answered with an error code, see Result::device_code
  Unsupported,         // the feature is not implemented, for example Zstd compression
  InvalidArgument,     // a value was missing or out of range
  Interrupted,         // the operation was cancelled by the caller
};

// Name of an Errc value, for logging. Never returns nullptr.
const char* ErrcName(Errc code) noexcept;

struct Result {
  Errc code = Errc::Ok;
  // Error code from the device (Error.code of the response). Set when code == DeviceError,
  // and also when a response carried a device code that was accepted (then it is 0x12040001
  // or 0x12040002). 0 when no response was involved.
  std::uint32_t device_code = 0;
  // Text for humans: the device's Error.desc, or a description of the local failure.
  std::string desc;

  bool ok() const noexcept { return code == Errc::Ok; }
  explicit operator bool() const noexcept { return ok(); }

  static Result Make(Errc code, std::string desc = std::string()) {
    Result r;
    r.code = code;
    r.desc = std::move(desc);
    return r;
  }
};

}  // namespace openapi
}  // namespace lixel
