// Device status codes used by the client. Values come from the device's ErrorCode enum:
// the state class uses base 0x12040000, warnings 0x22040000, errors 0x32040000.
// The full list is in the protocol reference (section "Error codes").
#pragma once

#include <cstdint>

namespace lixel {
namespace openapi {

// Error.code values that mean a request was accepted (EC_Success, EC_SuccessAndReboot).
constexpr std::uint32_t kSuccess = 0x12040001u;
constexpr std::uint32_t kSuccessAndReboot = 0x12040002u;

// DeviceStatusReport.modeling_state, reported once per second.
constexpr std::uint32_t kScanReady = 0x1204012Cu;     // EC_ScanReady, standby
constexpr std::uint32_t kScanStarting = 0x1204012Du;  // EC_ScanStarting
constexpr std::uint32_t kScanModeling = 0x1204012Eu;  // EC_ScanModeling, scanning
constexpr std::uint32_t kScanStopping = 0x1204012Fu;  // EC_ScanStopping
constexpr std::uint32_t kScanStopped = 0x12040130u;   // EC_ScanStopped
// Warning class: the device refuses to scan while in this state.
constexpr std::uint32_t kScanNotAllowedInUsbMass = 0x2204013Eu;  // USB mass storage mode

// True when a device Error.code means the request was accepted.
constexpr bool IsSuccessCode(std::uint32_t code) noexcept {
  return code == kSuccess || code == kSuccessAndReboot;
}

// Short English name of a modeling_state value, or its hexadecimal form when unknown.
// The returned pointer stays valid until the next call from the same thread.
const char* StateName(std::uint32_t state) noexcept;

}  // namespace openapi
}  // namespace lixel
