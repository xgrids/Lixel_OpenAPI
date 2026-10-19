// Example: data unpack (data_unpack)
// - Request topic : lixel/application/request/data_unpack   (DataUnpackRequest)
// - Response topic: lixel/application/response/data_unpack  (DataUnpackResponse)
// - Report topic  : lixel/application/report/data_unpack    (DataUnpackReport)
// Starts the unpacking of a project's xbin data and prints the progress reports until it reaches
// 100 or --watch seconds have passed. Only some devices support data processing.
// Exit status: as RunRequest in common.h, 130 when interrupted.
// The same as python/example/usage/data_unpack.py.
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

#include "common.h"
#include "openapi/aiot_msgs.pb.h"

namespace pbm = ::openapi::aiot_msgs;
using lixel::openapi::Client;
using lixel::openapi::Errc;
using lixel::openapi::Result;
using lixel_examples::Args;

int main(int argc, char** argv) {
  Args args("data unpack");
  args.AddRequiredOption("project", "project name");
  args.AddOption("watch", "30", "seconds to wait for progress reports");
  args.Parse(argc, argv);
  lixel_examples::InstallInterruptHandler();

  std::atomic<bool> done{false};  // declared before the client: handlers may run until it is gone
  Client client(lixel_examples::ClientOptionsFrom(args));
  if (!lixel_examples::ConnectOrReport(&client, args)) return 1;

  Result sub =
      client.SubscribeMessage("lixel/application/report/data_unpack", "DataUnpackReport",
                              [&done](const std::string&, const google::protobuf::Message& m) {
                                lixel_examples::Dump("DataUnpackReport", m);
                                if (static_cast<const pbm::DataUnpackReport&>(m).progress() == 100)
                                  done = true;
                              });
  if (!sub.ok()) {
    std::fprintf(stderr, "error: cannot subscribe: %s: %s\n", lixel::openapi::ErrcName(sub.code),
                 sub.desc.c_str());
    return 1;
  }

  pbm::DataUnpackRequest req;
  pbm::DataUnpackResponse rsp;
  req.set_project_name(args.Str("project"));
  req.set_data_source(pbm::DataUnpackRequest::XBIN_DATA);
  // To unpack only some topics: req.add_data_topics("/lidar/points"); by default all are unpacked.
  Result r = client.Request("lixel/application/request/data_unpack", req,
                            "lixel/application/response/data_unpack", &rsp);
  if (r.ok() || r.code == Errc::DeviceError) lixel_examples::Dump("DataUnpackResponse", rsp);
  if (!r.ok()) {
    std::fprintf(stderr, "error: %s: %s\n", lixel::openapi::ErrcName(r.code), r.desc.c_str());
    return r.code == Errc::DeviceError ? 3 : 1;
  }

  const auto deadline =
      std::chrono::steady_clock::now() +
      std::chrono::milliseconds(static_cast<long long>(args.Double("watch") * 1000));
  const int base = lixel_examples::InterruptCount();
  while (!done && std::chrono::steady_clock::now() < deadline) {
    if (lixel_examples::InterruptCount() > base) return 130;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  return 0;
}
