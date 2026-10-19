// Build pipeline smoke tests. They need no device and no broker.

#include <MQTTAsync.h>
#include <gtest/gtest.h>
#include <lz4.h>

#include <cstdio>
#include <string>
#include <vector>

#include "lixel/openapi/version.h"
#include "openapi/aiot_msgs.pb.h"

namespace {

using openapi::aiot_msgs::Application;
using openapi::aiot_msgs::Modeling;
using openapi::aiot_msgs::ModelingRequest;
using openapi::aiot_msgs::ModelingResponse;

std::string ToHex(const std::string& bytes) {
  std::string out;
  char buf[3];
  for (unsigned char c : bytes) {
    std::snprintf(buf, sizeof(buf), "%02x", c);
    out += buf;
  }
  return out;
}

std::string FromHex(const std::string& hex) {
  std::string out;
  for (size_t i = 0; i + 1 < hex.size(); i += 2) {
    out.push_back(static_cast<char>(std::stoi(hex.substr(i, 2), nullptr, 16)));
  }
  return out;
}

// Produced by the Python client (example/core/proto_codec.py, protobuf 7.36.2):
//   ModelingRequest{header{seq=7, openapi_key="k-123"}, action=AC_START,
//                   project_name="demo", record_mode=RM_RECORD_CALC}
const char kPythonRequestHex[] = "0a09080732056b2d31323310011a0464656d6f2002";
//   ModelingResponse{header{seq=7}, action=AC_START, error{code=0x12040001, desc="success"}}
const char kPythonResponseHex[] = "0a02080710017a0f088180909001120773756363657373";

}  // namespace

TEST(Version, LibraryAndProtobufVersionsAreReported) {
  EXPECT_STREQ(lixel::openapi::VersionString(), LIXEL_EXPECTED_VERSION);
  EXPECT_STREQ(lixel::openapi::VersionString(), LIXEL_OPENAPI_VERSION_STRING);
  // The version the CMake configuration resolved must be the one compiled in.
  EXPECT_STREQ(lixel::openapi::ProtobufVersionString(), LIXEL_EXPECTED_PROTOBUF_VERSION);
}

TEST(ProtoWire, RequestBytesEqualThePythonClientBytes) {
  ModelingRequest req;
  req.mutable_header()->set_seq(7);
  req.mutable_header()->set_openapi_key("k-123");
  req.set_action(Application::AC_START);
  req.set_project_name("demo");
  req.set_record_mode(Modeling::RM_RECORD_CALC);

  std::string bytes;
  ASSERT_TRUE(req.SerializeToString(&bytes));
  EXPECT_EQ(ToHex(bytes), kPythonRequestHex);
}

TEST(ProtoWire, PythonRequestBytesParseToTheSameFields) {
  ModelingRequest req;
  ASSERT_TRUE(req.ParseFromString(FromHex(kPythonRequestHex)));
  EXPECT_EQ(req.header().seq(), 7u);
  EXPECT_EQ(req.header().openapi_key(), "k-123");
  EXPECT_TRUE(req.header().has_openapi_key());
  EXPECT_EQ(req.action(), Application::AC_START);
  EXPECT_EQ(req.project_name(), "demo");
  EXPECT_EQ(req.record_mode(), Modeling::RM_RECORD_CALC);
  EXPECT_EQ(req.scan_mode(), Modeling::SM_PCL);  // zero value, not on the wire
}

TEST(ProtoWire, PythonResponseBytesParseToTheDeviceSuccessCode) {
  ModelingResponse rsp;
  ASSERT_TRUE(rsp.ParseFromString(FromHex(kPythonResponseHex)));
  EXPECT_EQ(rsp.header().seq(), 7u);
  EXPECT_EQ(rsp.action(), Application::AC_START);
  EXPECT_EQ(rsp.error().code(), 0x12040001u);
  EXPECT_EQ(rsp.error().desc(), "success");
}

TEST(ProtoWire, ZeroValuesAreNotEncodedAndOptionalHasPresence) {
  ModelingRequest empty;
  EXPECT_EQ(empty.SerializeAsString(), "");
  EXPECT_FALSE(empty.header().has_openapi_key());

  // proto3 optional: an explicitly set empty key is distinguishable from an unset one.
  ModelingRequest with_empty_key;
  with_empty_key.mutable_header()->set_openapi_key("");
  EXPECT_TRUE(with_empty_key.header().has_openapi_key());
  EXPECT_FALSE(with_empty_key.SerializeAsString().empty());
}

TEST(Paho, ClientHandleCanBeCreatedAndDestroyedWithoutANetwork) {
  MQTTAsync handle = nullptr;
  int rc = MQTTAsync_create(&handle, "tcp://127.0.0.1:1883", "lixel-smoke",
                            MQTTCLIENT_PERSISTENCE_NONE, nullptr);
  EXPECT_EQ(rc, MQTTASYNC_SUCCESS);
  EXPECT_NE(handle, nullptr);
  MQTTAsync_destroy(&handle);
  EXPECT_EQ(handle, nullptr);
}

TEST(Lz4, BlockRoundTripWithTheSizeKnownInAdvance) {
  // The device sends the LZ4 block format and the size before compression.
  std::vector<char> raw(4096);
  for (size_t i = 0; i < raw.size(); ++i) raw[i] = static_cast<char>(i % 17);
  std::vector<char> packed(static_cast<size_t>(LZ4_compressBound(static_cast<int>(raw.size()))));
  int packed_size = LZ4_compress_default(raw.data(), packed.data(), static_cast<int>(raw.size()),
                                         static_cast<int>(packed.size()));
  ASSERT_GT(packed_size, 0);
  std::vector<char> out(raw.size());
  int out_size =
      LZ4_decompress_safe(packed.data(), out.data(), packed_size, static_cast<int>(out.size()));
  ASSERT_EQ(out_size, static_cast<int>(raw.size()));
  EXPECT_EQ(out, raw);
}
