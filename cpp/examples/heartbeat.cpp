// Example: subscribe to the device heartbeat (heartbeat)
// - Report topic: lixel/application/report/heartbeat  (JSON text, not protobuf)
// The payload is a JSON string that can be used for device discovery, for example:
//   {"DeviceSN":"A256A60000","Timestamp":1697703246.5,"ProtocolVersion":"3.0.0"}
// The same as python/example/usage/heartbeat.py.
#include "common.h"

using lixel_examples::Args;

int main(int argc, char** argv) {
  Args args("subscribe to device heartbeat");
  args.AddOption("seconds", "15", "subscription duration");
  args.Parse(argc, argv);
  return lixel_examples::RunSubscribe(args, "lixel/application/report/heartbeat", "", "heartbeat",
                                      "Subscribing to heartbeat for " +
                                          lixel_examples::Num(args.Double("seconds")) +
                                          "s ... (Ctrl+C to exit)");
}
