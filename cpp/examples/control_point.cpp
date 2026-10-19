// Example: control point operations (control_point)
// - Request topic : lixel/application/request/control_point   (ControlPointRequest)
// - Response topic: lixel/application/response/control_point  (ControlPointResponse)
// - Report topic  : lixel/application/report/control_point    (ControlPointReport)
// action supports AC_ADD / AC_REMOVE. AC_UPDATE is defined in the proto but the device does
// not handle it (it answers EC_ProtoError, "invalid action").
// The same as python/example/usage/control_point.py.
#include "common.h"
#include "openapi/aiot_msgs.pb.h"

namespace pbm = ::openapi::aiot_msgs;
using lixel_examples::Args;

int main(int argc, char** argv) {
  Args args("control point operations");
  args.AddOption("action", "AC_ADD", "AC_ADD / AC_REMOVE");
  args.AddOption("id", "1", "control point ID");
  args.AddOption("label", "CP-1", "control point label");
  args.AddOption("height", "1800", "base point height (mm)");
  args.Parse(argc, argv);

  pbm::ControlPointRequest req;
  pbm::ControlPointResponse rsp;
  req.set_action(lixel_examples::EnumOption<pbm::Application::ActionCmd>(
      args, "action", &pbm::Application_ActionCmd_Parse));
  req.set_id(args.Int("id"));
  req.set_type(0);  // 0 device, 1 centering pole
  req.set_label(args.Str("label"));
  req.set_height(args.Int("height"));
  return lixel_examples::RunRequest(args, "lixel/application/request/control_point", req,
                                    "lixel/application/response/control_point", &rsp,
                                    "ControlPointResponse");
}
