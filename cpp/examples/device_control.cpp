// Example: device control (device_control)
// - Request topic : lixel/application/request/device_control   (DeviceControlRequest)
// - Response topic: lixel/application/response/device_control  (DeviceControlResponse)
// Actions: AC_USB_MASS / AC_USB_ETHER / AC_DISK_FORMAT / AC_POWER_OFF / AC_SYSTEM_REBOOT.
// AC_POWER_OFF, AC_SYSTEM_REBOOT and AC_DISK_FORMAT are high-risk: run them carefully on a
// real device. The relatively safe AC_USB_ETHER is the default.
// The same as python/example/usage/device_control.py.
#include "common.h"
#include "openapi/aiot_msgs.pb.h"

namespace pbm = ::openapi::aiot_msgs;
using lixel_examples::Args;

int main(int argc, char** argv) {
  Args args("device control");
  args.AddOption("action", "AC_USB_ETHER",
                 "AC_USB_MASS / AC_USB_ETHER / AC_DISK_FORMAT / AC_POWER_OFF / AC_SYSTEM_REBOOT");
  args.Parse(argc, argv);

  pbm::DeviceControlRequest req;
  pbm::DeviceControlResponse rsp;
  req.set_action(lixel_examples::EnumOption<pbm::Application::ActionCmd>(
      args, "action", &pbm::Application_ActionCmd_Parse));
  return lixel_examples::RunRequest(args, "lixel/application/request/device_control", req,
                                    "lixel/application/response/device_control", &rsp,
                                    "DeviceControlResponse");
}
