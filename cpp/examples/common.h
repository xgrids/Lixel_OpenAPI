// Helpers shared by the command line examples: option parsing, connecting, printing, Ctrl-C.
// The options are the same as in python/example/usage/_common.py, plus --ca-file and --insecure
// for TLS (see below).
#pragma once
#include <google/protobuf/message.h>

#include <map>
#include <string>
#include <vector>

#include "lixel/openapi/client.h"

namespace lixel_examples {

// A small command line parser: --name value, --name=value, flags, and lists.
// Every example starts with the connection options (--host --port --tls --username --password
// --key --timeout --ca-file --insecure) and adds its own.
// Exit status of the process: --help exits with 0, a bad option or value with 2.
class Args {
 public:
  explicit Args(std::string description);

  void AddOption(const std::string& name, const std::string& default_value,
                 const std::string& help);
  void AddRequiredOption(const std::string& name, const std::string& help);
  void AddFlag(const std::string& name, const std::string& help);
  // Takes every following word up to the next option, like argparse nargs="*".
  void AddList(const std::string& name, const std::vector<std::string>& defaults,
               const std::string& help);

  void Parse(int argc, char** argv);

  std::string Str(const std::string& name) const;
  int Int(const std::string& name) const;
  double Double(const std::string& name) const;
  bool Flag(const std::string& name) const;
  std::vector<std::string> List(const std::string& name) const;

 private:
  struct Spec {
    enum Kind { Option, Required, Flag, List } kind;
    std::string help;
    std::string default_value;
    std::vector<std::string> default_list;
  };
  [[noreturn]] void Fail(const std::string& message) const;
  void PrintHelp(const char* program) const;
  const Spec& SpecOf(const std::string& name) const;

  std::string description_;
  std::vector<std::string> order_;
  std::map<std::string, Spec> specs_;
  std::map<std::string, std::string> values_;
  std::map<std::string, std::vector<std::string>> lists_;
  std::map<std::string, bool> flags_;
};

lixel::openapi::ClientOptions ClientOptionsFrom(const Args& args);

// Connects, or prints why not (with a hint for TLS) and returns false.
bool ConnectOrReport(lixel::openapi::Client* client, const Args& args);

// Prints "\n===== title =====\n" followed by the message as JSON.
void Dump(const std::string& title, const google::protobuf::Message& message);
void DumpText(const std::string& title, const std::string& text);

// The whole of a one-shot example: connect, send `request`, print the response.
// Exit status: 0 the device accepted it, 1 could not connect or no response, 3 the device answered
// with an error code (the response is printed anyway).
int RunRequest(const Args& args, const std::string& request_topic,
               const google::protobuf::Message& request, const std::string& response_topic,
               google::protobuf::Message* response, const std::string& title);

// The whole of a subscribe-and-print example. message_name == "" means the payload is text.
int RunSubscribe(const Args& args, const std::string& topic, const std::string& message_name,
                 const std::string& title, const std::string& banner);

[[noreturn]] void BadValue(const std::string& option, const std::string& value);

// Parses a protobuf enum name given on the command line, for example "AC_ADD". A name that does
// not exist ends the process with status 2 and a message, instead of sending a request the
// device cannot understand. `parse` is the generated Xxx_Parse function of the enum.
template <typename Enum, typename ParseFn>
Enum EnumOption(const Args& args, const std::string& option, ParseFn parse) {
  Enum value;
  if (!parse(args.Str(option), &value)) BadValue(option, args.Str(option));
  return value;
}

// A number for messages: 10 stays "10", 2.5 stays "2.5".
std::string Num(double value);

// Ctrl-C (SIGINT) and SIGTERM are counted, never act by themselves.
void InstallInterruptHandler();
int InterruptCount();
// Sleeps `seconds` or until an interrupt. Returns false when interrupted.
bool SleepUnlessInterrupted(double seconds);

}  // namespace lixel_examples
