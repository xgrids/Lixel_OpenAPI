// Example: query device info (device_info)
// - Request topic : lixel/application/request/device_info   (DeviceInfoRequest)
// - Response topic: lixel/application/response/device_info  (DeviceInfoResponse)
// The simplest call: an empty request returns the full device info.
// The same as python/example/usage/device_info.py.
#include "common.h"
#include "openapi/aiot_msgs.pb.h"

namespace pbm = ::openapi::aiot_msgs;
using lixel_examples::Args;

int main(int argc, char** argv) {
  Args args("query device info");
  args.Parse(argc, argv);

  pbm::DeviceInfoRequest req;
  pbm::DeviceInfoResponse rsp;
  // Only the header is needed; the client fills in the OpenAPI key.
  return lixel_examples::RunRequest(args, "lixel/application/request/device_info", req,
                                    "lixel/application/response/device_info", &rsp,
                                    "DeviceInfoResponse");
}
