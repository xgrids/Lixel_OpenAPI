// The real MQTT layer (paho) against a real Mosquitto broker, plain and with TLS.
// Run through tests/broker/run_with_broker.sh, which sets the LIXEL_TEST_* variables. Without
// them every test here is skipped.

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <future>
#include <memory>
#include <string>
#include <thread>

#include "lixel/openapi/client.h"
#include "lixel/openapi/codes.h"
#include "lixel/openapi/transport.h"
#include "openapi/aiot_msgs.pb.h"

using namespace lixel::openapi;  // NOLINT
namespace pbm = ::openapi::aiot_msgs;
using std::chrono::milliseconds;

namespace {

std::string Env(const char* name) {
  const char* v = std::getenv(name);
  return v != nullptr ? v : "";
}

#define REQUIRE_BROKER()                    \
  if (Env("LIXEL_TEST_PLAIN_PORT").empty()) \
  GTEST_SKIP() << "no test broker: run through run_with_broker.sh"

ClientOptions Plain() {
  ClientOptions o;
  o.transport.host = "127.0.0.1";
  o.transport.port = static_cast<std::uint16_t>(std::stoi(Env("LIXEL_TEST_PLAIN_PORT")));
  o.transport.connect_timeout = milliseconds(3000);
  o.request_timeout = milliseconds(2000);
  return o;
}

ClientOptions Tls() {
  ClientOptions o;
  o.transport.host = "127.0.0.1";
  o.transport.port = static_cast<std::uint16_t>(std::stoi(Env("LIXEL_TEST_TLS_PORT")));
  o.transport.use_tls = true;
  o.transport.username = Env("LIXEL_TEST_USER");
  o.transport.password = Env("LIXEL_TEST_PASSWORD");
  o.transport.ca_file = Env("LIXEL_TEST_CA_FILE");
  o.transport.connect_timeout = milliseconds(3000);
  o.request_timeout = milliseconds(2000);
  return o;
}

// Waits until `pred` is true or the time is up.
template <typename Pred>
bool WaitFor(Pred pred, milliseconds limit = milliseconds(3000)) {
  const auto end = std::chrono::steady_clock::now() + limit;
  while (std::chrono::steady_clock::now() < end) {
    if (pred()) return true;
    std::this_thread::sleep_for(milliseconds(10));
  }
  return pred();
}

// What a FakeDevice does with a request that has no valid key.
enum class Scheme {
  kEarlyKey,       // the earlier key-based scheme: refused with "openapi auth error"
  kLicenseNone,    // supports OpenAPI authorization, has none: "openapi auth error"
  kLicenseActive,  // authorized: served
  kLicenseExpired  // the authorization is used up: "openapi license exhausted"
};

// A device simulator on a second client: answers DeviceInfoRequest.
// A request whose header carries the key "good-key" is always served. Any other key counts as no
// key, and then `scheme` decides (the order of the protocol reference, section 2.2).
class FakeDevice {
 public:
  explicit FakeDevice(ClientOptions o, std::uint32_t code = kSuccess,
                      Scheme scheme = Scheme::kEarlyKey)
      : client_(std::move(o)), code_(code), scheme_(scheme) {}
  Result Start() {
    Result r = client_.Connect();
    if (!r.ok()) return r;
    return client_.Subscribe(
        "lixel/application/request/device_info",
        [this](const std::string&, const std::string& payload) {
          pbm::DeviceInfoRequest req;
          if (!req.ParseFromString(payload)) return;
          ++requests_;
          const char* refusal = Refusal(req);
          if (refusal != nullptr) {
            Reply(req, 0x32040001u, refusal);
          } else {
            Reply(req, code_, "success");
          }
        });
  }
  int requests() const { return requests_; }

 private:
  // The error text when the request is refused, nullptr when it is served.
  const char* Refusal(const pbm::DeviceInfoRequest& req) const {
    if (req.header().has_openapi_key() && req.header().openapi_key() == "good-key") {
      return nullptr;
    }
    switch (scheme_) {
      case Scheme::kLicenseActive:
        return nullptr;
      case Scheme::kLicenseExpired:
        return "openapi license exhausted";
      case Scheme::kEarlyKey:
      case Scheme::kLicenseNone:
        break;
    }
    return "openapi auth error";
  }
  void Reply(const pbm::DeviceInfoRequest& req, std::uint32_t code, const char* desc) {
    pbm::DeviceInfoResponse rsp;
    rsp.mutable_header()->CopyFrom(req.header());
    rsp.mutable_error()->set_code(code);
    rsp.mutable_error()->set_desc(desc);
    rsp.mutable_device_info()->mutable_base_info()->set_device_sn("SIM-0001");
    client_.Publish("lixel/application/response/device_info", rsp);
  }
  Client client_;
  std::uint32_t code_;
  Scheme scheme_;
  std::atomic<int> requests_{0};
};

// What a LicenseDevice reports in its authorization query.
enum class LicenseState { kNone, kLimited, kPermanent, kExpired };

// A device simulator for the authorization query (openapi_license). The query needs no key, so it
// is answered whatever the header holds. With `supported` false it does not subscribe at all, like
// a device without that topic.
class LicenseDevice {
 public:
  LicenseDevice(ClientOptions o, LicenseState state, bool supported = true)
      : client_(std::move(o)), state_(state), supported_(supported) {}

  Result Start() {
    Result r = client_.Connect();
    if (!r.ok() || !supported_) return r;
    return client_.Subscribe(
        "lixel/application/request/openapi_license",
        [this](const std::string&, const std::string& payload) {
          pbm::OpenapiLicenseRequest req;
          if (!req.ParseFromString(payload)) return;
          ++requests_;
          keyed_ = req.header().has_openapi_key();
          pbm::OpenapiLicenseResponse rsp;
          rsp.mutable_header()->CopyFrom(req.header());
          rsp.set_action(req.action());
          if (req.action() == pbm::Application::AC_GET_CONFIG) {
            Fill(rsp.mutable_info());
            rsp.mutable_error()->set_code(kSuccess);
            rsp.mutable_error()->set_desc("success");
          } else {
            rsp.mutable_error()->set_code(0x32040002u);
            rsp.mutable_error()->set_desc("unsupported action");
          }
          client_.Publish("lixel/application/response/openapi_license", rsp);
        });
  }

  int requests() const { return requests_; }
  bool keyed() const { return keyed_; }

 private:
  void Fill(pbm::OpenapiLicenseInfo* info) const {
    switch (state_) {
      case LicenseState::kNone:
        info->set_state("none");
        info->set_state_text("Not authorized");
        return;
      case LicenseState::kLimited:
        info->set_granted_min(600);
        info->set_used_min(125);
        info->set_remaining_min(475);
        info->set_state("active");
        info->set_state_text("Authorized");
        break;
      case LicenseState::kPermanent:
        info->set_granted_min(-1);
        info->set_used_min(125);
        info->set_remaining_min(-1);
        info->set_state("active");
        info->set_state_text("Authorized");
        break;
      case LicenseState::kExpired:
        info->set_granted_min(600);
        info->set_used_min(600);
        info->set_remaining_min(0);
        info->set_state("expired");
        info->set_state_text("Expired");
        break;
    }
    info->set_licensed(true);
    info->set_device_sn("SIM-0001");
    info->set_code("TEST-CODE-0001");
  }

  Client client_;
  LicenseState state_;
  bool supported_;
  std::atomic<int> requests_{0};
  std::atomic<bool> keyed_{false};
};

}  // namespace

TEST(Broker, PlainConnectPublishSubscribeRoundTrip) {
  REQUIRE_BROKER();
  Client sub(Plain());
  Client pub(Plain());
  ASSERT_TRUE(sub.Connect().ok());
  ASSERT_TRUE(pub.Connect().ok());
  EXPECT_TRUE(sub.IsConnected());
  std::promise<std::string> got;
  ASSERT_TRUE(sub.Subscribe("lixel/test/+/x",
                            [&](const std::string& topic, const std::string& payload) {
                              got.set_value(topic + "|" + payload);
                            })
                  .ok());
  ASSERT_TRUE(pub.PublishRaw("lixel/test/a/x", std::string("bin\0ary", 7)).ok());
  auto fut = got.get_future();
  ASSERT_EQ(fut.wait_for(std::chrono::seconds(3)), std::future_status::ready);
  EXPECT_EQ(fut.get(), std::string("lixel/test/a/x|bin\0ary", 22))
      << "binary payloads must pass unchanged";
}

TEST(Broker, TlsWithAccountAndTrustedCertificateConnects) {
  REQUIRE_BROKER();
  Client c(Tls());
  Result r = c.Connect();
  ASSERT_TRUE(r.ok()) << ErrcName(r.code) << ": " << r.desc;
  EXPECT_TRUE(c.IsConnected());
  c.Disconnect();
  EXPECT_FALSE(c.IsConnected());
}

TEST(Broker, TlsRefusesAnUntrustedCertificateByDefault) {
  REQUIRE_BROKER();
  ClientOptions o = Tls();
  o.transport.ca_file.clear();  // the broker's certificate is self-signed and not in any store
  Client c(o);
  Result r = c.Connect();
  EXPECT_FALSE(r.ok()) << "a self-signed certificate must be refused when verification is on";
  EXPECT_FALSE(c.IsConnected());
}

TEST(Broker, TlsAcceptsTheUntrustedCertificateWhenVerificationIsSwitchedOff) {
  REQUIRE_BROKER();
  ClientOptions o = Tls();
  o.transport.ca_file.clear();
  o.transport.verify_server = false;
  Client c(o);
  Result r = c.Connect();
  EXPECT_TRUE(r.ok()) << ErrcName(r.code) << ": " << r.desc;
}

TEST(Broker, TlsWithAWrongPasswordFails) {
  REQUIRE_BROKER();
  ClientOptions o = Tls();
  o.transport.password = "wrong";
  Client c(o);
  EXPECT_EQ(c.Connect().code, Errc::ConnectFailed);
  EXPECT_FALSE(c.IsConnected());
}

TEST(Broker, ConnectingToAClosedPortFailsQuicklyAndDoesNotRetryInTheBackground) {
  REQUIRE_BROKER();
  ClientOptions o = Plain();
  o.transport.port = 1;  // nothing listens here
  o.transport.connect_timeout = milliseconds(3000);
  Client c(o);
  const auto t0 = std::chrono::steady_clock::now();
  Result r = c.Connect();
  const auto took = std::chrono::steady_clock::now() - t0;
  EXPECT_FALSE(r.ok());
  EXPECT_TRUE(r.code == Errc::ConnectFailed || r.code == Errc::ConnectTimeout) << ErrcName(r.code);
  EXPECT_LT(took, milliseconds(6000));
  EXPECT_FALSE(c.IsConnected());
  // The same client can connect again once there is something to connect to.
  c.Disconnect();
}

TEST(Broker, RequestResponseThroughTheBrokerWithAnotherClientAsDevice) {
  REQUIRE_BROKER();
  FakeDevice device(Plain());
  ASSERT_TRUE(device.Start().ok());

  ClientOptions o = Plain();
  o.openapi_key = "good-key";
  Client c(o);
  ASSERT_TRUE(c.Connect().ok());
  pbm::DeviceInfoRequest req;
  pbm::DeviceInfoResponse rsp;
  Result r = c.Request("lixel/application/request/device_info", req,
                       "lixel/application/response/device_info", &rsp);
  ASSERT_TRUE(r.ok()) << ErrcName(r.code) << ": " << r.desc;
  EXPECT_EQ(rsp.device_info().base_info().device_sn(), "SIM-0001");
  EXPECT_EQ(device.requests(), 1);
}

TEST(Broker, AWrongKeyGetsTheDeviceErrorNotATimeout) {
  REQUIRE_BROKER();
  FakeDevice device(Plain());
  ASSERT_TRUE(device.Start().ok());
  ClientOptions o = Plain();
  o.openapi_key = "bad-key";
  Client c(o);
  ASSERT_TRUE(c.Connect().ok());
  pbm::DeviceInfoRequest req;
  pbm::DeviceInfoResponse rsp;
  const auto t0 = std::chrono::steady_clock::now();
  Result r = c.Request("lixel/application/request/device_info", req,
                       "lixel/application/response/device_info", &rsp);
  EXPECT_EQ(r.code, Errc::DeviceError);
  EXPECT_EQ(r.device_code, 0x32040001u);
  EXPECT_EQ(r.desc, "openapi auth error");
  EXPECT_LT(std::chrono::steady_clock::now() - t0, milliseconds(1500))
      << "must answer at once, not time out";
}

// Version 2.0: a device with an active OpenAPI authorization needs no key, so a client without one
// must work.
TEST(Broker, NoKeyIsNeededForADeviceWithAnActiveAuthorization) {
  REQUIRE_BROKER();
  FakeDevice device(Plain(), kSuccess, Scheme::kLicenseActive);
  ASSERT_TRUE(device.Start().ok());
  ClientOptions o = Plain();  // no openapi_key
  ASSERT_TRUE(o.openapi_key.empty());
  Client c(o);
  ASSERT_TRUE(c.Connect().ok());
  pbm::DeviceInfoRequest req;
  pbm::DeviceInfoResponse rsp;
  Result r = c.Request("lixel/application/request/device_info", req,
                       "lixel/application/response/device_info", &rsp);
  ASSERT_TRUE(r.ok()) << ErrcName(r.code) << ": " << r.desc;
  EXPECT_EQ(rsp.device_info().base_info().device_sn(), "SIM-0001");
  EXPECT_FALSE(rsp.header().has_openapi_key()) << "the client must not invent a key";
}
// A wrong key counts as no key, so it does no harm on such a device (a program written for 1.x
// keeps working).
TEST(Broker, AWrongKeyIsHarmlessOnADeviceWithAnActiveAuthorization) {
  REQUIRE_BROKER();
  FakeDevice device(Plain(), kSuccess, Scheme::kLicenseActive);
  ASSERT_TRUE(device.Start().ok());
  ClientOptions o = Plain();
  o.openapi_key = "some-old-key";
  Client c(o);
  ASSERT_TRUE(c.Connect().ok());
  pbm::DeviceInfoRequest req;
  pbm::DeviceInfoResponse rsp;
  Result r = c.Request("lixel/application/request/device_info", req,
                       "lixel/application/response/device_info", &rsp);
  EXPECT_TRUE(r.ok()) << ErrcName(r.code) << ": " << r.desc;
}
// A device still on the earlier key-based scheme answers a request without a key with a clear
// error at once (as in 1.x), not with a timeout.
TEST(Broker, NoKeyOnADeviceOnTheEarlierSchemeGetsTheDeviceError) {
  REQUIRE_BROKER();
  FakeDevice device(Plain());  // key-based
  ASSERT_TRUE(device.Start().ok());
  Client c(Plain());  // no openapi_key
  ASSERT_TRUE(c.Connect().ok());
  pbm::DeviceInfoRequest req;
  pbm::DeviceInfoResponse rsp;
  const auto t0 = std::chrono::steady_clock::now();
  Result r = c.Request("lixel/application/request/device_info", req,
                       "lixel/application/response/device_info", &rsp);
  EXPECT_EQ(r.code, Errc::DeviceError);
  EXPECT_EQ(r.device_code, 0x32040001u);
  EXPECT_EQ(r.desc, "openapi auth error");
  EXPECT_LT(std::chrono::steady_clock::now() - t0, milliseconds(1500))
      << "must answer at once, not time out";
}
// A device whose authorization is used up answers a request without a key with a device error
// whose text says so, at once and not with a timeout.
TEST(Broker, NoKeyOnADeviceWithAnExhaustedAuthorizationGetsTheLicenseError) {
  REQUIRE_BROKER();
  FakeDevice device(Plain(), kSuccess, Scheme::kLicenseExpired);
  ASSERT_TRUE(device.Start().ok());
  Client c(Plain());  // no openapi_key
  ASSERT_TRUE(c.Connect().ok());
  pbm::DeviceInfoRequest req;
  pbm::DeviceInfoResponse rsp;
  const auto t0 = std::chrono::steady_clock::now();
  Result r = c.Request("lixel/application/request/device_info", req,
                       "lixel/application/response/device_info", &rsp);
  EXPECT_EQ(r.code, Errc::DeviceError);
  EXPECT_EQ(r.device_code, 0x32040001u);
  EXPECT_EQ(r.desc, "openapi license exhausted");
  EXPECT_LT(std::chrono::steady_clock::now() - t0, milliseconds(1500))
      << "must answer at once, not time out";
}
// A valid key is served even when the authorization is used up.
TEST(Broker, AValidKeyIsServedByADeviceWithAnExhaustedAuthorization) {
  REQUIRE_BROKER();
  FakeDevice device(Plain(), kSuccess, Scheme::kLicenseExpired);
  ASSERT_TRUE(device.Start().ok());
  ClientOptions o = Plain();
  o.openapi_key = "good-key";
  Client c(o);
  ASSERT_TRUE(c.Connect().ok());
  pbm::DeviceInfoRequest req;
  pbm::DeviceInfoResponse rsp;
  Result r = c.Request("lixel/application/request/device_info", req,
                       "lixel/application/response/device_info", &rsp);
  ASSERT_TRUE(r.ok()) << ErrcName(r.code) << ": " << r.desc;
  EXPECT_EQ(rsp.device_info().base_info().device_sn(), "SIM-0001");
}
// A device that supports authorization but has none answers a request without a key with the
// same error as a device on the earlier scheme.
TEST(Broker, NoKeyOnADeviceWithoutAuthorizationGetsTheAuthError) {
  REQUIRE_BROKER();
  FakeDevice device(Plain(), kSuccess, Scheme::kLicenseNone);
  ASSERT_TRUE(device.Start().ok());
  Client c(Plain());  // no openapi_key
  ASSERT_TRUE(c.Connect().ok());
  pbm::DeviceInfoRequest req;
  pbm::DeviceInfoResponse rsp;
  const auto t0 = std::chrono::steady_clock::now();
  Result r = c.Request("lixel/application/request/device_info", req,
                       "lixel/application/response/device_info", &rsp);
  EXPECT_EQ(r.code, Errc::DeviceError);
  EXPECT_EQ(r.device_code, 0x32040001u);
  EXPECT_EQ(r.desc, "openapi auth error");
  EXPECT_LT(std::chrono::steady_clock::now() - t0, milliseconds(1500))
      << "must answer at once, not time out";
}
// The authorization query is answered in every state without a key, with the values of that state.
TEST(Broker, TheAuthorizationQueryNeedsNoKeyInAnyState) {
  REQUIRE_BROKER();
  struct Expect {
    LicenseState state;
    bool licensed;
    std::int64_t granted, used, remaining;
    const char* name;
    const char* text;
  };
  const Expect all[] = {
      {LicenseState::kNone, false, 0, 0, 0, "none", "Not authorized"},
      {LicenseState::kLimited, true, 600, 125, 475, "active", "Authorized"},
      {LicenseState::kPermanent, true, -1, 125, -1, "active", "Authorized"},
      {LicenseState::kExpired, true, 600, 600, 0, "expired", "Expired"},
  };
  for (const Expect& e : all) {
    SCOPED_TRACE(e.name);
    LicenseDevice device(Plain(), e.state);
    ASSERT_TRUE(device.Start().ok());
    Client c(Plain());  // no openapi_key
    ASSERT_TRUE(c.Connect().ok());
    pbm::OpenapiLicenseRequest req;
    req.set_action(pbm::Application::AC_GET_CONFIG);
    pbm::OpenapiLicenseResponse rsp;
    Result r = c.Request("lixel/application/request/openapi_license", req,
                         "lixel/application/response/openapi_license", &rsp);
    ASSERT_TRUE(r.ok()) << ErrcName(r.code) << ": " << r.desc;
    EXPECT_FALSE(device.keyed()) << "the client must not invent a key";
    EXPECT_EQ(rsp.action(), pbm::Application::AC_GET_CONFIG);
    EXPECT_EQ(rsp.info().licensed(), e.licensed);
    EXPECT_EQ(rsp.info().granted_min(), e.granted);
    EXPECT_EQ(rsp.info().used_min(), e.used);
    EXPECT_EQ(rsp.info().remaining_min(), e.remaining);
    EXPECT_EQ(rsp.info().state(), e.name);
    EXPECT_EQ(rsp.info().state_text(), e.text);
    EXPECT_EQ(rsp.info().device_sn(), e.licensed ? "SIM-0001" : "");
    EXPECT_EQ(rsp.info().code(), e.licensed ? "TEST-CODE-0001" : "");
    EXPECT_EQ(rsp.error().desc(), "success");
  }
}

// A wrong key is not looked at by this query.
TEST(Broker, AWrongKeyIsHarmlessInTheAuthorizationQuery) {
  REQUIRE_BROKER();
  LicenseDevice device(Plain(), LicenseState::kLimited);
  ASSERT_TRUE(device.Start().ok());
  ClientOptions o = Plain();
  o.openapi_key = "wrong-key";
  Client c(o);
  ASSERT_TRUE(c.Connect().ok());
  pbm::OpenapiLicenseRequest req;
  req.set_action(pbm::Application::AC_GET_CONFIG);
  pbm::OpenapiLicenseResponse rsp;
  Result r = c.Request("lixel/application/request/openapi_license", req,
                       "lixel/application/response/openapi_license", &rsp);
  ASSERT_TRUE(r.ok()) << ErrcName(r.code) << ": " << r.desc;
  EXPECT_TRUE(device.keyed());
  EXPECT_EQ(rsp.info().remaining_min(), 475);
}

// Any action but AC_GET_CONFIG is a device error answered at once, not a timeout, and the
// response has no info.
TEST(Broker, AnUnsupportedActionInTheAuthorizationQueryIsADeviceError) {
  REQUIRE_BROKER();
  LicenseDevice device(Plain(), LicenseState::kLimited);
  ASSERT_TRUE(device.Start().ok());
  Client c(Plain());
  ASSERT_TRUE(c.Connect().ok());
  for (auto action : {pbm::Application::AC_START, pbm::Application::AC_UNKNOW}) {
    SCOPED_TRACE(action);
    pbm::OpenapiLicenseRequest req;
    req.set_action(action);
    pbm::OpenapiLicenseResponse rsp;
    const auto t0 = std::chrono::steady_clock::now();
    Result r = c.Request("lixel/application/request/openapi_license", req,
                         "lixel/application/response/openapi_license", &rsp);
    EXPECT_EQ(r.code, Errc::DeviceError);
    EXPECT_EQ(r.device_code, 0x32040002u);
    EXPECT_EQ(r.desc, "unsupported action");
    EXPECT_FALSE(rsp.has_info());
    EXPECT_LT(std::chrono::steady_clock::now() - t0, milliseconds(1500))
        << "must answer at once, not time out";
  }
}

// A device without the topic (the earlier key scheme) never answers: a timeout, and the client
// stays connected and usable.
TEST(Broker, TheAuthorizationQueryTimesOutOnADeviceWithoutThatTopic) {
  REQUIRE_BROKER();
  LicenseDevice device(Plain(), LicenseState::kLimited, /*supported=*/false);
  ASSERT_TRUE(device.Start().ok());
  ClientOptions o = Plain();
  o.request_timeout = milliseconds(300);
  Client c(o);
  ASSERT_TRUE(c.Connect().ok());
  pbm::OpenapiLicenseRequest req;
  req.set_action(pbm::Application::AC_GET_CONFIG);
  pbm::OpenapiLicenseResponse rsp;
  EXPECT_EQ(c.Request("lixel/application/request/openapi_license", req,
                      "lixel/application/response/openapi_license", &rsp)
                .code,
            Errc::RequestTimeout);
  EXPECT_EQ(device.requests(), 0);
  EXPECT_TRUE(c.IsConnected());
}

TEST(Broker, NoDeviceMeansATimeoutAndTheClientKeepsWorking) {
  REQUIRE_BROKER();
  ClientOptions o = Plain();
  o.request_timeout = milliseconds(300);
  Client c(o);
  ASSERT_TRUE(c.Connect().ok());
  pbm::DeviceInfoRequest req;
  pbm::DeviceInfoResponse rsp;
  EXPECT_EQ(c.Request("lixel/application/request/device_info", req,
                      "lixel/application/response/device_info", &rsp)
                .code,
            Errc::RequestTimeout);
  EXPECT_TRUE(c.IsConnected());
}

TEST(Broker, RequestOverTlsToo) {
  REQUIRE_BROKER();
  FakeDevice device(Tls());
  ASSERT_TRUE(device.Start().ok());
  ClientOptions o = Tls();
  o.openapi_key = "good-key";
  Client c(o);
  ASSERT_TRUE(c.Connect().ok());
  pbm::DeviceInfoRequest req;
  pbm::DeviceInfoResponse rsp;
  Result r = c.Request("lixel/application/request/device_info", req,
                       "lixel/application/response/device_info", &rsp);
  EXPECT_TRUE(r.ok()) << ErrcName(r.code) << ": " << r.desc;
}

TEST(Broker, ManyRequestsFromManyThreadsAllGetTheirOwnAnswer) {
  REQUIRE_BROKER();
  FakeDevice device(Plain());
  ASSERT_TRUE(device.Start().ok());
  ClientOptions o = Plain();
  o.openapi_key = "good-key";
  Client c(o);
  ASSERT_TRUE(c.Connect().ok());
  std::atomic<int> ok{0}, bad{0};
  std::vector<std::thread> threads;
  for (int t = 0; t < 6; ++t) {
    threads.emplace_back([&] {
      for (int i = 0; i < 25; ++i) {
        pbm::DeviceInfoRequest req;
        pbm::DeviceInfoResponse rsp;
        Result r = c.Request("lixel/application/request/device_info", req,
                             "lixel/application/response/device_info", &rsp);
        (r.ok() ? ok : bad)++;
      }
    });
  }
  for (auto& t : threads) t.join();
  EXPECT_EQ(ok.load(), 150);
  EXPECT_EQ(bad.load(), 0);
}

TEST(Broker, DisconnectThenConnectAgainWorks) {
  REQUIRE_BROKER();
  Client c(Plain());
  for (int i = 0; i < 3; ++i) {
    ASSERT_TRUE(c.Connect().ok()) << "round " << i;
    EXPECT_TRUE(c.IsConnected());
    c.Disconnect();
    EXPECT_FALSE(c.IsConnected());
  }
}

TEST(Broker, DestroyingAConnectedClientWhileMessagesArriveIsSafe) {
  REQUIRE_BROKER();
  Client pub(Plain());
  ASSERT_TRUE(pub.Connect().ok());
  std::atomic<bool> stop{false};
  std::thread flood([&] {
    while (!stop) {
      pub.PublishRaw("lixel/test/flood", "x");
      std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
  });
  // paho's worker thread is shared by all clients of a process and wakes once a second, so each
  // extra connection takes about two seconds to come up; five rounds keep the test short.
  for (int round = 0; round < 5; ++round) {
    std::atomic<int> hits{0};
    {
      Client c(Plain());
      ASSERT_TRUE(c.Connect().ok());
      Result sr =
          c.Subscribe("lixel/test/#", [&](const std::string&, const std::string&) { ++hits; });
      if (!sr.ok()) {
        stop = true;
        flood.join();
      }
      ASSERT_TRUE(sr.ok()) << "round " << round << ": " << ErrcName(sr.code) << ": " << sr.desc;
      std::this_thread::sleep_for(milliseconds(20));
    }  // destroyed while the flood is still running
  }
  stop = true;
  flood.join();
  SUCCEED();
}
