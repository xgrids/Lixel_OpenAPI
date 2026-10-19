// Example: set RTK advanced configuration (set_rtk_advance)
// - Request topic : lixel/application/request/set_rtk_advance   (SetRtkAdvanceRequest)
// - Response topic: lixel/application/response/set_rtk_advance  (SetRtkAdvanceResponse)
// Sets the RTK type and the satellite systems. rtk_register_code and rtk_control_mode apply to
// the L2PS1 only and are not sent here.
// The same as python/example/usage/set_rtk_advance.py.
#include "common.h"
#include "openapi/aiot_msgs.pb.h"

namespace pbm = ::openapi::aiot_msgs;
using lixel_examples::Args;

int main(int argc, char** argv) {
  Args args("set RTK advanced configuration");
  args.AddOption("rtk-type", "RTK", "RTK / PPK / DJI");
  args.AddList("systems", {"GPS", "BDS", "GLO", "GAL"}, "satellite system list");
  args.Parse(argc, argv);

  pbm::SetRtkAdvanceRequest req;
  pbm::SetRtkAdvanceResponse rsp;
  req.set_rtk_type(
      lixel_examples::EnumOption<pbm::RtkAttr::Type>(args, "rtk-type", &pbm::RtkAttr_Type_Parse));
  for (const std::string& system : args.List("systems")) req.add_satellite_system(system);
  return lixel_examples::RunRequest(args, "lixel/application/request/set_rtk_advance", req,
                                    "lixel/application/response/set_rtk_advance", &rsp,
                                    "SetRtkAdvanceResponse");
}
