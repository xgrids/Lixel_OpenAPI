// Example: subscribe to device status reports (device_status)
// - Report topic: lixel/application/report/device_status  (DeviceStatusReport)
// The device reports mapping status, system resources, RTK status and more at 1 Hz.
// The same as python/example/usage/device_status.py.
#include "common.h"

using lixel_examples::Args;

int main(int argc, char** argv) {
  Args args("subscribe to device status");
  args.AddOption("seconds", "10", "subscription duration");
  args.Parse(argc, argv);
  return lixel_examples::RunSubscribe(
      args, "lixel/application/report/device_status", "DeviceStatusReport", "DeviceStatusReport",
      "Subscribing to device status for " + lixel_examples::Num(args.Double("seconds")) + "s ...");
}
