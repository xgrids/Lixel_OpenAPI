// Start a scan, wait until the device really scans, stop it, wait until the device is in standby.
//
// This is the flow of python/example/usage/modeling.py. Starting and stopping are asynchronous on
// the device: the response to AC_START only says the request was accepted, and the device then
// goes standby -> starting -> scanning over several seconds. A stop request sent before it is
// scanning is deferred until the start sequence has finished. So the flow watches
// DeviceStatusReport.modeling_state (topic lixel/application/report/device_status, once per
// second) instead of sleeping for a fixed time.
//
// RunScan blocks the calling thread. The client must be connected, and RunScan must not be called
// from a subscription handler (Client::Request refuses that).
#pragma once
#include <chrono>
#include <cstdint>
#include <functional>
#include <string>

#include "lixel/openapi/client.h"
#include "lixel/openapi/result.h"
#include "openapi/aiot_msgs.pb.h"
namespace lixel {
namespace openapi {
// What happened. The numbers are the exit codes of the scan examples.
enum class ScanOutcome {
  Completed = 0,      // scanned, stopped, and the device is in standby again
  Timeout = 1,        // the device did not reach scanning or standby in time
  NotStandby = 2,     // the device was not in standby, nothing was sent
  StartRejected = 3,  // the device answered AC_START with an error code
  NoResponse = 4,     // no answer to AC_START (nothing is stopped) or to AC_STOP (the device
                      // may still be scanning)
  Error = 5,          // not connected, or a request could not be built or sent
  Interrupted = 130,  // the caller asked to stop
};
const char* ScanOutcomeName(ScanOutcome outcome) noexcept;
struct ScanOptions {
  // The project the scan is stored in on the device.
  std::string project_name = "demo_project";
  ::openapi::aiot_msgs::Modeling::RecordMode record_mode =
      ::openapi::aiot_msgs::Modeling::RM_RECORD_CALC;
  ::openapi::aiot_msgs::Modeling::ScanMode scan_mode = ::openapi::aiot_msgs::Modeling::SM_PCL;
  ::openapi::aiot_msgs::Modeling::MountType mount_type = ::openapi::aiot_msgs::Modeling::MT_HAND;

  // How long to wait for the device to be in standby before sending AC_START.
  std::chrono::milliseconds ready_timeout{10000};
  // How long to wait for scanning after AC_START was accepted.
  std::chrono::milliseconds start_timeout{90000};
  // How long to stay in scanning state before stopping.
  std::chrono::milliseconds watch{5000};
  // How long to wait for standby after AC_STOP. Finishing a scan can take minutes.
  std::chrono::milliseconds stop_timeout{180000};

  // Returns how many times the user has asked to stop (0 at the start; a signal handler can
  // increment a counter). It is polled about every 50 ms while waiting, and once more after each
  // request returns. The first increase makes the flow stop the scan. When it increases again
  // while the flow waits for standby after AC_STOP, the flow gives up waiting. A request that is
  // already waiting for its response is not interrupted: it ends within the client's
  // request_timeout. May be empty.
  std::function<int()> interrupt_count;

  // Called with one line of progress text, for example "state: scanning". It runs on the calling
  // thread or on the thread that delivers the device status reports, never two at a time. May be
  // empty.
  std::function<void(const std::string& line)> on_progress;
};
struct ScanReport {
  ScanOutcome outcome = ScanOutcome::Error;
  // For humans: what went wrong, and what to check on the device. Empty for Completed.
  std::string message;
  bool start_sent = false;       // AC_START left this process
  bool stop_sent = false;        // AC_STOP left this process
  bool scanning_seen = false;    // the device reported the scanning state
  std::uint32_t last_state = 0;  // last modeling_state received, 0 when none
  // The answers, valid when the matching result is Ok or DeviceError.
  Result start_result;
  Result stop_result;
  ::openapi::aiot_msgs::ModelingResponse start_response;
  ::openapi::aiot_msgs::ModelingResponse stop_response;
  int exit_code() const noexcept { return static_cast<int>(outcome); }
};
// Runs the whole flow.
//
//   1. subscribe to the device status, wait for standby (ready_timeout) -> NotStandby
//   2. send AC_START: no answer -> NoResponse (and no AC_STOP, the request may not have arrived),
//      error code -> StartRejected
//   3. wait for scanning (start_timeout), then stay `watch` in it
//   4. send AC_STOP, always once AC_START was accepted, also after a start timeout or an interrupt
//   5. wait for a standby report that arrived after AC_STOP was answered (stop_timeout)
//
// An AC_STOP that gets no answer ends the flow with NoResponse (Interrupted when the caller had
// asked to stop) and a message that the device may still be scanning.
// An AC_STOP that the device answers with an error code does not end the flow: it still waits for
// standby, because the device may simply not have been scanning any more. The code is in
// ScanReport::stop_result.
ScanReport RunScan(Client& client, const ScanOptions& options);
}  // namespace openapi
}  // namespace lixel
