#include "lixel/openapi/result.h"

#include <cstdio>

#include "lixel/openapi/codes.h"

namespace lixel {
namespace openapi {

const char* ErrcName(Errc code) noexcept {
  switch (code) {
    case Errc::Ok:
      return "Ok";
    case Errc::NotConnected:
      return "NotConnected";
    case Errc::ConnectFailed:
      return "ConnectFailed";
    case Errc::ConnectTimeout:
      return "ConnectTimeout";
    case Errc::RequestTimeout:
      return "RequestTimeout";
    case Errc::EncodeFailed:
      return "EncodeFailed";
    case Errc::DecodeFailed:
      return "DecodeFailed";
    case Errc::TransportError:
      return "TransportError";
    case Errc::CalledFromCallback:
      return "CalledFromCallback";
    case Errc::DeviceError:
      return "DeviceError";
    case Errc::Unsupported:
      return "Unsupported";
    case Errc::InvalidArgument:
      return "InvalidArgument";
    case Errc::Interrupted:
      return "Interrupted";
  }
  return "Unknown";
}

const char* StateName(std::uint32_t state) noexcept {
  switch (state) {
    case kScanReady:
      return "standby";
    case kScanStarting:
      return "starting";
    case kScanModeling:
      return "scanning";
    case kScanStopping:
      return "stopping";
    case kScanStopped:
      return "stopped";
    case kScanNotAllowedInUsbMass:
      return "scan not allowed (USB mass storage mode)";
  }
  thread_local char buffer[16];
  std::snprintf(buffer, sizeof(buffer), "0x%X", static_cast<unsigned>(state));
  return buffer;
}

}  // namespace openapi
}  // namespace lixel
