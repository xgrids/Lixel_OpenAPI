// Interface table, JSON codec, status codes and result types. No device and no broker needed.

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <set>
#include <string>

#include "lixel/openapi/catalog.h"
#include "lixel/openapi/codec.h"
#include "lixel/openapi/codes.h"
#include "lixel/openapi/result.h"
#include "openapi/aiot_msgs.pb.h"

using namespace lixel::openapi;  // NOLINT

TEST(Catalog, ListsEveryInterfaceOnce) {
  std::size_t n = 0;
  const ApiInterface* all = Interfaces(&n);
  ASSERT_NE(all, nullptr);
  EXPECT_EQ(n, 19u);
  std::set<std::string> keys;
  for (std::size_t i = 0; i < n; ++i) keys.insert(all[i].key);
  EXPECT_EQ(keys.size(), n) << "duplicate keys";
}

TEST(Catalog, EveryMessageNameInTheTableExistsInTheProto) {
  std::size_t n = 0;
  const ApiInterface* all = Interfaces(&n);
  for (std::size_t i = 0; i < n; ++i) {
    for (const char* name :
         {all[i].request_message, all[i].response_message, all[i].report_message}) {
      if (name == nullptr || *name == '\0') continue;  // no endpoint, or the JSON heartbeat
      EXPECT_NE(NewMessage(name), nullptr) << all[i].key << ": unknown message " << name;
    }
  }
}

TEST(Catalog, CountsTopicsAndKnowsTheHeartbeatIsJson) {
  std::size_t n = 0;
  const ApiInterface* all = Interfaces(&n);
  int topics = 0;
  for (std::size_t i = 0; i < n; ++i) {
    topics += all[i].request_topic != nullptr;
    topics += all[i].response_topic != nullptr;
    topics += all[i].report_topic != nullptr;
  }
  EXPECT_EQ(topics,
            36);  // the Python catalog has 37: it lists report/device_config, the device has none

  const ApiInterface* hb = FindInterface("heartbeat");
  ASSERT_NE(hb, nullptr);
  EXPECT_STREQ(hb->report_message, "");
  EXPECT_EQ(hb->request_topic, nullptr);
}

TEST(Catalog, DeviceConfigHasNoReportTopic) {
  const ApiInterface* dc = FindInterface("device_config");
  ASSERT_NE(dc, nullptr);
  EXPECT_NE(dc->request_topic, nullptr);
  EXPECT_NE(dc->response_topic, nullptr);
  EXPECT_EQ(dc->report_topic, nullptr);
  EXPECT_EQ(FindInterfaceByTopic("lixel/application/report/device_config"), nullptr);
}

TEST(Catalog, HasTheAuthorizationQuery) {
  const ApiInterface* q = FindInterface("openapi_license");
  ASSERT_NE(q, nullptr);
  EXPECT_STREQ(q->request_topic, "lixel/application/request/openapi_license");
  EXPECT_STREQ(q->response_topic, "lixel/application/response/openapi_license");
  EXPECT_STREQ(q->request_message, "OpenapiLicenseRequest");
  EXPECT_STREQ(q->response_message, "OpenapiLicenseResponse");
  EXPECT_EQ(q->report_topic, nullptr);
  EXPECT_EQ(FindInterfaceByTopic("lixel/application/request/openapi_license"), q);
  EXPECT_EQ(FindInterfaceByTopic("lixel/application/response/openapi_license"), q);
  EXPECT_STREQ(MessageForTopic("lixel/application/response/openapi_license"),
               "OpenapiLicenseResponse");
}

TEST(Catalog, LooksUpByTopicAndKey) {
  const ApiInterface* m = FindInterfaceByTopic("lixel/application/response/modeling");
  ASSERT_NE(m, nullptr);
  EXPECT_STREQ(m->key, "modeling");
  EXPECT_STREQ(MessageForTopic("lixel/application/request/modeling"), "ModelingRequest");
  EXPECT_STREQ(MessageForTopic("lixel/application/response/modeling"), "ModelingResponse");
  EXPECT_STREQ(MessageForTopic("lixel/application/report/lio_pcl"), "MqttCompressMsg");
  EXPECT_EQ(MessageForTopic("lixel/application/request/nope"), nullptr);
  EXPECT_EQ(FindInterface("nope"), nullptr);
  EXPECT_EQ(FindInterface(nullptr), nullptr);
}

TEST(Codec, NewMessageFindsGeneratedTypesByName) {
  auto msg = NewMessage("ModelingRequest");
  ASSERT_NE(msg, nullptr);
  EXPECT_EQ(msg->GetDescriptor()->full_name(), "openapi.aiot_msgs.ModelingRequest");
  EXPECT_EQ(NewMessage("NoSuchMessage"), nullptr);
  EXPECT_EQ(NewMessage(""), nullptr);
}

TEST(Codec, JsonToMessageAndBack) {
  ::openapi::aiot_msgs::ModelingRequest req;
  Result r = JsonToMessage(
      R"({"header":{"seq":7,"openapi_key":"k-123"},"action":"AC_START","project_name":"demo","record_mode":"RM_RECORD_CALC"})",
      &req);
  ASSERT_TRUE(r.ok()) << r.desc;
  EXPECT_EQ(req.header().seq(), 7u);
  EXPECT_EQ(req.action(), ::openapi::aiot_msgs::Application::AC_START);
  EXPECT_EQ(req.project_name(), "demo");

  // The same bytes the Python client produces for this request (see test_smoke.cpp).
  EXPECT_EQ(req.SerializeAsString().size(), 21u);

  std::string json;
  ASSERT_TRUE(MessageToJson(req, &json, false).ok());
  EXPECT_NE(json.find("\"action\":\"AC_START\""), std::string::npos) << json;
  EXPECT_NE(json.find("\"project_name\":\"demo\""), std::string::npos) << json;
  // proto field names, and defaults are shown: mount_type is zero and still printed
  EXPECT_NE(json.find("\"mount_type\":\"MT_HAND\""), std::string::npos) << json;
}

TEST(Codec, MisspelledEnumAndFieldNamesAreReportedNotDropped) {
  ::openapi::aiot_msgs::ModelingRequest req;
  Result typo_enum = JsonToMessage(R"({"action":"AC_STRAT"})", &req);
  EXPECT_EQ(typo_enum.code, Errc::DecodeFailed);
  EXPECT_FALSE(typo_enum.desc.empty());

  Result typo_field = JsonToMessage(R"({"project_nmae":"x"})", &req);
  EXPECT_EQ(typo_field.code, Errc::DecodeFailed);

  // A failed parse must not leave the message half filled.
  ::openapi::aiot_msgs::ModelingRequest keep;
  keep.set_project_name("before");
  EXPECT_EQ(JsonToMessage(R"({"project_name":"after","action":"AC_STRAT"})", &keep).code,
            Errc::DecodeFailed);
  EXPECT_EQ(keep.project_name(), "before");

  // The lenient mode of the Python codec is still available when asked for.
  ::openapi::aiot_msgs::ModelingRequest lenient;
  Result r = JsonToMessage(R"({"project_nmae":"x","project_name":"ok"})", &lenient, true);
  EXPECT_TRUE(r.ok()) << r.desc;
  EXPECT_EQ(lenient.project_name(), "ok");
}

TEST(Codec, InvalidInputGivesResultsNotCrashes) {
  EXPECT_EQ(JsonToMessage("{", nullptr).code, Errc::InvalidArgument);
  ::openapi::aiot_msgs::ModelingRequest req;
  EXPECT_EQ(JsonToMessage("{ not json", &req).code, Errc::DecodeFailed);
  EXPECT_EQ(JsonToMessage("", &req).code, Errc::DecodeFailed);
  EXPECT_EQ(MessageToJson(req, nullptr).code, Errc::InvalidArgument);
  std::string out;
  EXPECT_EQ(DecodeToJson("NoSuchMessage", "", &out).code, Errc::InvalidArgument);
}

TEST(Codec, DecodeToJsonOfPythonBytes) {
  // ModelingResponse produced by the Python client: header.seq=7, action=AC_START,
  // error{code=0x12040001, desc="success"}
  const char hex[] = "0a02080710017a0f088180909001120773756363657373";
  std::string bytes;
  for (size_t i = 0; hex[i] != '\0'; i += 2)
    bytes.push_back(static_cast<char>(std::stoi(std::string(hex + i, 2), nullptr, 16)));
  std::string json;
  ASSERT_TRUE(DecodeToJson("ModelingResponse", bytes, &json, false).ok());
  EXPECT_NE(json.find("\"code\":302252033"), std::string::npos) << json;  // 0x12040001
  EXPECT_NE(json.find("\"desc\":\"success\""), std::string::npos) << json;
  EXPECT_NE(json.find("\"action\":\"AC_START\""), std::string::npos) << json;

  EXPECT_EQ(DecodeToJson("ModelingResponse", "\xff\xff\xff", &json).code, Errc::DecodeFailed);
}

TEST(Codec, DefaultJsonShowsEveryFieldOfARequest) {
  std::string json;
  ASSERT_TRUE(DefaultJson("ModelingRequest", &json).ok());
  for (const char* field :
       {"action", "project_name", "record_mode", "scan_mode", "mount_type", "assist_mode"}) {
    EXPECT_NE(json.find(std::string("\"") + field + "\""), std::string::npos)
        << field << " missing in " << json;
  }
  EXPECT_EQ(DefaultJson("NoSuchMessage", &json).code, Errc::InvalidArgument);
}

namespace {
std::string FromHex(const char* hex) {
  std::string bytes;
  for (size_t i = 0; hex[i] != '\0'; i += 2)
    bytes.push_back(static_cast<char>(std::stoi(std::string(hex + i, 2), nullptr, 16)));
  return bytes;
}

::openapi::aiot_msgs::OpenapiLicenseResponse MakeAuthorization(bool licensed, int64_t granted,
                                                               int64_t used, int64_t remaining,
                                                               const char* state,
                                                               const char* state_text) {
  ::openapi::aiot_msgs::OpenapiLicenseResponse rsp;
  rsp.set_action(::openapi::aiot_msgs::Application::AC_GET_CONFIG);
  auto* info = rsp.mutable_info();
  info->set_licensed(licensed);
  if (licensed) {
    info->set_device_sn("FAKE-0001");
    info->set_code("TEST-CODE-0001");
  }
  info->set_granted_min(granted);
  info->set_used_min(used);
  info->set_remaining_min(remaining);
  info->set_state(state);
  info->set_state_text(state_text);
  rsp.mutable_error()->set_code(kSuccess);
  rsp.mutable_error()->set_desc("success");
  return rsp;
}
}  // namespace

TEST(Codec, AuthorizationResponseKeepsTheValuesOfEveryState) {
  struct Case {
    const char* name;
    ::openapi::aiot_msgs::OpenapiLicenseResponse rsp;
    const char* granted;
    const char* remaining;
    const char* state;
  };
  const Case cases[] = {
      {"none", MakeAuthorization(false, 0, 0, 0, "none", "Not authorized"), "0", "0", "none"},
      {"limited", MakeAuthorization(true, 600, 125, 475, "active", "Authorized"), "600", "475",
       "active"},
      {"permanent", MakeAuthorization(true, -1, 125, -1, "active", "Authorized"), "-1", "-1",
       "active"},
      {"expired", MakeAuthorization(true, 600, 600, 0, "expired", "Expired"), "600", "0",
       "expired"},
  };
  for (const Case& c : cases) {
    SCOPED_TRACE(c.name);
    // bytes round trip
    ::openapi::aiot_msgs::OpenapiLicenseResponse parsed;
    ASSERT_TRUE(parsed.ParseFromString(c.rsp.SerializeAsString()));
    EXPECT_EQ(parsed.info().granted_min(), c.rsp.info().granted_min());
    EXPECT_EQ(parsed.info().used_min(), c.rsp.info().used_min());
    EXPECT_EQ(parsed.info().remaining_min(), c.rsp.info().remaining_min());
    EXPECT_EQ(parsed.info().state(), c.state);
    // JSON round trip: int64 values are strings, -1 stays -1
    std::string json;
    ASSERT_TRUE(MessageToJson(c.rsp, &json, false).ok());
    EXPECT_NE(json.find(std::string("\"granted_min\":\"") + c.granted + "\""),
              std::string::npos)
        << json;
    EXPECT_NE(json.find(std::string("\"remaining_min\":\"") + c.remaining + "\""),
              std::string::npos)
        << json;
    ::openapi::aiot_msgs::OpenapiLicenseResponse again;
    ASSERT_TRUE(JsonToMessage(json, &again).ok()) << json;
    EXPECT_EQ(again.SerializeAsString(), c.rsp.SerializeAsString());
  }
  // none: the text fields stay empty and the number fields stay 0
  const auto none = MakeAuthorization(false, 0, 0, 0, "none", "Not authorized");
  EXPECT_FALSE(none.info().licensed());
  EXPECT_TRUE(none.info().device_sn().empty());
  EXPECT_TRUE(none.info().code().empty());
}

TEST(Codec, AuthorizationResponseMatchesTheDeviceWireFormat) {
  // Encoded with protoc from the device-side proto: error 0x12040001 "success", action
  // AC_GET_CONFIG, no header.
  const struct {
    const char* hex;
    bool licensed;
    int64_t granted, used, remaining;
    const char* state;
  } samples[] = {
      {"10221a3a0801220946414b452d303030312a0e544553542d434f44452d3030303130b00938fa0140b6074a0661"
       "6374697665520a417574686f72697a65647a0f088180909001120773756363657373",
       true, 600, 125, 475, "active"},
      {"10221a380801220946414b452d303030312a0e544553542d434f44452d30303031300138fa0140014a06616374"
       "697665520a417574686f72697a65647a0f088180909001120773756363657373",
       true, -1, 125, -1, "active"},
      {"10221a350801220946414b452d303030312a0e544553542d434f44452d3030303130b00938b0094a0765787069"
       "7265645207457870697265647a0f088180909001120773756363657373",
       true, 600, 600, 0, "expired"},
      {"10221a164a046e6f6e65520e4e6f7420617574686f72697a65647a0f088180909001120773756363657373",
       false, 0, 0, 0, "none"},
  };
  for (const auto& sample : samples) {
    SCOPED_TRACE(sample.state);
    const std::string bytes = FromHex(sample.hex);
    ::openapi::aiot_msgs::OpenapiLicenseResponse rsp;
    ASSERT_TRUE(rsp.ParseFromString(bytes));
    EXPECT_EQ(rsp.action(), ::openapi::aiot_msgs::Application::AC_GET_CONFIG);
    EXPECT_EQ(rsp.info().licensed(), sample.licensed);
    EXPECT_EQ(rsp.info().granted_min(), sample.granted);
    EXPECT_EQ(rsp.info().used_min(), sample.used);
    EXPECT_EQ(rsp.info().remaining_min(), sample.remaining);
    EXPECT_EQ(rsp.info().state(), sample.state);
    EXPECT_EQ(rsp.error().code(), kSuccess);
    EXPECT_EQ(rsp.error().desc(), "success");
    // our encoder produces the same bytes
    EXPECT_EQ(rsp.SerializeAsString(), bytes);
    // and the codec decodes them by message name
    std::string json;
    ASSERT_TRUE(DecodeToJson("OpenapiLicenseResponse", bytes, &json, false).ok());
    EXPECT_NE(json.find(std::string("\"state\":\"") + sample.state + "\""), std::string::npos)
        << json;
  }
  // the request is AC_GET_CONFIG = 34 in field 2
  ::openapi::aiot_msgs::OpenapiLicenseRequest req;
  req.set_action(::openapi::aiot_msgs::Application::AC_GET_CONFIG);
  EXPECT_EQ(req.SerializeAsString(), FromHex("1022"));
}

TEST(Codes, SuccessAndStateValuesMatchTheDevice) {
  // Values measured on a Lixel L2Pro and listed in the protocol reference.
  EXPECT_EQ(kSuccess, 302252033u);  // device_info answered with error.code=302252033
  EXPECT_EQ(kScanReady, 302252332u);
  EXPECT_EQ(kScanModeling, 302252334u);
  EXPECT_TRUE(IsSuccessCode(kSuccess));
  EXPECT_TRUE(IsSuccessCode(kSuccessAndReboot));
  EXPECT_FALSE(IsSuccessCode(0x32040001u));  // EC_ProtoError, for example "openapi auth error"
  EXPECT_FALSE(IsSuccessCode(0));

  EXPECT_STREQ(StateName(kScanReady), "standby");
  EXPECT_STREQ(StateName(kScanModeling), "scanning");
  EXPECT_NE(std::strstr(StateName(kScanNotAllowedInUsbMass), "USB"), nullptr);
  EXPECT_STREQ(StateName(0x12345678u), "0x12345678");
}

TEST(Result, DefaultsToSuccessAndCarriesTheReason) {
  Result ok;
  EXPECT_TRUE(ok.ok());
  EXPECT_TRUE(static_cast<bool>(ok));
  Result bad = Result::Make(Errc::RequestTimeout, "no response");
  EXPECT_FALSE(bad.ok());
  EXPECT_EQ(bad.desc, "no response");
  EXPECT_STREQ(ErrcName(Errc::RequestTimeout), "RequestTimeout");
  EXPECT_STREQ(ErrcName(Errc::Ok), "Ok");
}
