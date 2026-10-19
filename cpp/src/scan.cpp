#include "lixel/openapi/scan.h"

#include <algorithm>
#include <condition_variable>
#include <cstdio>
#include <memory>
#include <mutex>
#include <thread>

#include "lixel/openapi/codes.h"

namespace lixel {
namespace openapi {

namespace pbm = ::openapi::aiot_msgs;
using std::chrono::milliseconds;
using Clock = std::chrono::steady_clock;

const char* ScanOutcomeName(ScanOutcome outcome) noexcept {
  switch (outcome) {
    case ScanOutcome::Completed:
      return "Completed";
    case ScanOutcome::Timeout:
      return "Timeout";
    case ScanOutcome::NotStandby:
      return "NotStandby";
    case ScanOutcome::StartRejected:
      return "StartRejected";
    case ScanOutcome::NoResponse:
      return "NoResponse";
    case ScanOutcome::Error:
      return "Error";
    case ScanOutcome::Interrupted:
      return "Interrupted";
  }
  return "Unknown";
}

namespace {

const char kStatusTopic[] = "lixel/application/report/device_status";
const char kRequestTopic[] = "lixel/application/request/modeling";
const char kResponseTopic[] = "lixel/application/response/modeling";
const milliseconds kPoll(50);

std::string Hex(std::uint32_t v) {
  char buf[16];
  std::snprintf(buf, sizeof(buf), "0x%08X", static_cast<unsigned>(v));
  return buf;
}

std::string Seconds(milliseconds ms) {
  return std::to_string(std::chrono::duration_cast<std::chrono::seconds>(ms).count()) + " s";
}

// The latest DeviceStatusReport.modeling_state.
struct Watcher {
  std::mutex mu;
  std::condition_variable cv;
  bool has_state = false;
  std::uint32_t state = 0;
  std::uint64_t reports = 0;  // number of reports received

  std::uint32_t State() {
    std::lock_guard<std::mutex> lock(mu);
    return has_state ? state : 0;
  }
};

// Progress lines go to the caller's function, one at a time, and never after RunScan returned.
struct Sink {
  std::mutex mu;
  std::function<void(const std::string&)> fn;
  void Say(const std::string& line) {
    std::lock_guard<std::mutex> lock(mu);
    if (fn) fn(line);
  }
  void Close() {
    std::lock_guard<std::mutex> lock(mu);
    fn = nullptr;
  }
};

enum class Wait { Reached, TimedOut, Interrupted };

class Runner {
 public:
  Runner(Client& client, const ScanOptions& options)
      : client_(client),
        o_(options),
        watcher_(std::make_shared<Watcher>()),
        sink_(std::make_shared<Sink>()) {
    sink_->fn = o_.on_progress;
    base_ = Interrupts();
  }
  ~Runner() {
    if (sub_ != 0) client_.RemoveSubscription(sub_);
    sink_->Close();  // waits for a progress call that is running on the handler thread
  }

  ScanReport Run();

 private:
  int Interrupts() const { return o_.interrupt_count ? o_.interrupt_count() : 0; }
  bool InterruptedSince(int base) const { return Interrupts() > base; }
  void Say(const std::string& line) { sink_->Say(line); }

  // Waits until modeling_state == target. With `fresh`, a state received before this call does
  // not count: a report that arrives after the call is required.
  Wait WaitForState(std::uint32_t target, milliseconds timeout, bool fresh, int interrupt_base) {
    const auto deadline = Clock::now() + timeout;
    std::unique_lock<std::mutex> lock(watcher_->mu);
    const std::uint64_t mark = watcher_->reports;
    for (;;) {
      if (watcher_->has_state && watcher_->state == target && !(fresh && watcher_->reports == mark))
        return Wait::Reached;
      if (InterruptedSince(interrupt_base)) return Wait::Interrupted;
      const auto now = Clock::now();
      if (now >= deadline) return Wait::TimedOut;
      watcher_->cv.wait_until(lock, std::min(deadline, now + kPoll));
    }
  }

  // Sleeps `duration`, ending early when the caller asks to stop. Returns false then.
  bool Hold(milliseconds duration) {
    const auto deadline = Clock::now() + duration;
    while (Clock::now() < deadline) {
      if (InterruptedSince(base_)) return false;
      std::this_thread::sleep_for(std::min<milliseconds>(
          kPoll, std::chrono::duration_cast<milliseconds>(deadline - Clock::now())));
    }
    return !InterruptedSince(base_);
  }

  std::string StateText() {
    std::lock_guard<std::mutex> lock(watcher_->mu);
    if (!watcher_->has_state) return "no status report received";
    return std::string(StateName(watcher_->state));
  }

  ScanReport Finish(ScanReport rep, ScanOutcome outcome, std::string message) {
    rep.outcome = outcome;
    rep.message = std::move(message);
    rep.last_state = watcher_->State();
    return rep;
  }

  Client& client_;
  const ScanOptions& o_;
  std::shared_ptr<Watcher> watcher_;
  std::shared_ptr<Sink> sink_;
  SubscriptionId sub_ = 0;
  int base_ = 0;
};

ScanReport Runner::Run() {
  ScanReport rep;
  if (o_.project_name.empty()) {
    return Finish(rep, ScanOutcome::Error, "project_name is empty");
  }

  // Watch the device status from now on; the first reports arrive within a second.
  std::shared_ptr<Watcher> watcher = watcher_;
  std::shared_ptr<Sink> sink = sink_;
  Result sr = client_.SubscribeMessage(
      kStatusTopic, "DeviceStatusReport",
      [watcher, sink](const std::string&, const google::protobuf::Message& m) {
        const std::uint32_t state = static_cast<const pbm::DeviceStatusReport&>(m).modeling_state();
        bool changed = false;
        {
          std::lock_guard<std::mutex> lock(watcher->mu);
          changed = !watcher->has_state || watcher->state != state;
          watcher->has_state = true;
          watcher->state = state;
          ++watcher->reports;
        }
        watcher->cv.notify_all();
        if (changed) sink->Say(std::string("state: ") + StateName(state));
      },
      &sub_);
  if (!sr.ok()) {
    return Finish(rep, ScanOutcome::Error,
                  std::string("cannot subscribe to the device status: ") + ErrcName(sr.code) +
                      ": " + sr.desc);
  }

  // 1. The device must be in standby before a scan can be started.
  Say("waiting for the device to be in standby (up to " + Seconds(o_.ready_timeout) + ") ...");
  Wait w = WaitForState(kScanReady, o_.ready_timeout, false, base_);
  if (w == Wait::Interrupted) {
    return Finish(rep, ScanOutcome::Interrupted,
                  "interrupted before AC_START was sent; nothing was started");
  }
  if (w == Wait::TimedOut) {
    return Finish(rep, ScanOutcome::NotStandby,
                  "the device is not in standby (" + StateText() + "); nothing was sent");
  }

  // 2. Start. From here on the request may reach the device.
  pbm::ModelingRequest start;
  start.set_action(pbm::Application::AC_START);
  start.set_project_name(o_.project_name);
  start.set_record_mode(o_.record_mode);
  start.set_scan_mode(o_.scan_mode);
  start.set_mount_type(o_.mount_type);
  Say("sending AC_START for project \"" + o_.project_name + "\" ...");
  rep.start_result = client_.Request(kRequestTopic, start, kResponseTopic, &rep.start_response);
  switch (rep.start_result.code) {
    case Errc::Ok:
      rep.start_sent = true;
      break;
    case Errc::DeviceError:
      rep.start_sent = true;
      return Finish(rep, ScanOutcome::StartRejected,
                    "the device rejected AC_START (error code " +
                        Hex(rep.start_result.device_code) + ": " + rep.start_result.desc +
                        "); nothing is scanning");
    case Errc::RequestTimeout:
      rep.start_sent = true;
      return Finish(rep, ScanOutcome::NoResponse,
                    "no response to AC_START. The scan state is unknown; check the device before "
                    "retrying");
    default:
      return Finish(rep, ScanOutcome::Error,
                    std::string("AC_START failed: ") + ErrcName(rep.start_result.code) + ": " +
                        rep.start_result.desc);
  }
  Say("AC_START accepted (" + Hex(rep.start_result.device_code) + ")");

  // 3. Wait until the device is really scanning, then watch it for a while.
  bool interrupted = InterruptedSince(base_);
  bool started = false;
  if (!interrupted) {
    Say("waiting for the device to start scanning (up to " + Seconds(o_.start_timeout) + ") ...");
    w = WaitForState(kScanModeling, o_.start_timeout, false, base_);
    if (w == Wait::Reached) {
      started = true;
      rep.scanning_seen = true;
      Say("scanning; watching for " + Seconds(o_.watch));
      if (!Hold(o_.watch)) interrupted = true;
    } else if (w == Wait::Interrupted) {
      interrupted = true;
    } else {
      Say("timed out waiting for the device to start scanning; stopping");
    }
  }
  if (interrupted) Say("interrupted; stopping the scan");

  // 4. Stop. Also after an interrupt or a start timeout, so the device is not left scanning.
  const int stop_base = Interrupts();
  pbm::ModelingRequest stop;
  stop.set_action(pbm::Application::AC_STOP);
  Say("sending AC_STOP ...");
  rep.stop_result = client_.Request(kRequestTopic, stop, kResponseTopic, &rep.stop_response);
  switch (rep.stop_result.code) {
    case Errc::Ok:
      rep.stop_sent = true;
      break;
    case Errc::DeviceError:
      // The device may simply not be scanning any more, so keep waiting for standby.
      rep.stop_sent = true;
      Say("AC_STOP answered with error code " + Hex(rep.stop_result.device_code) + ": " +
          rep.stop_result.desc + "; still waiting for standby");
      break;
    case Errc::RequestTimeout:
      rep.stop_sent = true;
      return Finish(rep, interrupted ? ScanOutcome::Interrupted : ScanOutcome::NoResponse,
                    "no response to AC_STOP. The device may still be scanning; check it and stop "
                    "it manually");
    default:
      return Finish(rep, interrupted ? ScanOutcome::Interrupted : ScanOutcome::Error,
                    std::string("AC_STOP failed: ") + ErrcName(rep.stop_result.code) + ": " +
                        rep.stop_result.desc +
                        ". The device may still be scanning; check it and "
                        "stop it manually");
  }

  // 5. Wait for a standby report that arrived after AC_STOP was answered: the state cached
  // before the stop may still be the old one.
  Say("waiting for the device to return to standby (up to " + Seconds(o_.stop_timeout) + ") ...");
  w = WaitForState(kScanReady, o_.stop_timeout, true, stop_base);
  if (w == Wait::Interrupted) {
    return Finish(rep, ScanOutcome::Interrupted,
                  "interrupted while waiting for standby; AC_STOP has been sent");
  }
  const bool stopped = (w == Wait::Reached);
  if (interrupted) {
    return Finish(rep, ScanOutcome::Interrupted,
                  stopped ? "interrupted; the scan was stopped and the device is in standby"
                          : "interrupted; AC_STOP was sent but the device did not report standby "
                            "within " +
                                Seconds(o_.stop_timeout));
  }
  if (started && stopped) return Finish(rep, ScanOutcome::Completed, std::string());
  std::string why;
  if (!started) {
    why = "the device did not start scanning within " + Seconds(o_.start_timeout);
  }
  if (!stopped) {
    if (!why.empty()) why += "; ";
    why += "the device did not return to standby within " + Seconds(o_.stop_timeout) +
           " (state: " + StateText() + ")";
  }
  return Finish(rep, ScanOutcome::Timeout, why);
}

}  // namespace

ScanReport RunScan(Client& client, const ScanOptions& options) {
  Runner runner(client, options);
  return runner.Run();
}

}  // namespace openapi
}  // namespace lixel
