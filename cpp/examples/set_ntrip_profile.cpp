// Example: set Ntrip profile (set_ntrip_profile)
// - Request topic : lixel/application/request/set_ntrip_profile   (SetNtripProfileRequest)
// - Response topic: lixel/application/response/set_ntrip_profile  (SetNtripProfileResponse)
// Logs in to a custom CORS (action AC_LOGIN; AC_LOGOUT is also supported).
// Setting is only allowed when the RTK status is HwConnected. When the device WiFi is in AP
// mode (Ntrip data is forwarded over WebSocket) the response carries error code
// AppCode::EMBError::Unknown; that error can be ignored, the device Ntrip connection is not
// affected. This example then exits with status 3 and prints the response.
// The same as python/example/usage/set_ntrip_profile.py.
#include "common.h"
#include "openapi/aiot_msgs.pb.h"

namespace pbm = ::openapi::aiot_msgs;
using lixel_examples::Args;

int main(int argc, char** argv) {
  Args args("set Ntrip profile");
  args.AddOption("server", "203.107.45.154", "CORS server address");
  args.AddOption("port2", "8002", "CORS port");
  args.AddOption("mount", "RTCM33_GRCEJ", "mount point");
  args.AddOption("ntrip-user", "user", "CORS username");
  args.AddOption("ntrip-pass", "pass", "CORS password");
  args.Parse(argc, argv);

  pbm::SetNtripProfileRequest req;
  pbm::SetNtripProfileResponse rsp;
  req.set_action(pbm::Application::AC_LOGIN);
  req.set_cors(pbm::NtripAttr::CUSTOM);     // CUSTOM / QIANXUN / ZHONGYI
  req.set_frame(pbm::NtripAttr::CGCS2000);  // UNKNOWN / ITRF2008 / WGS84 / CGCS2000
  req.set_server(args.Str("server"));
  req.set_port(args.Int("port2"));
  req.set_mount_point(args.Str("mount"));
  req.set_username(args.Str("ntrip-user"));
  req.set_password(args.Str("ntrip-pass"));
  return lixel_examples::RunRequest(args, "lixel/application/request/set_ntrip_profile", req,
                                    "lixel/application/response/set_ntrip_profile", &rsp,
                                    "SetNtripProfileResponse");
}
