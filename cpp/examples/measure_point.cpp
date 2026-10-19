// Example: measure point operations (measure_point)
// - Request topic : lixel/application/request/measure_point   (MeasurePointRequest)
// - Response topic: lixel/application/response/measure_point  (MeasurePointResponse)
// action supports AC_ADD / AC_REMOVE. Only some devices support measure points.
// The same as python/example/usage/measure_point.py.
#include "common.h"
#include "openapi/aiot_msgs.pb.h"

namespace pbm = ::openapi::aiot_msgs;
using lixel_examples::Args;

int main(int argc, char** argv) {
  Args args("measure point operations");
  args.AddOption("action", "AC_ADD", "AC_ADD / AC_REMOVE");
  args.AddOption("id", "1", "measure point ID");
  args.AddOption("label", "MP-1", "measure point label");
  args.Parse(argc, argv);

  pbm::MeasurePointRequest req;
  pbm::MeasurePointResponse rsp;
  req.set_action(lixel_examples::EnumOption<pbm::Application::ActionCmd>(
      args, "action", &pbm::Application_ActionCmd_Parse));
  req.set_id(args.Int("id"));
  req.set_label(args.Str("label"));
  return lixel_examples::RunRequest(args, "lixel/application/request/measure_point", req,
                                    "lixel/application/response/measure_point", &rsp,
                                    "MeasurePointResponse");
}
