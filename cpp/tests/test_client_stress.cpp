// Stress for the parts of Client where a data race or a lifetime error would hide: many threads
// requesting while the connection drops and while the client is torn down. The checks are that
// nothing crashes, nothing hangs, and every request that reports success got a response that
// belongs to it. Run it under AddressSanitizer when you change src/client.cpp.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <random>
#include <thread>
#include <vector>

#include "fake_transport.h"
#include "lixel/openapi/client.h"
#include "lixel/openapi/codes.h"
#include "openapi/aiot_msgs.pb.h"

using namespace lixel::openapi;  // NOLINT
using testing_support::FakeTransport;
namespace pbm = ::openapi::aiot_msgs;
using std::chrono::milliseconds;

namespace {
const char kReq[] = "lixel/application/request/device_info";
const char kRsp[] = "lixel/application/response/device_info";

// Answers every request with its own seq in desc, after a short pseudo-random delay.
FakeTransport::PublishHook EchoSeq() {
  auto rng = std::make_shared<std::mt19937>(12345);
  auto mu = std::make_shared<std::mutex>();
  return [rng, mu](FakeTransport& t, const std::string&, const std::string& payload) {
    pbm::DeviceInfoRequest req;
    if (!req.ParseFromString(payload)) return;
    pbm::DeviceInfoResponse rsp;
    rsp.mutable_header()->CopyFrom(req.header());
    rsp.mutable_error()->set_code(kSuccess);
    rsp.mutable_error()->set_desc(std::to_string(req.header().seq()));
    int delay;
    {
      std::lock_guard<std::mutex> lock(*mu);
      delay = static_cast<int>((*rng)() % 8);
    }
    t.Inject(kRsp, rsp.SerializeAsString(), milliseconds(delay));
  };
}
}  // namespace

TEST(Stress, ManyThreadsRequestingWhileTheConnectionDropsNeverGetSomeoneElsesAnswer) {
  auto t = std::unique_ptr<FakeTransport>(new FakeTransport);
  FakeTransport* fake = t.get();
  fake->SetPublishHook(EchoSeq());
  ClientOptions o;
  o.request_timeout = milliseconds(400);
  Client client(o, std::move(t));
  ASSERT_TRUE(client.Connect().ok());

  std::atomic<bool> stop{false};
  std::atomic<int> ok{0}, timeouts{0}, other{0}, wrong{0};
  std::vector<std::thread> workers;
  for (int i = 0; i < 6; ++i) {
    workers.emplace_back([&] {
      while (!stop) {
        pbm::DeviceInfoRequest req;
        pbm::DeviceInfoResponse rsp;
        Result r = client.Request(kReq, req, kRsp, &rsp);
        if (r.ok()) {
          // The answer carries the seq of the request it answers; it must be one we sent.
          ++ok;
          if (rsp.error().desc() != std::to_string(rsp.header().seq())) ++wrong;
        } else if (r.code == Errc::RequestTimeout) {
          ++timeouts;
        } else {
          ++other;
        }
      }
    });
  }
  std::thread chaos([&] {
    while (!stop) {
      fake->SimulateReconnect();
      std::this_thread::sleep_for(milliseconds(25));
    }
  });
  std::this_thread::sleep_for(std::chrono::seconds(3));
  stop = true;
  chaos.join();
  for (auto& w : workers) w.join();

  EXPECT_GT(ok.load(), 100) << "too few requests completed: ok=" << ok << " timeouts=" << timeouts
                            << " other=" << other;
  EXPECT_EQ(wrong.load(), 0) << "a request got an answer that did not belong to it";
  EXPECT_EQ(other.load(), 0) << "unexpected error codes";
}

TEST(Stress, TearingTheClientDownWhileRequestsAreInFlight) {
  for (int round = 0; round < 25; ++round) {
    auto t = std::unique_ptr<FakeTransport>(new FakeTransport);
    FakeTransport* fake = t.get();
    fake->SetPublishHook(EchoSeq());
    auto client = std::make_shared<Client>(ClientOptions(), std::move(t));
    ASSERT_TRUE(client->Connect().ok());
    std::atomic<int> finished{0};
    std::vector<std::thread> workers;
    for (int i = 0; i < 4; ++i) {
      workers.emplace_back([&, client] {
        pbm::DeviceInfoRequest req;
        pbm::DeviceInfoResponse rsp;
        client->Request(kReq, req, kRsp, &rsp, milliseconds(300));
        ++finished;
      });
    }
    std::this_thread::sleep_for(milliseconds(3));
    client->Disconnect();  // releases the waiting threads
    for (auto& w : workers) w.join();
    EXPECT_EQ(finished.load(), 4);
    client.reset();
  }
}
