// Example: scan control (modeling)
// - Request topic : lixel/application/request/modeling    (ModelingRequest)
// - Response topic: lixel/application/response/modeling   (ModelingResponse)
// - Report topic  : lixel/application/report/modeling     (ModelingReport)
// - Report topic  : lixel/application/report/device_status (DeviceStatusReport, 1 Hz)
// The complete flow: start scanning -> wait until the device really scans -> watch the scan
// reports -> stop scanning -> wait until the device is in standby again. The flow itself is
// lixel::openapi::RunScan (lixel/openapi/scan.h); this file only connects, prints and maps the
// outcome to an exit status.
//
// Starting and stopping are asynchronous: the response to AC_START only says the request was
// accepted, the device then goes standby -> starting -> scanning over several seconds, and a stop
// request sent before it scans is deferred until the start sequence is finished.
//
// Exit status: 0 success; 1 timeout while starting or stopping; 2 the device was not in standby
// (or a bad option); 3 the device rejected AC_START; 4 no response to AC_START or AC_STOP;
// 5 not connected or a request could not be sent; 130 interrupted with Ctrl-C. Once AC_START was
// sent and accepted, AC_STOP is always sent before the program ends.
// The same as python/example/usage/modeling.py.
#include <chrono>
#include <cstdio>

#include "common.h"
#include "lixel/openapi/scan.h"
#include "openapi/aiot_msgs.pb.h"

using lixel::openapi::Client;
using lixel::openapi::Errc;
using lixel::openapi::ScanOptions;
using lixel::openapi::ScanReport;
using lixel_examples::Args;

namespace {
std::chrono::milliseconds Seconds(double s) {
  return std::chrono::milliseconds(static_cast<long long>(s * 1000));
}
bool HasAnswer(const lixel::openapi::Result& r) { return r.ok() || r.code == Errc::DeviceError; }
}  // namespace

int main(int argc, char** argv) {
  Args args("scan control");
  args.AddOption("project", "demo_project", "project name");
  args.AddOption("watch", "5", "seconds to watch scan reports once the device is scanning");
  args.AddOption("start-timeout", "90", "max seconds to wait for the device to start scanning");
  args.AddOption("stop-timeout", "180", "max seconds to wait for the device to return to standby");
  args.Parse(argc, argv);
  lixel_examples::InstallInterruptHandler();

  Client client(lixel_examples::ClientOptionsFrom(args));
  if (!lixel_examples::ConnectOrReport(&client, args)) return 1;

  // Scan status reports: only to show them. RunScan watches the device status by itself.
  lixel::openapi::Result sub =
      client.SubscribeMessage("lixel/application/report/modeling", "ModelingReport",
                              [](const std::string&, const google::protobuf::Message& m) {
                                lixel_examples::Dump("ModelingReport", m);
                              });
  if (!sub.ok()) {
    std::fprintf(stderr, "error: cannot subscribe: %s: %s\n", lixel::openapi::ErrcName(sub.code),
                 sub.desc.c_str());
    return 1;
  }

  ScanOptions options;
  options.project_name = args.Str("project");
  options.watch = Seconds(args.Double("watch"));
  options.start_timeout = Seconds(args.Double("start-timeout"));
  options.stop_timeout = Seconds(args.Double("stop-timeout"));
  options.interrupt_count = [] { return lixel_examples::InterruptCount(); };
  options.on_progress = [](const std::string& line) {
    if (line.compare(0, 7, "state: ") == 0) {
      std::printf("  [state] %s\n", line.c_str() + 7);
    } else {
      std::printf("%s\n", line.c_str());
    }
  };

  ScanReport report = lixel::openapi::RunScan(client, options);
  if (HasAnswer(report.start_result)) {
    lixel_examples::Dump("ModelingResponse (start)", report.start_response);
  }
  if (HasAnswer(report.stop_result)) {
    lixel_examples::Dump("ModelingResponse (stop)", report.stop_response);
  }
  if (!report.message.empty()) {
    std::fprintf(stderr, "%s: %s\n", lixel::openapi::ScanOutcomeName(report.outcome),
                 report.message.c_str());
  }
  return report.exit_code();
}
