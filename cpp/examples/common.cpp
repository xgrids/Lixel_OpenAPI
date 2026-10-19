#include "common.h"

#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

#include "lixel/openapi/codec.h"
#include "lixel/openapi/codes.h"

namespace lixel_examples {

using lixel::openapi::Client;
using lixel::openapi::ClientOptions;
using lixel::openapi::Errc;
using lixel::openapi::Result;

namespace {

volatile std::sig_atomic_t g_interrupts = 0;
extern "C" void OnSignal(int) { g_interrupts = g_interrupts + 1; }

bool StartsWith(const std::string& s, const char* prefix) {
  return s.compare(0, std::strlen(prefix), prefix) == 0;
}

std::string Hex(std::uint32_t v) {
  char buf[16];
  std::snprintf(buf, sizeof(buf), "0x%08X", static_cast<unsigned>(v));
  return buf;
}

}  // namespace

// ---------------------------------------------------------------------------
// Args
// ---------------------------------------------------------------------------

Args::Args(std::string description) : description_(std::move(description)) {
  AddOption("host", "192.168.55.1", "device address");
  AddOption("port", "1883", "MQTT port");
  AddFlag("tls", "use MQTTS (TLS)");
  AddOption("username", "", "MQTT username");
  AddOption("password", "", "MQTT password");
  AddOption("key", "", "OpenAPI Key");
  AddOption("timeout", "10", "request timeout (seconds)");
  AddOption("ca-file", "", "PEM file with the device certificate to trust (TLS)");
  AddFlag("insecure", "do not verify the device certificate (TLS)");
}

void Args::AddOption(const std::string& name, const std::string& default_value,
                     const std::string& help) {
  specs_[name] = Spec{Spec::Option, help, default_value, {}};
  order_.push_back(name);
}

void Args::AddRequiredOption(const std::string& name, const std::string& help) {
  specs_[name] = Spec{Spec::Required, help, "", {}};
  order_.push_back(name);
}

void Args::AddFlag(const std::string& name, const std::string& help) {
  specs_[name] = Spec{Spec::Flag, help, "", {}};
  order_.push_back(name);
}

void Args::AddList(const std::string& name, const std::vector<std::string>& defaults,
                   const std::string& help) {
  specs_[name] = Spec{Spec::List, help, "", defaults};
  order_.push_back(name);
}

const Args::Spec& Args::SpecOf(const std::string& name) const {
  auto it = specs_.find(name);
  if (it == specs_.end()) {
    std::fprintf(stderr, "internal error: option --%s was never declared\n", name.c_str());
    std::exit(70);
  }
  return it->second;
}

void Args::Fail(const std::string& message) const {
  std::fprintf(stderr, "error: %s\nTry --help.\n", message.c_str());
  std::exit(2);
}

void Args::PrintHelp(const char* program) const {
  std::printf("usage: %s [options]\n\n%s\n\noptions:\n", program, description_.c_str());
  for (const std::string& name : order_) {
    const Spec& s = specs_.at(name);
    std::string left = "--" + name;
    if (s.kind == Spec::Option || s.kind == Spec::Required) left += " VALUE";
    if (s.kind == Spec::List) left += " [VALUE ...]";
    std::string right = s.help;
    if (s.kind == Spec::Required) right += " (required)";
    if (s.kind == Spec::Option && !s.default_value.empty())
      right += " (default: " + s.default_value + ")";
    if (s.kind == Spec::List && !s.default_list.empty()) {
      right += " (default:";
      for (const auto& d : s.default_list) right += " " + d;
      right += ")";
    }
    std::printf("  %-26s %s\n", left.c_str(), right.c_str());
  }
  std::printf("  %-26s %s\n", "--help", "show this help");
}

void Args::Parse(int argc, char** argv) {
  std::setvbuf(stdout, nullptr, _IOLBF, 0);  // lines appear at once when piped
  const char* program = argc > 0 ? argv[0] : "example";
  for (int i = 1; i < argc; ++i) {
    std::string word = argv[i];
    if (word == "--help" || word == "-h") {
      PrintHelp(program);
      std::exit(0);
    }
    if (!StartsWith(word, "--")) Fail("unexpected argument: " + word);
    std::string name = word.substr(2);
    std::string inline_value;
    bool has_inline = false;
    const std::size_t eq = name.find('=');
    if (eq != std::string::npos) {
      inline_value = name.substr(eq + 1);
      name = name.substr(0, eq);
      has_inline = true;
    }
    auto it = specs_.find(name);
    if (it == specs_.end()) Fail("unknown option: --" + name);
    switch (it->second.kind) {
      case Spec::Flag:
        if (has_inline) Fail("--" + name + " takes no value");
        flags_[name] = true;
        break;
      case Spec::Option:
      case Spec::Required:
        if (has_inline) {
          values_[name] = inline_value;
        } else {
          if (i + 1 >= argc) Fail("--" + name + " needs a value");
          values_[name] = argv[++i];
        }
        break;
      case Spec::List: {
        std::vector<std::string> items;
        if (has_inline) items.push_back(inline_value);
        while (i + 1 < argc && !StartsWith(argv[i + 1], "--")) items.push_back(argv[++i]);
        lists_[name] = items;
        break;
      }
    }
  }
  for (const auto& kv : specs_) {
    if (kv.second.kind == Spec::Required && values_.count(kv.first) == 0) {
      Fail("--" + kv.first + " is required");
    }
  }
}

std::string Args::Str(const std::string& name) const {
  const Spec& s = SpecOf(name);
  auto it = values_.find(name);
  return it != values_.end() ? it->second : s.default_value;
}

int Args::Int(const std::string& name) const {
  const std::string text = Str(name);
  char* end = nullptr;
  const long v = std::strtol(text.c_str(), &end, 10);
  if (text.empty() || end == nullptr || *end != '\0') {
    Fail("--" + name + " needs an integer, got \"" + text + "\"");
  }
  return static_cast<int>(v);
}

double Args::Double(const std::string& name) const {
  const std::string text = Str(name);
  char* end = nullptr;
  const double v = std::strtod(text.c_str(), &end);
  if (text.empty() || end == nullptr || *end != '\0') {
    Fail("--" + name + " needs a number, got \"" + text + "\"");
  }
  return v;
}

bool Args::Flag(const std::string& name) const {
  SpecOf(name);
  auto it = flags_.find(name);
  return it != flags_.end() && it->second;
}

std::vector<std::string> Args::List(const std::string& name) const {
  const Spec& s = SpecOf(name);
  auto it = lists_.find(name);
  return it != lists_.end() ? it->second : s.default_list;
}

// ---------------------------------------------------------------------------
// Connecting and printing
// ---------------------------------------------------------------------------

std::string Num(double value) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%g", value);
  return buf;
}

void BadValue(const std::string& option, const std::string& value) {
  std::fprintf(stderr, "error: --%s: \"%s\" is not a valid value\n", option.c_str(), value.c_str());
  std::exit(2);
}

ClientOptions ClientOptionsFrom(const Args& args) {
  ClientOptions o;
  o.transport.host = args.Str("host");
  const int port = args.Int("port");
  if (port <= 0 || port > 65535) {
    std::fprintf(stderr, "error: --port must be 1..65535\n");
    std::exit(2);
  }
  o.transport.port = static_cast<std::uint16_t>(port);
  o.transport.use_tls = args.Flag("tls");
  o.transport.username = args.Str("username");
  o.transport.password = args.Str("password");
  o.transport.ca_file = args.Str("ca-file");
  o.transport.verify_server = !args.Flag("insecure");
  o.openapi_key = args.Str("key");
  const double timeout = args.Double("timeout");
  if (timeout <= 0) {
    std::fprintf(stderr, "error: --timeout must be greater than 0\n");
    std::exit(2);
  }
  o.request_timeout = std::chrono::milliseconds(static_cast<long long>(timeout * 1000));
  o.transport.connect_timeout = o.request_timeout;
  return o;
}

bool ConnectOrReport(Client* client, const Args& args) {
  Result r = client->Connect();
  if (r.ok()) return true;
  std::fprintf(stderr, "error: cannot connect to %s:%d: %s: %s\n", args.Str("host").c_str(),
               args.Int("port"), lixel::openapi::ErrcName(r.code), r.desc.c_str());
  if (args.Flag("tls") && !args.Flag("insecure") && args.Str("ca-file").empty()) {
    std::fprintf(
        stderr,
        "hint: the device certificate is self-signed, so it is refused by default. Pass\n"
        "      --ca-file <pem> to trust that certificate, or --insecure to skip the check\n"
        "      (the connection is then encrypted but anybody on the network can pose as\n"
        "      the device and read the OpenAPI Key).\n");
  }
  return false;
}

void Dump(const std::string& title, const google::protobuf::Message& message) {
  std::string json;
  Result r = lixel::openapi::MessageToJson(message, &json);
  std::printf("\n===== %s =====\n", title.c_str());
  if (r.ok()) {
    std::printf("%s\n", json.c_str());
  } else {
    std::printf("(cannot show as JSON: %s)\n", r.desc.c_str());
  }
}

void DumpText(const std::string& title, const std::string& text) {
  std::printf("\n===== %s =====\n%s\n", title.c_str(), text.c_str());
}

int RunRequest(const Args& args, const std::string& request_topic,
               const google::protobuf::Message& request, const std::string& response_topic,
               google::protobuf::Message* response, const std::string& title) {
  Client client(ClientOptionsFrom(args));
  if (!ConnectOrReport(&client, args)) return 1;
  Result r = client.Request(request_topic, request, response_topic, response);
  if (r.ok() || r.code == Errc::DeviceError) Dump(title, *response);
  if (r.ok()) return 0;
  if (r.code == Errc::DeviceError) {
    std::fprintf(stderr, "error: the device answered with error code %s: %s\n",
                 Hex(r.device_code).c_str(), r.desc.c_str());
    return 3;
  }
  std::fprintf(stderr, "error: %s: %s\n", lixel::openapi::ErrcName(r.code), r.desc.c_str());
  return 1;
}

int RunSubscribe(const Args& args, const std::string& topic, const std::string& message_name,
                 const std::string& title, const std::string& banner) {
  InstallInterruptHandler();
  Client client(ClientOptionsFrom(args));
  if (!ConnectOrReport(&client, args)) return 1;
  Result r;
  if (message_name.empty()) {
    r = client.Subscribe(topic, [title](const std::string&, const std::string& payload) {
      DumpText(title, payload);
    });
  } else {
    r = client.SubscribeMessage(
        topic, message_name,
        [title](const std::string&, const google::protobuf::Message& m) { Dump(title, m); });
  }
  if (!r.ok()) {
    std::fprintf(stderr, "error: cannot subscribe to %s: %s: %s\n", topic.c_str(),
                 lixel::openapi::ErrcName(r.code), r.desc.c_str());
    return 1;
  }
  std::printf("%s\n", banner.c_str());
  if (!SleepUnlessInterrupted(args.Double("seconds"))) return 130;
  return 0;
}

// ---------------------------------------------------------------------------
// Ctrl-C
// ---------------------------------------------------------------------------

void InstallInterruptHandler() {
  std::signal(SIGINT, OnSignal);
  std::signal(SIGTERM, OnSignal);
}

int InterruptCount() { return static_cast<int>(g_interrupts); }

bool SleepUnlessInterrupted(double seconds) {
  const int base = InterruptCount();
  const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(static_cast<long long>(seconds * 1000));
  while (std::chrono::steady_clock::now() < deadline) {
    if (InterruptCount() > base) return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  return InterruptCount() == base;
}

}  // namespace lixel_examples
