// Example: query the OpenAPI authorization (openapi_license)
// - Request topic : lixel/application/request/openapi_license   (OpenapiLicenseRequest)
// - Response topic: lixel/application/response/openapi_license  (OpenapiLicenseResponse)
// Only AC_GET_CONFIG is sent. No OpenAPI key is needed. The response carries the state
// (none / active / expired) and the remaining minutes (-1 means permanent).
// Only devices whose heartbeat Features contains "openapi_license" reply.
// The same as python/example/usage/openapi_license.py.
#include "common.h"
#include "openapi/aiot_msgs.pb.h"

namespace pbm = ::openapi::aiot_msgs;
using lixel_examples::Args;

int main(int argc, char** argv) {
  Args args("query the OpenAPI authorization");
  args.Parse(argc, argv);

  pbm::OpenapiLicenseRequest req;
  pbm::OpenapiLicenseResponse rsp;
  req.set_action(pbm::Application::AC_GET_CONFIG);
  return lixel_examples::RunRequest(args, "lixel/application/request/openapi_license", req,
                                    "lixel/application/response/openapi_license", &rsp,
                                    "OpenapiLicenseResponse");
}
