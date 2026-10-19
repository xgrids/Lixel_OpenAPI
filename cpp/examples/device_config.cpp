// Example: device parameter configuration (device_config)
// - Request topic : lixel/application/request/device_config   (DeviceConfigRequest)
// - Response topic: lixel/application/response/device_config  (DeviceConfigResponse)
// Sets the device time and time zone. Optional fields that are not set are not sent.
// To switch the main storage as well, call req.mutable_main_storage_config()->set_storage_type(n)
// (0 built-in, 1 TF card, 2 USB disk).
// The same as python/example/usage/device_config.py.
#include <cstdint>
#include <ctime>

#include "common.h"
#include "openapi/aiot_msgs.pb.h"

namespace pbm = ::openapi::aiot_msgs;
using lixel_examples::Args;

int main(int argc, char** argv) {
  Args args("device parameter configuration");
  args.AddOption("tz", "Asia/Shanghai", "time zone");
  args.Parse(argc, argv);

  pbm::DeviceConfigRequest req;
  pbm::DeviceConfigResponse rsp;
  req.mutable_time_config()->set_time_secs(static_cast<std::int64_t>(std::time(nullptr)));
  req.mutable_time_config()->set_time_zone(args.Str("tz"));
  return lixel_examples::RunRequest(args, "lixel/application/request/device_config", req,
                                    "lixel/application/response/device_config", &rsp,
                                    "DeviceConfigResponse");
}
