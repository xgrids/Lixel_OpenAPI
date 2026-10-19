// A program that knows message names only: it includes no generated header and refers to no
// generated class. The registrations of the messages must still be linked in by the library.
// (Regression test: the subscribe examples once reported "unknown message name" because the
// generated code was dropped by the linker.)

#include <gtest/gtest.h>

#include "lixel/openapi/codec.h"

using namespace lixel::openapi;  // NOLINT

TEST(CodecStandalone, MessagesAreFoundByNameWithoutReferringToGeneratedCode) {
  for (const char* name : {"DeviceStatusReport", "LioPoseReport", "SystemErrorReport",
                           "MqttCompressMsg", "ModelingRequest", "DeviceInfoResponse"}) {
    EXPECT_NE(NewMessage(name), nullptr) << name;
  }
  EXPECT_EQ(NewMessage("NoSuchMessage"), nullptr);
}

TEST(CodecStandalone, JsonWorksToo) {
  auto msg = NewMessage("ModelingRequest");
  ASSERT_NE(msg, nullptr);
  ASSERT_TRUE(JsonToMessage(R"({"action":"AC_START","project_name":"p"})", msg.get()).ok());
  std::string json;
  ASSERT_TRUE(MessageToJson(*msg, &json, false).ok());
  EXPECT_NE(json.find("\"AC_START\""), std::string::npos) << json;
}
