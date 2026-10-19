// Example: query the file URLs of the current project (modeling_status)
// - Request topic : lixel/application/request/modeling_status   (ModelingStatusRequest)
// - Response topic: lixel/application/response/modeling_status  (ModelingStatusResponse)
// Output URLs (pose, control points, measure points) of the current or most recent project.
// The same as python/example/usage/modeling_status.py.
#include "common.h"
#include "openapi/aiot_msgs.pb.h"

namespace pbm = ::openapi::aiot_msgs;
using lixel_examples::Args;

int main(int argc, char** argv) {
  Args args("query the file URLs of the current project");
  args.Parse(argc, argv);

  pbm::ModelingStatusRequest req;
  pbm::ModelingStatusResponse rsp;
  return lixel_examples::RunRequest(args, "lixel/application/request/modeling_status", req,
                                    "lixel/application/response/modeling_status", &rsp,
                                    "ModelingStatusResponse");
}
