// Example: subscribe to system error reports (system_error)
// - Report topic: lixel/application/report/system_error  (SystemErrorReport)
// The device reports system error codes and descriptions on its own.
// The same as python/example/usage/system_error.py.
#include "common.h"

using lixel_examples::Args;

int main(int argc, char** argv) {
  Args args("subscribe to system errors");
  args.AddOption("seconds", "15", "subscription duration");
  args.Parse(argc, argv);
  return lixel_examples::RunSubscribe(
      args, "lixel/application/report/system_error", "SystemErrorReport", "SystemErrorReport",
      "Subscribing to system errors for " + lixel_examples::Num(args.Double("seconds")) + "s ...");
}
