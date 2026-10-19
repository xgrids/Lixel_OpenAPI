// The scan flow against a scripted device on a fake transport: no broker, no real device.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "fake_transport.h"
#include "lixel/openapi/client.h"
#include "lixel/openapi/codes.h"
#include "lixel/openapi/scan.h"
#include "openapi/aiot_msgs.pb.h"

using namespace lixel::openapi;  // NOLINT
using testing_support::FakeTransport;
namespace pbm = ::openapi::aiot_msgs;
using std::chrono::milliseconds;
using Clock = std::chrono::steady_clock;

namespace {

const char kStatusTopic[] = "lixel/application/report/device_status";
const char kRequestTopic[] = "lixel/application/request/modeling";
const char kResponseTopic[] = "lixel/application/response/modeling";
const std::uint32_t kSomeDeviceError = 0x32040002u;

// A device that reports its state and answers AC_START / AC_STOP the way the test says.
class ScriptedDevice {
 public:
  enum class Answer { Accept, Reject, Silent };

  ScriptedDevice(FakeTransport* fake, std::uint32_t initial_state, milliseconds report_every)
      : fake_(fake), state_(initial_state), report_every_(report_every) {
    fake_->SetPublishHook([this](FakeTransport&, const std::string& topic, const std::string& p) {
      if (topic == kRequestTopic) OnRequest(p);
    });
    thread_ = std::thread([this] { Loop(); });
  }
  ~ScriptedDevice() {
    stop_ = true;
    thread_.join();
  }

  // ---- script ----
  Answer start_answer = Answer::Accept;
  Answer stop_answer = Answer::Accept;
  milliseconds starting_to_scanning{40};
  bool reach_scanning = true;          // after an accepted AC_START
  milliseconds stopping_to_ready{60};  // after an accepted (or rejected) AC_STOP
  bool ready_after_stop = true;
  std::function<void()> on_start_request;  // runs inside the publish of AC_START

  // ---- observations ----
  std::atomic<int> starts{0}, stops{0};
  std::mutex mu;
  pbm::ModelingRequest last_start;

  void SetState(std::uint32_t s) {
    std::lock_guard<std::mutex> lock(state_mu_);
    state_ = s;
  }
  void PublishStatusNow() {
    pbm::DeviceStatusReport rep;
    rep.set_modeling_state(StateCopy());
    fake_->Inject(kStatusTopic, rep.SerializeAsString());
  }
  void At(milliseconds delay, std::uint32_t s) {
    std::lock_guard<std::mutex> lock(state_mu_);
    events_.push_back({Clock::now() + delay, s});
  }

 private:
  std::uint32_t StateCopy() {
    std::lock_guard<std::mutex> lock(state_mu_);
    return state_;
  }
  void Respond(const pbm::ModelingRequest& req, std::uint32_t code) {
    pbm::ModelingResponse rsp;
    rsp.mutable_header()->CopyFrom(req.header());
    rsp.set_action(req.action());
    rsp.mutable_error()->set_code(code);
    rsp.mutable_error()->set_desc(code == kSuccess ? "success" : "refused");
    fake_->Inject(kResponseTopic, rsp.SerializeAsString());
  }
  void OnRequest(const std::string& payload) {
    pbm::ModelingRequest req;
    if (!req.ParseFromString(payload)) return;
    if (req.action() == pbm::Application::AC_START) {
      ++starts;
      {
        std::lock_guard<std::mutex> lock(mu);
        last_start = req;
      }
      if (on_start_request) on_start_request();
      if (start_answer == Answer::Silent) return;
      if (start_answer == Answer::Reject) return Respond(req, kSomeDeviceError);
      SetState(kScanStarting);
      if (reach_scanning) At(starting_to_scanning, kScanModeling);
      Respond(req, kSuccess);
    } else if (req.action() == pbm::Application::AC_STOP) {
      ++stops;
      if (stop_answer == Answer::Silent) return;
      if (stop_answer == Answer::Reject) {
        if (ready_after_stop) At(stopping_to_ready, kScanReady);
        return Respond(req, kSomeDeviceError);
      }
      SetState(kScanStopping);
      if (ready_after_stop) At(stopping_to_ready, kScanReady);
      Respond(req, kSuccess);
    }
  }
  void Loop() {
    auto next_report = Clock::now();
    while (!stop_) {
      {
        std::lock_guard<std::mutex> lock(state_mu_);
        for (auto it = events_.begin(); it != events_.end();) {
          if (it->first <= Clock::now()) {
            state_ = it->second;
            it = events_.erase(it);
          } else {
            ++it;
          }
        }
      }
      if (report_every_.count() > 0 && Clock::now() >= next_report) {
        PublishStatusNow();
        next_report = Clock::now() + report_every_;
      }
      std::this_thread::sleep_for(milliseconds(5));
    }
  }

  FakeTransport* fake_;
  std::mutex state_mu_;
  std::uint32_t state_;
  std::vector<std::pair<Clock::time_point, std::uint32_t>> events_;
  milliseconds report_every_;
  std::atomic<bool> stop_{false};
  std::thread thread_;
};

struct Rig {
  FakeTransport* fake;
  std::unique_ptr<Client> client;
  std::unique_ptr<ScriptedDevice> device;
  std::atomic<int> interrupts{0};
  std::vector<std::string> lines;
  std::mutex lines_mu;

  explicit Rig(std::uint32_t initial_state = kScanReady,
               milliseconds report_every = milliseconds(20), bool connect = true) {
    auto t = std::unique_ptr<FakeTransport>(new FakeTransport);
    fake = t.get();
    ClientOptions o;
    o.request_timeout = milliseconds(300);
    client.reset(new Client(o, std::move(t)));
    if (connect) {
      EXPECT_TRUE(client->Connect().ok());
    }
    device.reset(new ScriptedDevice(fake, initial_state, report_every));
  }
  ~Rig() {
    device.reset();  // before the client, so nothing injects into a dead transport
    client.reset();
  }

  ScanOptions Options() {
    ScanOptions o;
    o.project_name = "demo";
    o.ready_timeout = milliseconds(500);
    o.start_timeout = milliseconds(800);
    o.watch = milliseconds(100);
    o.stop_timeout = milliseconds(800);
    o.interrupt_count = [this] { return interrupts.load(); };
    o.on_progress = [this](const std::string& l) {
      std::lock_guard<std::mutex> lock(lines_mu);
      lines.push_back(l);
    };
    return o;
  }
  bool Said(const std::string& part) {
    std::lock_guard<std::mutex> lock(lines_mu);
    for (const auto& l : lines) {
      if (l.find(part) != std::string::npos) return true;
    }
    return false;
  }
};

bool Contains(const std::string& s, const std::string& part) {
  return s.find(part) != std::string::npos;
}

}  // namespace

TEST(Scan, HappyPathStartsWatchesStopsAndWaitsForStandby) {
  Rig rig;
  const auto t0 = Clock::now();
  ScanReport rep = RunScan(*rig.client, rig.Options());
  EXPECT_EQ(rep.outcome, ScanOutcome::Completed) << rep.message;
  EXPECT_EQ(rep.exit_code(), 0);
  EXPECT_TRUE(rep.message.empty());
  EXPECT_TRUE(rep.start_sent);
  EXPECT_TRUE(rep.stop_sent);
  EXPECT_TRUE(rep.scanning_seen);
  EXPECT_EQ(rep.last_state, kScanReady);
  EXPECT_EQ(rep.start_result.device_code, kSuccess);
  EXPECT_EQ(rep.stop_result.device_code, kSuccess);
  EXPECT_EQ(rep.start_response.error().desc(), "success");
  EXPECT_LT(Clock::now() - t0, std::chrono::seconds(5));
  EXPECT_EQ(rig.device->starts.load(), 1);
  EXPECT_EQ(rig.device->stops.load(), 1);
  {
    std::lock_guard<std::mutex> lock(rig.device->mu);
    const pbm::ModelingRequest& s = rig.device->last_start;
    EXPECT_EQ(s.action(), pbm::Application::AC_START);
    EXPECT_EQ(s.project_name(), "demo");
    EXPECT_EQ(s.record_mode(), pbm::Modeling::RM_RECORD_CALC);
    EXPECT_EQ(s.scan_mode(), pbm::Modeling::SM_PCL);
    EXPECT_EQ(s.mount_type(), pbm::Modeling::MT_HAND);
  }
  EXPECT_TRUE(rig.Said("state: scanning"));
  EXPECT_TRUE(rig.Said("state: standby"));
}

TEST(Scan, TheRequestedModesAreSent) {
  Rig rig;
  ScanOptions o = rig.Options();
  o.record_mode = pbm::Modeling::RM_RECORD;
  o.scan_mode = pbm::Modeling::SM_LCC;
  o.mount_type = pbm::Modeling::MT_BACKPACK;
  ASSERT_EQ(RunScan(*rig.client, o).outcome, ScanOutcome::Completed);
  std::lock_guard<std::mutex> lock(rig.device->mu);
  EXPECT_EQ(rig.device->last_start.record_mode(), pbm::Modeling::RM_RECORD);
  EXPECT_EQ(rig.device->last_start.scan_mode(), pbm::Modeling::SM_LCC);
  EXPECT_EQ(rig.device->last_start.mount_type(), pbm::Modeling::MT_BACKPACK);
}

// ---- exit code 2: the device is not in standby ----

TEST(Scan, ADeviceThatIsAlreadyScanningIsNotTouched) {
  Rig rig(kScanModeling);
  ScanOptions o = rig.Options();
  o.ready_timeout = milliseconds(200);
  ScanReport rep = RunScan(*rig.client, o);
  EXPECT_EQ(rep.outcome, ScanOutcome::NotStandby);
  EXPECT_EQ(rep.exit_code(), 2);
  EXPECT_TRUE(Contains(rep.message, "scanning")) << rep.message;
  EXPECT_FALSE(rep.start_sent);
  EXPECT_EQ(rig.device->starts.load(), 0);
  EXPECT_EQ(rig.device->stops.load(), 0);
}

TEST(Scan, ADeviceThatReportsNothingIsNotInStandby) {
  Rig rig(kScanReady, milliseconds(0));  // never publishes its state
  ScanOptions o = rig.Options();
  o.ready_timeout = milliseconds(200);
  ScanReport rep = RunScan(*rig.client, o);
  EXPECT_EQ(rep.outcome, ScanOutcome::NotStandby);
  EXPECT_TRUE(Contains(rep.message, "no status report")) << rep.message;
  EXPECT_EQ(rig.device->starts.load(), 0);
}

TEST(Scan, UsbMassStorageModeIsNamedInTheMessage) {
  Rig rig(kScanNotAllowedInUsbMass);
  ScanOptions o = rig.Options();
  o.ready_timeout = milliseconds(200);
  ScanReport rep = RunScan(*rig.client, o);
  EXPECT_EQ(rep.outcome, ScanOutcome::NotStandby);
  EXPECT_TRUE(Contains(rep.message, "USB")) << rep.message;
}

// ---- exit code 3: the device refuses AC_START ----

TEST(Scan, ARejectedStartEndsWithoutStop) {
  Rig rig;
  rig.device->start_answer = ScriptedDevice::Answer::Reject;
  ScanReport rep = RunScan(*rig.client, rig.Options());
  EXPECT_EQ(rep.outcome, ScanOutcome::StartRejected);
  EXPECT_EQ(rep.exit_code(), 3);
  EXPECT_EQ(rep.start_result.device_code, kSomeDeviceError);
  EXPECT_TRUE(Contains(rep.message, "0x32040002")) << rep.message;
  EXPECT_EQ(rig.device->stops.load(), 0) << "nothing was started, so nothing is stopped";
}

// ---- exit code 4: no answer ----

TEST(Scan, NoAnswerToStartMeansNoStop) {
  Rig rig;
  rig.device->start_answer = ScriptedDevice::Answer::Silent;
  ScanReport rep = RunScan(*rig.client, rig.Options());
  EXPECT_EQ(rep.outcome, ScanOutcome::NoResponse);
  EXPECT_EQ(rep.exit_code(), 4);
  EXPECT_TRUE(rep.start_sent);
  EXPECT_FALSE(rep.stop_sent);
  EXPECT_TRUE(Contains(rep.message, "AC_START")) << rep.message;
  EXPECT_EQ(rig.device->stops.load(), 0);
}

TEST(Scan, NoAnswerToStopSaysTheDeviceMayStillBeScanning) {
  Rig rig;
  rig.device->stop_answer = ScriptedDevice::Answer::Silent;
  ScanReport rep = RunScan(*rig.client, rig.Options());
  EXPECT_EQ(rep.outcome, ScanOutcome::NoResponse);
  EXPECT_EQ(rep.exit_code(), 4);
  EXPECT_TRUE(rep.stop_sent);
  EXPECT_TRUE(Contains(rep.message, "may still be scanning")) << rep.message;
  EXPECT_EQ(rig.device->stops.load(), 1);
}

// ---- exit code 1: timeouts, and the device is still stopped ----

TEST(Scan, ADeviceThatNeverStartsScanningIsStoppedAndReportsTimeout) {
  Rig rig;
  rig.device->reach_scanning = false;  // stays in "starting"
  ScanOptions o = rig.Options();
  o.start_timeout = milliseconds(200);
  ScanReport rep = RunScan(*rig.client, o);
  EXPECT_EQ(rep.outcome, ScanOutcome::Timeout);
  EXPECT_EQ(rep.exit_code(), 1);
  EXPECT_FALSE(rep.scanning_seen);
  EXPECT_TRUE(Contains(rep.message, "did not start scanning")) << rep.message;
  EXPECT_EQ(rig.device->stops.load(), 1) << "AC_STOP follows a start timeout";
}

TEST(Scan, ADeviceThatDoesNotComeBackToStandbyIsAnError) {
  Rig rig;
  rig.device->ready_after_stop = false;  // stays in "stopping"
  ScanOptions o = rig.Options();
  o.stop_timeout = milliseconds(250);
  ScanReport rep = RunScan(*rig.client, o);
  EXPECT_EQ(rep.outcome, ScanOutcome::Timeout);
  EXPECT_TRUE(rep.scanning_seen);
  EXPECT_TRUE(Contains(rep.message, "did not return to standby")) << rep.message;
  EXPECT_TRUE(Contains(rep.message, "stopping")) << rep.message;
}

TEST(Scan, AStopTheDeviceRefusesStillWaitsForStandby) {
  Rig rig;
  rig.device->stop_answer = ScriptedDevice::Answer::Reject;
  ScanReport rep = RunScan(*rig.client, rig.Options());
  EXPECT_EQ(rep.outcome, ScanOutcome::Completed) << rep.message;
  EXPECT_EQ(rep.stop_result.code, Errc::DeviceError);
  EXPECT_EQ(rep.stop_result.device_code, kSomeDeviceError);
  EXPECT_TRUE(rig.Said("AC_STOP answered with error code"));
}

// A standby report that was cached before AC_STOP was answered must not end the wait.
TEST(Scan, TheStandbyAfterStopMustBeANewReport) {
  Rig rig(kScanReady, milliseconds(0));  // reports only when the test says so
  rig.device->reach_scanning = false;
  rig.device->ready_after_stop = false;
  ScanOptions o = rig.Options();
  o.ready_timeout = milliseconds(3000);
  o.start_timeout = milliseconds(150);
  o.stop_timeout = milliseconds(3000);
  std::atomic<bool> returned{false};
  ScanReport rep;
  std::thread runner([&] {
    rep = RunScan(*rig.client, o);
    returned = true;
  });
  // If an assertion below fails, the thread must still be joined: the device ends the flow.
  struct Joiner {
    std::thread& t;
    Rig& rig;
    ~Joiner() {
      if (t.joinable()) {
        rig.device->SetState(kScanReady);
        rig.device->PublishStatusNow();
        t.join();
      }
    }
  } joiner{runner, rig};
  const auto limit = Clock::now() + std::chrono::seconds(3);
  // The status message is lost if it comes before the flow has subscribed.
  while (!rig.fake->IsSubscribed(kStatusTopic) && Clock::now() < limit) {
    std::this_thread::sleep_for(milliseconds(5));
  }
  ASSERT_TRUE(rig.fake->IsSubscribed(kStatusTopic));
  rig.device->PublishStatusNow();  // standby, cached by the flow
  // Wait until AC_STOP has been sent and answered.
  while (rig.device->stops.load() == 0 && Clock::now() < limit) {
    std::this_thread::sleep_for(milliseconds(5));
  }
  ASSERT_EQ(rig.device->stops.load(), 1);
  std::this_thread::sleep_for(milliseconds(300));
  EXPECT_FALSE(returned.load()) << "the cached standby from before the stop must be ignored";
  rig.device->SetState(kScanReady);
  rig.device->PublishStatusNow();
  runner.join();
  EXPECT_TRUE(returned.load());
  EXPECT_EQ(rep.outcome, ScanOutcome::Timeout) << "it never scanned";
  EXPECT_EQ(rep.last_state, kScanReady);
}

// ---- exit code 130: the caller asks to stop ----

TEST(Scan, AnInterruptWhileScanningStopsTheScanAndReturns130) {
  Rig rig;
  ScanOptions o = rig.Options();
  o.watch = std::chrono::seconds(30);
  std::thread asker([&] {
    // as soon as the device scans
    const auto limit = Clock::now() + std::chrono::seconds(3);
    while (!rig.Said("state: scanning") && Clock::now() < limit) {
      std::this_thread::sleep_for(milliseconds(5));
    }
    ++rig.interrupts;
  });
  const auto t0 = Clock::now();
  ScanReport rep = RunScan(*rig.client, o);
  asker.join();
  EXPECT_EQ(rep.outcome, ScanOutcome::Interrupted);
  EXPECT_EQ(rep.exit_code(), 130);
  EXPECT_TRUE(rep.stop_sent);
  EXPECT_TRUE(Contains(rep.message, "stopped")) << rep.message;
  EXPECT_LT(Clock::now() - t0, std::chrono::seconds(5)) << "must not sit out the 30 s watch";
  EXPECT_EQ(rig.device->stops.load(), 1);
}

TEST(Scan, AnInterruptWhileWaitingForScanningStillStops) {
  Rig rig;
  rig.device->reach_scanning = false;
  ScanOptions o = rig.Options();
  o.start_timeout = std::chrono::seconds(30);
  std::thread asker([&] {
    const auto limit = Clock::now() + std::chrono::seconds(3);
    while (rig.device->starts.load() == 0 && Clock::now() < limit) {
      std::this_thread::sleep_for(milliseconds(5));
    }
    std::this_thread::sleep_for(milliseconds(80));
    ++rig.interrupts;
  });
  const auto t0 = Clock::now();
  ScanReport rep = RunScan(*rig.client, o);
  asker.join();
  EXPECT_EQ(rep.outcome, ScanOutcome::Interrupted);
  EXPECT_EQ(rig.device->stops.load(), 1) << "the start sequence may be running on the device";
  EXPECT_LT(Clock::now() - t0, std::chrono::seconds(5));
}

TEST(Scan, AnInterruptWhileAcStartIsInFlightGoesStraightToStop) {
  Rig rig;
  rig.device->on_start_request = [&] { ++rig.interrupts; };
  ScanOptions o = rig.Options();
  o.start_timeout = std::chrono::seconds(30);  // would be a long wait if the interrupt were missed
  const auto t0 = Clock::now();
  ScanReport rep = RunScan(*rig.client, o);
  EXPECT_EQ(rep.outcome, ScanOutcome::Interrupted);
  EXPECT_TRUE(rep.start_sent);
  EXPECT_TRUE(rep.stop_sent);
  EXPECT_EQ(rig.device->stops.load(), 1);
  EXPECT_LT(Clock::now() - t0, std::chrono::seconds(5));
}

TEST(Scan, AnInterruptBeforeAnythingWasSentSendsNothing) {
  Rig rig(kScanReady, milliseconds(0));  // no status, so the flow is waiting for standby
  ScanOptions o = rig.Options();
  o.ready_timeout = std::chrono::seconds(30);
  std::thread asker([&] {
    std::this_thread::sleep_for(milliseconds(100));
    ++rig.interrupts;
  });
  const auto t0 = Clock::now();
  ScanReport rep = RunScan(*rig.client, o);
  asker.join();
  EXPECT_EQ(rep.outcome, ScanOutcome::Interrupted);
  EXPECT_FALSE(rep.start_sent);
  EXPECT_EQ(rig.device->starts.load(), 0);
  EXPECT_EQ(rig.device->stops.load(), 0);
  EXPECT_LT(Clock::now() - t0, std::chrono::seconds(5));
}

TEST(Scan, ASecondInterruptGivesUpWaitingForStandby) {
  Rig rig;
  rig.device->ready_after_stop = false;  // never comes back
  ScanOptions o = rig.Options();
  o.watch = std::chrono::seconds(30);
  o.stop_timeout = std::chrono::seconds(30);
  std::thread asker([&] {
    while (!rig.Said("state: scanning")) std::this_thread::sleep_for(milliseconds(5));
    ++rig.interrupts;  // first: stop the scan
    while (rig.device->stops.load() == 0) std::this_thread::sleep_for(milliseconds(5));
    std::this_thread::sleep_for(milliseconds(150));
    ++rig.interrupts;  // second: do not wait any longer
  });
  const auto t0 = Clock::now();
  ScanReport rep = RunScan(*rig.client, o);
  asker.join();
  EXPECT_EQ(rep.outcome, ScanOutcome::Interrupted);
  EXPECT_TRUE(rep.stop_sent);
  EXPECT_TRUE(Contains(rep.message, "AC_STOP has been sent")) << rep.message;
  EXPECT_LT(Clock::now() - t0, std::chrono::seconds(5));
}

TEST(Scan, InterruptsThatHappenedBeforeTheCallAreNotCounted) {
  Rig rig;
  rig.interrupts = 3;  // a counter that was already used
  ScanReport rep = RunScan(*rig.client, rig.Options());
  EXPECT_EQ(rep.outcome, ScanOutcome::Completed) << rep.message;
}

// ---- bad input and housekeeping ----

TEST(Scan, WithoutConnectionItFailsAtOnceAndSendsNothing) {
  Rig rig(kScanReady, milliseconds(20), /*connect=*/false);
  ScanReport rep = RunScan(*rig.client, rig.Options());
  EXPECT_EQ(rep.outcome, ScanOutcome::Error);
  EXPECT_EQ(rep.exit_code(), 5);
  EXPECT_FALSE(rep.start_sent);
  EXPECT_TRUE(Contains(rep.message, "NotConnected")) << rep.message;
}

TEST(Scan, AnEmptyProjectNameIsRefused) {
  Rig rig;
  ScanOptions o = rig.Options();
  o.project_name.clear();
  EXPECT_EQ(RunScan(*rig.client, o).outcome, ScanOutcome::Error);
  EXPECT_EQ(rig.device->starts.load(), 0);
}

TEST(Scan, ItRemovesOnlyItsOwnStatusSubscription) {
  Rig rig;
  std::atomic<int> mine{0};
  ASSERT_TRUE(
      rig.client->Subscribe(kStatusTopic, [&](const std::string&, const std::string&) { ++mine; })
          .ok());
  ASSERT_EQ(RunScan(*rig.client, rig.Options()).outcome, ScanOutcome::Completed);
  EXPECT_TRUE(rig.fake->IsSubscribed(kStatusTopic)) << "the caller's own handler still needs it";
  const int before = mine.load();
  rig.device->PublishStatusNow();
  std::this_thread::sleep_for(milliseconds(100));
  EXPECT_GT(mine.load(), before);
}

TEST(Scan, NothingIsCalledAfterItReturned) {
  Rig rig;
  std::atomic<int> calls{0};
  {
    ScanOptions o = rig.Options();
    o.on_progress = [&](const std::string&) { ++calls; };
    ASSERT_EQ(RunScan(*rig.client, o).outcome, ScanOutcome::Completed);
  }
  EXPECT_FALSE(rig.fake->IsSubscribed(kStatusTopic)) << "no other handler: the broker is told";
  const int at_return = calls.load();
  rig.device->SetState(kScanModeling);  // would be a state change
  rig.device->PublishStatusNow();
  std::this_thread::sleep_for(milliseconds(150));
  EXPECT_EQ(calls.load(), at_return);
}

TEST(Scan, ItCanRunTwiceOnTheSameClient) {
  Rig rig;
  EXPECT_EQ(RunScan(*rig.client, rig.Options()).outcome, ScanOutcome::Completed);
  EXPECT_EQ(RunScan(*rig.client, rig.Options()).outcome, ScanOutcome::Completed);
  EXPECT_EQ(rig.device->starts.load(), 2);
  EXPECT_EQ(rig.device->stops.load(), 2);
}

TEST(Scan, OutcomeNamesAndExitCodes) {
  EXPECT_STREQ(ScanOutcomeName(ScanOutcome::Completed), "Completed");
  EXPECT_STREQ(ScanOutcomeName(ScanOutcome::Interrupted), "Interrupted");
  ScanReport r;
  r.outcome = ScanOutcome::Timeout;
  EXPECT_EQ(r.exit_code(), 1);
  r.outcome = ScanOutcome::NotStandby;
  EXPECT_EQ(r.exit_code(), 2);
  r.outcome = ScanOutcome::StartRejected;
  EXPECT_EQ(r.exit_code(), 3);
  r.outcome = ScanOutcome::NoResponse;
  EXPECT_EQ(r.exit_code(), 4);
}
