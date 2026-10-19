// Interface table, JSON codec, status codes and result types. No device and no broker needed.

#include <gtest/gtest.h>

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
  EXPECT_EQ(n, 18u);
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
            34);  // the Python catalog has 35: it lists report/device_config, the device has none

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
