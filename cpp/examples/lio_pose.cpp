// Example: subscribe to real-time pose reports (lio_pose)
// - Report topic: lixel/application/report/lio_pose  (LioPoseReport)
// While scanning, the device reports its real-time LIO pose (position and attitude
// quaternion) and the accumulated distance.
// The same as python/example/usage/lio_pose.py.
#include "common.h"

using lixel_examples::Args;

int main(int argc, char** argv) {
  Args args("subscribe to real-time pose");
  args.AddOption("seconds", "10", "subscription duration");
  args.Parse(argc, argv);
  return lixel_examples::RunSubscribe(
      args, "lixel/application/report/lio_pose", "LioPoseReport", "LioPoseReport",
      "Subscribing to real-time pose for " + lixel_examples::Num(args.Double("seconds")) +
          "s ... (device must be scanning)");
}
