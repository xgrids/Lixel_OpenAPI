// Client logic against a fake transport: no broker, no device.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
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

const char kReqTopic[] = "lixel/application/request/device_info";
const char kRspTopic[] = "lixel/application/response/device_info";

// A device that answers every DeviceInfoRequest with the given error code, echoing the header.
FakeTransport::PublishHook Answering(std::uint32_t code, const std::string& desc = "ok",
                                     milliseconds delay = milliseconds(0)) {
  return [=](FakeTransport& t, const std::string& topic, const std::string& payload) {
    if (topic != kReqTopic) return;
    pbm::DeviceInfoRequest req;
    ASSERT_TRUE(req.ParseFromString(payload));
    pbm::DeviceInfoResponse rsp;
    rsp.mutable_header()->CopyFrom(req.header());
    rsp.mutable_error()->set_code(code);
    rsp.mutable_error()->set_desc(desc);
    t.Inject(kRspTopic, rsp.SerializeAsString(), delay);
  };
}

struct Fixture {
  FakeTransport* fake;
  std::unique_ptr<Client> client;
  explicit Fixture(ClientOptions o = ClientOptions()) {
    auto t = std::unique_ptr<FakeTransport>(new FakeTransport);
    fake = t.get();
    o.request_timeout = milliseconds(500);
    client.reset(new Client(o, std::move(t)));
  }
};

}  // namespace

// ---- TopicMatches (MQTT rules) ----

TEST(TopicMatches, FollowsTheMqttSpecificationExamples) {
  EXPECT_TRUE(TopicMatches("a/b/c", "a/b/c"));
  EXPECT_FALSE(TopicMatches("a/b/c", "a/b"));
  EXPECT_FALSE(TopicMatches("a/b", "a/b/c"));
  EXPECT_TRUE(TopicMatches("a/+/c", "a/b/c"));
  EXPECT_FALSE(TopicMatches("a/+/c", "a/b/d"));
  EXPECT_FALSE(TopicMatches("a/+", "a/b/c"));
  EXPECT_TRUE(TopicMatches("+/+", "a/b"));
  EXPECT_TRUE(TopicMatches("#", "a/b/c"));
  EXPECT_TRUE(TopicMatches("a/#", "a/b/c"));
  EXPECT_TRUE(TopicMatches("a/#", "a/b"));
  EXPECT_TRUE(TopicMatches("a/#", "a"));  // '#' also matches the parent level
  EXPECT_FALSE(TopicMatches("a/b/#", "a"));
  EXPECT_FALSE(TopicMatches("a/#", "b/c"));
  EXPECT_TRUE(TopicMatches("lixel/#", "lixel/application/report/heartbeat"));
  EXPECT_FALSE(TopicMatches("a/+", "a"));
  EXPECT_FALSE(TopicMatches("a/#/c", "a/b/c"));  // '#' must be last
  EXPECT_FALSE(TopicMatches("", "a"));
}

// ---- connect ----

TEST(Client, ConnectAndDisconnect) {
  Fixture f;
  EXPECT_FALSE(f.client->IsConnected());
  EXPECT_TRUE(f.client->Connect().ok());
  EXPECT_TRUE(f.client->IsConnected());
  EXPECT_TRUE(f.client->Connect().ok());  // already connected
  f.client->Disconnect();
  EXPECT_FALSE(f.client->IsConnected());
  f.client->Disconnect();  // safe twice
}

TEST(Client, ConnectFailureIsReportedAndTheClientCanTryAgain) {
  Fixture f;
  f.fake->refuse_connect = true;
  EXPECT_EQ(f.client->Connect().code, Errc::ConnectFailed);
  EXPECT_FALSE(f.client->IsConnected());
  f.fake->refuse_connect = false;
  EXPECT_TRUE(f.client->Connect().ok());
}

TEST(Client, PassesTransportOptionsOn) {
  ClientOptions o;
  o.transport.host = "10.1.2.3";
  o.transport.port = 8883;
  o.transport.use_tls = true;
  Fixture f(o);
  ASSERT_TRUE(f.client->Connect().ok());
  EXPECT_EQ(f.fake->options.host, "10.1.2.3");
  EXPECT_EQ(f.fake->options.port, 8883);
  EXPECT_TRUE(f.fake->options.use_tls);
  EXPECT_TRUE(f.fake->options.verify_server) << "certificate verification must be on by default";
}

// ---- request / response ----

TEST(Request, SuccessFillsTheResponseAndStampsTheHeader) {
  ClientOptions o;
  o.openapi_key = "key-1";
  o.device_id = "dev-9";
  Fixture f(o);
  f.fake->SetPublishHook(Answering(kSuccess, "success"));
  ASSERT_TRUE(f.client->Connect().ok());

  pbm::DeviceInfoRequest req;
  pbm::DeviceInfoResponse rsp;
  Result r = f.client->Request(kReqTopic, req, kRspTopic, &rsp);
  ASSERT_TRUE(r.ok()) << ErrcName(r.code) << " " << r.desc;
  EXPECT_EQ(r.device_code, kSuccess);
  EXPECT_EQ(rsp.error().desc(), "success");

  // What went to the broker carried the key, the device id and a sequence number.
  auto sent = f.fake->Published();
  ASSERT_EQ(sent.size(), 1u);
  EXPECT_EQ(sent[0].first, kReqTopic);
  pbm::DeviceInfoRequest on_wire;
  ASSERT_TRUE(on_wire.ParseFromString(sent[0].second));
  EXPECT_EQ(on_wire.header().openapi_key(), "key-1");
  EXPECT_EQ(on_wire.header().device_id(), "dev-9");
  EXPECT_NE(on_wire.header().seq(), 0u);
  // The caller's own message was not modified.
  EXPECT_FALSE(req.has_header());
  // The response topic was subscribed before the request went out.
  EXPECT_TRUE(f.fake->IsSubscribed(kRspTopic));
}

TEST(Request, AcceptsBothSuccessCodes) {
  Fixture f;
  ASSERT_TRUE(f.client->Connect().ok());
  for (std::uint32_t code : {kSuccess, kSuccessAndReboot}) {
    f.fake->SetPublishHook(Answering(code));
    pbm::DeviceInfoRequest req;
    pbm::DeviceInfoResponse rsp;
    EXPECT_TRUE(f.client->Request(kReqTopic, req, kRspTopic, &rsp).ok()) << std::hex << code;
  }
}

TEST(Request, DeviceErrorKeepsTheResponseAndCarriesTheCode) {
  Fixture f;
  f.fake->SetPublishHook(Answering(0x32040001u, "openapi auth error"));  // EC_ProtoError
  ASSERT_TRUE(f.client->Connect().ok());
  pbm::DeviceInfoRequest req;
  pbm::DeviceInfoResponse rsp;
  Result r = f.client->Request(kReqTopic, req, kRspTopic, &rsp);
  EXPECT_EQ(r.code, Errc::DeviceError);
  EXPECT_EQ(r.device_code, 0x32040001u);
  EXPECT_EQ(r.desc, "openapi auth error");
  EXPECT_EQ(rsp.error().code(), 0x32040001u);  // the response is still filled
}

TEST(Request, TimeoutWhenNobodyAnswersAndTheClientStaysUsable) {
  Fixture f;
  ASSERT_TRUE(f.client->Connect().ok());
  pbm::DeviceInfoRequest req;
  pbm::DeviceInfoResponse rsp;
  const auto t0 = std::chrono::steady_clock::now();
  Result r = f.client->Request(kReqTopic, req, kRspTopic, &rsp, milliseconds(150));
  const auto waited = std::chrono::steady_clock::now() - t0;
  EXPECT_EQ(r.code, Errc::RequestTimeout);
  EXPECT_GE(waited, milliseconds(140));
  EXPECT_LT(waited, milliseconds(1000));
  EXPECT_TRUE(f.client->IsConnected());

  // The next request works.
  f.fake->SetPublishHook(Answering(kSuccess));
  EXPECT_TRUE(f.client->Request(kReqTopic, req, kRspTopic, &rsp).ok());
}

TEST(Request, ALateResponseIsNotTakenForTheNextRequest) {
  Fixture f;
  ASSERT_TRUE(f.client->Connect().ok());
  pbm::DeviceInfoRequest req;
  pbm::DeviceInfoResponse rsp;

  // First request: the device answers after 300 ms, the client gives up after 100 ms.
  f.fake->SetPublishHook(Answering(kSuccess, "FIRST", milliseconds(300)));
  EXPECT_EQ(f.client->Request(kReqTopic, req, kRspTopic, &rsp, milliseconds(100)).code,
            Errc::RequestTimeout);

  // Second request: answered at once. It must get its own answer, not the stray FIRST.
  f.fake->SetPublishHook(Answering(kSuccess, "SECOND", milliseconds(0)));
  Result r = f.client->Request(kReqTopic, req, kRspTopic, &rsp, milliseconds(1000));
  ASSERT_TRUE(r.ok());
  EXPECT_EQ(rsp.error().desc(), "SECOND");

  // Let the stray FIRST response arrive; nothing must break.
  std::this_thread::sleep_for(milliseconds(350));
  EXPECT_TRUE(f.client->IsConnected());
}

TEST(Request, AResponseWithAnotherSeqIsIgnored) {
  Fixture f;
  // The "device" first sends a response for a different request, then the right one.
  f.fake->SetPublishHook([](FakeTransport& t, const std::string&, const std::string& payload) {
    pbm::DeviceInfoRequest req;
    req.ParseFromString(payload);
    pbm::DeviceInfoResponse wrong;
    wrong.mutable_header()->set_seq(req.header().seq() + 1000);
    wrong.mutable_error()->set_code(kSuccess);
    wrong.mutable_error()->set_desc("WRONG");
    t.Inject(kRspTopic, wrong.SerializeAsString(), milliseconds(0));
    pbm::DeviceInfoResponse right;
    right.mutable_header()->set_seq(req.header().seq());
    right.mutable_error()->set_code(kSuccess);
    right.mutable_error()->set_desc("RIGHT");
    t.Inject(kRspTopic, right.SerializeAsString(), milliseconds(30));
  });
  ASSERT_TRUE(f.client->Connect().ok());
  pbm::DeviceInfoRequest req;
  pbm::DeviceInfoResponse rsp;
  ASSERT_TRUE(f.client->Request(kReqTopic, req, kRspTopic, &rsp).ok());
  EXPECT_EQ(rsp.error().desc(), "RIGHT");
}

TEST(Request, ConcurrentRequestsOnTheSameTopicEachGetTheirOwnAnswer) {
  Fixture f;
  // Answer with a delay that depends on the sequence number, so the answers arrive out of order.
  f.fake->SetPublishHook([](FakeTransport& t, const std::string&, const std::string& payload) {
    pbm::DeviceInfoRequest req;
    req.ParseFromString(payload);
    pbm::DeviceInfoResponse rsp;
    rsp.mutable_header()->CopyFrom(req.header());
    rsp.mutable_error()->set_code(kSuccess);
    rsp.mutable_error()->set_desc("answer-for-" + std::to_string(req.header().seq()));
    t.Inject(kRspTopic, rsp.SerializeAsString(),
             milliseconds(req.header().seq() % 3 == 0 ? 60 : 5));
  });
  ASSERT_TRUE(f.client->Connect().ok());

  constexpr int kThreads = 8;
  std::vector<std::future<bool>> results;
  for (int i = 0; i < kThreads; ++i) {
    results.push_back(std::async(std::launch::async, [&] {
      pbm::DeviceInfoRequest req;
      pbm::DeviceInfoResponse rsp;
      Result r = f.client->Request(kReqTopic, req, kRspTopic, &rsp, milliseconds(2000));
      if (!r.ok()) return false;
      // the answer names the seq of the request that this thread sent
      auto sent = f.fake->Published();
      for (const auto& p : sent) {
        pbm::DeviceInfoRequest w;
        w.ParseFromString(p.second);
        if (rsp.error().desc() == "answer-for-" + std::to_string(w.header().seq())) return true;
      }
      return false;
    }));
  }
  for (auto& r : results) EXPECT_TRUE(r.get());
  // All sequence numbers on the wire are different.
  std::set<std::uint32_t> seqs;
  for (const auto& p : f.fake->Published()) {
    pbm::DeviceInfoRequest w;
    w.ParseFromString(p.second);
    seqs.insert(w.header().seq());
  }
  EXPECT_EQ(seqs.size(), static_cast<size_t>(kThreads));
}

TEST(Request, WithoutConnectionOrWithBadArgumentsItFailsFast) {
  Fixture f;
  pbm::DeviceInfoRequest req;
  pbm::DeviceInfoResponse rsp;
  EXPECT_EQ(f.client->Request(kReqTopic, req, kRspTopic, &rsp).code, Errc::NotConnected);
  ASSERT_TRUE(f.client->Connect().ok());
  EXPECT_EQ(f.client->Request(kReqTopic, req, kRspTopic, nullptr).code, Errc::InvalidArgument);
  EXPECT_EQ(f.client->Request("", req, kRspTopic, &rsp).code, Errc::InvalidArgument);
  EXPECT_EQ(f.client->Request(kReqTopic, req, "", &rsp).code, Errc::InvalidArgument);
}

TEST(Request, DisconnectWhileWaitingReleasesTheCaller) {
  Fixture f;
  ASSERT_TRUE(f.client->Connect().ok());
  auto pending = std::async(std::launch::async, [&] {
    pbm::DeviceInfoRequest req;
    pbm::DeviceInfoResponse rsp;
    return f.client->Request(kReqTopic, req, kRspTopic, &rsp, milliseconds(5000));
  });
  std::this_thread::sleep_for(milliseconds(100));
  const auto t0 = std::chrono::steady_clock::now();
  f.client->Disconnect();
  Result r = pending.get();
  EXPECT_EQ(r.code, Errc::NotConnected);
  EXPECT_LT(std::chrono::steady_clock::now() - t0, milliseconds(1500))
      << "the caller was not released";
}

TEST(Request, SubscribeFailureIsReturnedAndNothingIsSent) {
  Fixture f;
  ASSERT_TRUE(f.client->Connect().ok());
  f.fake->fail_subscribe = true;
  pbm::DeviceInfoRequest req;
  pbm::DeviceInfoResponse rsp;
  EXPECT_EQ(f.client->Request(kReqTopic, req, kRspTopic, &rsp).code, Errc::TransportError);
  EXPECT_TRUE(f.fake->Published().empty());
}

TEST(Request, ResponseTopicIsSubscribedOnlyOnce) {
  Fixture f;
  f.fake->SetPublishHook(Answering(kSuccess));
  ASSERT_TRUE(f.client->Connect().ok());
  pbm::DeviceInfoRequest req;
  pbm::DeviceInfoResponse rsp;
  for (int i = 0; i < 3; ++i) ASSERT_TRUE(f.client->Request(kReqTopic, req, kRspTopic, &rsp).ok());
  EXPECT_EQ(f.fake->SubscribeTimeouts().size(), 1u);
}

// ---- subscriptions ----

TEST(Subscribe, HandlerGetsMatchingMessagesOnly) {
  Fixture f;
  ASSERT_TRUE(f.client->Connect().ok());
  std::mutex mu;
  std::vector<std::string> got;
  ASSERT_TRUE(f.client
                  ->Subscribe("lixel/application/report/+",
                              [&](const std::string& topic, const std::string& payload) {
                                std::lock_guard<std::mutex> lock(mu);
                                got.push_back(topic + "=" + payload);
                              })
                  .ok());
  EXPECT_TRUE(f.fake->IsSubscribed("lixel/application/report/+"));
  f.fake->Inject("lixel/application/report/heartbeat", "hb");
  f.fake->Inject("lixel/application/response/device_info", "other");
  f.fake->Inject("lixel/application/report/lio_pose", "pose");
  std::this_thread::sleep_for(milliseconds(150));
  std::lock_guard<std::mutex> lock(mu);
  ASSERT_EQ(got.size(), 2u);
  EXPECT_EQ(got[0], "lixel/application/report/heartbeat=hb");
  EXPECT_EQ(got[1], "lixel/application/report/lio_pose=pose");
}

TEST(Subscribe, TwoHandlersOnTheSameFilterBothRunAndUnsubscribeRemovesBoth) {
  Fixture f;
  ASSERT_TRUE(f.client->Connect().ok());
  std::atomic<int> a{0}, b{0};
  ASSERT_TRUE(
      f.client->Subscribe("t/x", [&](const std::string&, const std::string&) { ++a; }).ok());
  ASSERT_TRUE(
      f.client->Subscribe("t/x", [&](const std::string&, const std::string&) { ++b; }).ok());
  EXPECT_EQ(f.fake->SubscribeTimeouts().size(), 1u) << "the broker is asked once per filter";
  f.fake->Inject("t/x", "1");
  std::this_thread::sleep_for(milliseconds(100));
  EXPECT_EQ(a.load(), 1);
  EXPECT_EQ(b.load(), 1);
  ASSERT_TRUE(f.client->Unsubscribe("t/x").ok());
  EXPECT_FALSE(f.fake->IsSubscribed("t/x"));
  f.fake->Inject("t/x", "2");
  std::this_thread::sleep_for(milliseconds(100));
  EXPECT_EQ(a.load(), 1);
  EXPECT_EQ(b.load(), 1);
}

TEST(Subscribe, TypedHandlerParsesAndCountsBadPayloads) {
  Fixture f;
  ASSERT_TRUE(f.client->Connect().ok());
  std::promise<std::string> got;
  ASSERT_TRUE(f.client
                  ->SubscribeMessage("lixel/application/report/system_error", "SystemErrorReport",
                                     [&](const std::string&, const google::protobuf::Message& m) {
                                       const auto& e =
                                           static_cast<const pbm::SystemErrorReport&>(m);
                                       got.set_value(e.error().desc());
                                     })
                  .ok());
  f.fake->Inject("lixel/application/report/system_error", "\xff\xff\xff not protobuf");
  pbm::SystemErrorReport rep;
  rep.mutable_error()->set_code(0x32010001u);
  rep.mutable_error()->set_desc("lidar lost");
  f.fake->Inject("lixel/application/report/system_error", rep.SerializeAsString());
  auto fut = got.get_future();
  ASSERT_EQ(fut.wait_for(std::chrono::seconds(2)), std::future_status::ready);
  EXPECT_EQ(fut.get(), "lidar lost");
  EXPECT_EQ(f.client->DroppedMessages(), 1u);
}

TEST(Subscribe, BadArgumentsAndUnknownMessageNames) {
  Fixture f;
  EXPECT_EQ(f.client->Subscribe("t", [](const std::string&, const std::string&) {}).code,
            Errc::NotConnected);
  ASSERT_TRUE(f.client->Connect().ok());
  EXPECT_EQ(f.client->Subscribe("", [](const std::string&, const std::string&) {}).code,
            Errc::InvalidArgument);
  EXPECT_EQ(f.client->Subscribe("t", RawHandler()).code, Errc::InvalidArgument);
  EXPECT_EQ(f.client
                ->SubscribeMessage("t", "NoSuchMessage",
                                   [](const std::string&, const google::protobuf::Message&) {})
                .code,
            Errc::InvalidArgument);
}

TEST(Subscribe, ABrokerThatRefusesTheSubscriptionLeavesNoHandlerBehind) {
  Fixture f;
  ASSERT_TRUE(f.client->Connect().ok());
  f.fake->fail_subscribe = true;
  std::atomic<int> hits{0};
  EXPECT_EQ(
      f.client->Subscribe("t/y", [&](const std::string&, const std::string&) { ++hits; }).code,
      Errc::TransportError);
  f.fake->fail_subscribe = false;
  f.fake->Inject("t/y", "x");
  std::this_thread::sleep_for(milliseconds(100));
  EXPECT_EQ(hits.load(), 0);
}

// ---- callback discipline ----

TEST(Callbacks, RequestFromInsideAHandlerReturnsInsteadOfBlocking) {
  Fixture f;
  ASSERT_TRUE(f.client->Connect().ok());
  std::promise<Errc> outcome;
  ASSERT_TRUE(
      f.client
          ->Subscribe(
              "t/cb",
              [&](const std::string&, const std::string&) {
                pbm::DeviceInfoRequest req;
                pbm::DeviceInfoResponse rsp;
                outcome.set_value(
                    f.client->Request(kReqTopic, req, kRspTopic, &rsp, milliseconds(3000)).code);
              })
          .ok());
  const auto t0 = std::chrono::steady_clock::now();
  f.fake->Inject("t/cb", "go");
  auto fut = outcome.get_future();
  ASSERT_EQ(fut.wait_for(std::chrono::seconds(2)), std::future_status::ready);
  EXPECT_EQ(fut.get(), Errc::CalledFromCallback);
  EXPECT_LT(std::chrono::steady_clock::now() - t0, milliseconds(1000))
      << "it must not wait for the timeout";
}

TEST(Callbacks, ASlowHandlerDoesNotBlockOtherThreadsFromRequesting) {
  Fixture f;
  f.fake->SetPublishHook(Answering(kSuccess));
  ASSERT_TRUE(f.client->Connect().ok());
  std::atomic<bool> in_handler{false};
  ASSERT_TRUE(f.client
                  ->Subscribe("t/slow",
                              [&](const std::string&, const std::string&) {
                                in_handler = true;
                                std::this_thread::sleep_for(milliseconds(300));
                              })
                  .ok());
  f.fake->Inject("t/slow", "x");
  while (!in_handler) std::this_thread::sleep_for(milliseconds(5));
  // No lock is held while the handler runs: this thread can still publish. (The reply is
  // delivered after the slow handler, because the transport delivers one message at a time.)
  pbm::DeviceInfoRequest req;
  pbm::DeviceInfoResponse rsp;
  const auto t0 = std::chrono::steady_clock::now();
  Result r = f.client->Request(kReqTopic, req, kRspTopic, &rsp, milliseconds(2000));
  EXPECT_TRUE(r.ok());
  EXPECT_FALSE(f.fake->Published().empty());
  (void)t0;
}

// ---- reconnect ----

TEST(Reconnect, UserSubscriptionsAreRestoredAndResponseTopicsAreSubscribedAgainOnDemand) {
  Fixture f;
  ASSERT_TRUE(f.client->Connect().ok());
  std::atomic<int> hits{0};
  ASSERT_TRUE(
      f.client->Subscribe("t/keep", [&](const std::string&, const std::string&) { ++hits; }).ok());
  f.fake->SetPublishHook(Answering(kSuccess));
  pbm::DeviceInfoRequest req;
  pbm::DeviceInfoResponse rsp;
  ASSERT_TRUE(f.client->Request(kReqTopic, req, kRspTopic, &rsp).ok());
  ASSERT_TRUE(f.fake->IsSubscribed("t/keep"));
  ASSERT_TRUE(f.fake->IsSubscribed(kRspTopic));

  f.fake->SimulateReconnect();  // the broker forgets every subscription
  // The subscriptions the user asked for are restored by the client, without being asked again.
  for (int i = 0; i < 100 && !f.fake->IsSubscribed("t/keep"); ++i)
    std::this_thread::sleep_for(milliseconds(10));
  EXPECT_TRUE(f.fake->IsSubscribed("t/keep"));
  // Restoring runs on the transport's thread, so it must queue the request and not wait for the
  // acknowledgement that only this thread could deliver.
  EXPECT_EQ(f.fake->SubscribeTimeouts().back(), milliseconds(0));

  f.fake->Inject("t/keep", "again");
  std::this_thread::sleep_for(milliseconds(100));
  EXPECT_EQ(hits.load(), 1);

  // A response topic is subscribed again by the next Request; it works without any other step.
  ASSERT_TRUE(f.client->Request(kReqTopic, req, kRspTopic, &rsp).ok());
  EXPECT_TRUE(f.fake->IsSubscribed(kRspTopic));
}

TEST(Reconnect, ARequestThatWaitsDuringTheLossStillGetsItsAnswerAfterTheReconnect) {
  Fixture f;
  ASSERT_TRUE(f.client->Connect().ok());
  // The device answers 300 ms after the request; the connection is lost and comes back in between.
  f.fake->SetPublishHook(Answering(kSuccess, "late but there", milliseconds(300)));
  auto pending = std::async(std::launch::async, [&] {
    pbm::DeviceInfoRequest req;
    pbm::DeviceInfoResponse rsp;
    Result r = f.client->Request(kReqTopic, req, kRspTopic, &rsp, milliseconds(2000));
    return std::make_pair(r, rsp.error().desc());
  });
  std::this_thread::sleep_for(milliseconds(100));
  f.fake->SimulateReconnect();
  auto outcome = pending.get();
  EXPECT_TRUE(outcome.first.ok()) << ErrcName(outcome.first.code);
  EXPECT_EQ(outcome.second, "late but there");
}

// ---- lifetime ----

TEST(Lifetime, DestroyingAConnectedClientWhileMessagesArriveIsSafe) {
  for (int round = 0; round < 20; ++round) {
    auto t = std::unique_ptr<FakeTransport>(new FakeTransport);
    FakeTransport* fake = t.get();
    std::atomic<int> hits{0};
    {
      Client client(ClientOptions(), std::move(t));
      ASSERT_TRUE(client.Connect().ok());
      ASSERT_TRUE(
          client.Subscribe("t/#", [&](const std::string&, const std::string&) { ++hits; }).ok());
      for (int i = 0; i < 50; ++i) fake->Inject("t/a", "x", milliseconds(i % 5));
      std::this_thread::sleep_for(milliseconds(8));
    }  // the client goes away with messages still queued; nothing may touch it afterwards
  }
  SUCCEED();
}
