// Example: get RTK advanced configuration (get_rtk_advance)
// - Request topic : lixel/application/request/get_rtk_advance   (GetRtkAdvanceRequest)
// - Response topic: lixel/application/response/get_rtk_advance  (GetRtkAdvanceResponse)
// Returns the RTK type and the enabled satellite systems.
// The same as python/example/usage/get_rtk_advance.py.
#include "common.h"
#include "openapi/aiot_msgs.pb.h"

namespace pbm = ::openapi::aiot_msgs;
using lixel_examples::Args;

int main(int argc, char** argv) {
  Args args("get RTK advanced configuration");
  args.Parse(argc, argv);

  pbm::GetRtkAdvanceRequest req;
  pbm::GetRtkAdvanceResponse rsp;
  return lixel_examples::RunRequest(args, "lixel/application/request/get_rtk_advance", req,
                                    "lixel/application/response/get_rtk_advance", &rsp,
                                    "GetRtkAdvanceResponse");
}
