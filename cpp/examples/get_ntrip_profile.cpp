// Example: get Ntrip profile (get_ntrip_profile)
// - Request topic : lixel/application/request/get_ntrip_profile   (GetNtripProfileRequest)
// - Response topic: lixel/application/response/get_ntrip_profile  (GetNtripProfileResponse)
// The same as python/example/usage/get_ntrip_profile.py.
#include "common.h"
#include "openapi/aiot_msgs.pb.h"

namespace pbm = ::openapi::aiot_msgs;
using lixel_examples::Args;

int main(int argc, char** argv) {
  Args args("get Ntrip profile");
  args.Parse(argc, argv);

  pbm::GetNtripProfileRequest req;
  pbm::GetNtripProfileResponse rsp;
  return lixel_examples::RunRequest(args, "lixel/application/request/get_ntrip_profile", req,
                                    "lixel/application/response/get_ntrip_profile", &rsp,
                                    "GetNtripProfileResponse");
}
