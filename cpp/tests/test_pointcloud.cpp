// Point cloud decoding: bytes made by the Python client side (lz4.block + the repo's ProtoCodec)
// must decode here, and damaged or hostile input must be refused without crashing.

#include <gtest/gtest.h>
#include <lz4.h>

#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include "lixel/openapi/pointcloud.h"

using namespace lixel::openapi;  // NOLINT
namespace pbm = ::openapi::aiot_msgs;

namespace {

std::string FromHex(const std::string& hex) {
  std::string out;
  for (std::size_t i = 0; i + 1 < hex.size(); i += 2) {
    out.push_back(static_cast<char>(std::stoi(hex.substr(i, 2), nullptr, 16)));
  }
  return out;
}

// Python: ProtoCodec().encode("MqttCompressMsg", {compression Lz4, compressed_size 56,
// compressed_data lz4.block.compress(report, store_size=False)}) with a report of three points,
// header stamp 1700000000123, scaler 1000, device_id "dev". Too short for LZ4 to find a match, so
// the block is all literals.
const char* kPythonSmall =
    "1838223af0290a0f10f6a1abfef96218d00f2203646576120e08b81710872718d804208486880812001213080110aa"
    "b4de7518"
    "e1a2f3ad0720ffffffff0f";

// Python: 64 points x = (i % 8) * 50, y = (i / 8) * 50, z = 1000, rgbi = 0x80808000 + i % 4,
// header stamp 5, scaler 1000. The report is 1031 bytes and compresses to 539, so the block
// contains real matches.
const char* kPythonMatches =
    "188708229b04f10c0a05100a18d00f120918d00f208080828408120b086418d00f20810d00930c08c80118d00f2082"
    "0e0073ac02"
    "18d00f20830e002690033700310c08f40e0004380028d804380026bc053800210b106100026e00320d08640f000238"
    "00420e08c8"
    "011000027200420e08ac021000027400420e0890031000023f00330e08f4100004400022d804200004400022bc0510"
    "0002400022"
    "0c10d000043e0013641000023e00430f08c8011100023f00430f08ac021100024000430f0890031100024300340f08"
    "f4110004"
    "440023d804220004440023bc051100024400220c104601024100330e0864100004400023c801110004400023ac0211"
    "0002400001"
    "840008430001840002110004440023d804330004440023bc051100024400280c10bc01330e0864100004400023c801"
    "11000440"
    "0023ac02110002400001840008430001840002110004440023d804330004440023bc051100024400220c1032020608"
    "0102100004"
    "400023c801210004400023ac02110002400001840002330002430001840002110004440023d804330004440023bc05"
    "1100024400"
    "220c10a802024100330e0864100004400023c801110004400023ac0211000240000184000843000184000211000444"
    "0023d8043"
    "30004440023bc051100024400220c101e0306840002100004400023c801210004400023ac021100024000018400023"
    "30002430"
    "001840002110004440023d804330004440023bc051100508380828408";

// Builds a MqttCompressMsg from a report, compressing with LZ4 like the device does.
pbm::MqttCompressMsg Pack(const pbm::LioPclReport& report) {
  const std::string raw = report.SerializeAsString();
  std::string comp(static_cast<std::size_t>(LZ4_compressBound(static_cast<int>(raw.size()))), '\0');
  const int n = LZ4_compress_default(raw.data(), &comp[0], static_cast<int>(raw.size()),
                                     static_cast<int>(comp.size()));
  comp.resize(static_cast<std::size_t>(n));
  pbm::MqttCompressMsg msg;
  msg.set_compression(pbm::MqttCompressMsg::CompressionLz4);
  msg.set_compressed_size(static_cast<std::uint32_t>(raw.size()));
  msg.set_compressed_data(comp);
  return msg;
}

pbm::LioPclReport Report(std::int64_t scaler, int count) {
  pbm::LioPclReport r;
  r.mutable_header()->set_stamp(42);
  r.mutable_header()->set_scaler(scaler);
  for (int i = 0; i < count; ++i) {
    auto* p = r.add_points();
    p->set_x(i);
    p->set_y(-i);
    p->set_z(i * 10);
    p->set_rgbi(static_cast<std::uint32_t>(i) * 2654435761u);
  }
  return r;
}

}  // namespace

TEST(PointCloud, DecodesWhatThePythonSideProduced_Literals) {
  PointCloud pc;
  Result r = DecodePointCloud(FromHex(kPythonSmall), &pc);
  ASSERT_TRUE(r.ok()) << ErrcName(r.code) << ": " << r.desc;
  EXPECT_EQ(pc.stamp, 1700000000123);
  EXPECT_EQ(pc.scaler, 1000);
  ASSERT_EQ(pc.points.size(), 3u);
  EXPECT_DOUBLE_EQ(pc.points[0].x, 1.5);
  EXPECT_DOUBLE_EQ(pc.points[0].y, -2.5);
  EXPECT_DOUBLE_EQ(pc.points[0].z, 0.3);
  EXPECT_EQ(pc.points[0].rgbi, 0x01020304u);
  EXPECT_DOUBLE_EQ(pc.points[1].x, 0.0);
  EXPECT_EQ(pc.points[1].rgbi, 0u);
  EXPECT_DOUBLE_EQ(pc.points[2].x, -0.001);
  EXPECT_DOUBLE_EQ(pc.points[2].y, 123456.789);
  EXPECT_DOUBLE_EQ(pc.points[2].z, -987654.321);
  EXPECT_EQ(pc.points[2].rgbi, 0xFFFFFFFFu);
}

TEST(PointCloud, DecodesWhatThePythonSideProduced_WithMatches) {
  PointCloud pc;
  Result r = DecodePointCloud(FromHex(kPythonMatches), &pc);
  ASSERT_TRUE(r.ok()) << ErrcName(r.code) << ": " << r.desc;
  EXPECT_EQ(pc.stamp, 5);
  ASSERT_EQ(pc.points.size(), 64u);
  for (int i = 0; i < 64; ++i) {
    const auto& p = pc.points[static_cast<std::size_t>(i)];
    EXPECT_DOUBLE_EQ(p.x, (i % 8) * 0.05) << "point " << i;
    EXPECT_DOUBLE_EQ(p.y, (i / 8) * 0.05) << "point " << i;
    EXPECT_DOUBLE_EQ(p.z, 1.0) << "point " << i;
    EXPECT_EQ(p.rgbi, 0x80808000u + static_cast<std::uint32_t>(i % 4)) << "point " << i;
  }
}

TEST(PointCloud, RoundTripOfALargeFrame) {
  pbm::MqttCompressMsg msg = Pack(Report(1000, 20000));
  PointCloud pc;
  ASSERT_TRUE(DecodePointCloud(msg, &pc).ok());
  ASSERT_EQ(pc.points.size(), 20000u);
  EXPECT_DOUBLE_EQ(pc.points[19999].x, 19.999);
  EXPECT_DOUBLE_EQ(pc.points[19999].y, -19.999);
  EXPECT_DOUBLE_EQ(pc.points[19999].z, 199.99);
  EXPECT_EQ(pc.points[12345].rgbi, 12345u * 2654435761u);
}

TEST(PointCloud, AnEmptyFrameIsValid) {
  PointCloud pc;
  pc.points.resize(3);
  ASSERT_TRUE(DecodePointCloud(Pack(Report(1000, 0)), &pc).ok());
  EXPECT_TRUE(pc.points.empty()) << "the output is replaced, not appended to";
}

TEST(PointCloud, ScalerZeroMeansNoScaling) {
  PointCloud pc;
  ASSERT_TRUE(DecodePointCloud(Pack(Report(0, 3)), &pc).ok());
  EXPECT_DOUBLE_EQ(pc.points[2].x, 2.0);
  EXPECT_DOUBLE_EQ(pc.points[2].z, 20.0);
}

TEST(PointCloud, ANegativeScalerIsRefusedNotGuessed) {
  PointCloud pc;
  Result r = DecodePointCloud(Pack(Report(-1000, 3)), &pc);
  EXPECT_EQ(r.code, Errc::Unsupported);
  EXPECT_NE(r.desc.find("-1000"), std::string::npos);
}

TEST(PointCloud, ZeroOrHugeSizeBeforeCompressionIsRefused) {
  pbm::MqttCompressMsg msg = Pack(Report(1000, 5));
  PointCloud pc;
  msg.set_compressed_size(0);
  EXPECT_EQ(DecodePointCloud(msg, &pc).code, Errc::InvalidArgument);
  msg.set_compressed_size(kMaxPointCloudBytes + 1);
  EXPECT_EQ(DecodePointCloud(msg, &pc).code, Errc::InvalidArgument);
  msg.set_compressed_size(0xFFFFFFFFu);
  EXPECT_EQ(DecodePointCloud(msg, &pc).code, Errc::InvalidArgument);
}

TEST(PointCloud, ZstdIsReportedAsUnsupported) {
  pbm::MqttCompressMsg msg = Pack(Report(1000, 5));
  msg.set_compression(pbm::MqttCompressMsg::CompressionZstd);
  PointCloud pc;
  EXPECT_EQ(DecodePointCloud(msg, &pc).code, Errc::Unsupported);
}

TEST(PointCloud, ASizeThatDoesNotMatchTheDataIsAnError) {
  const pbm::MqttCompressMsg good = Pack(Report(1000, 50));
  PointCloud pc;
  pbm::MqttCompressMsg bigger = good;
  bigger.set_compressed_size(good.compressed_size() + 10);  // decompresses to fewer bytes
  Result r = DecodePointCloud(bigger, &pc);
  EXPECT_EQ(r.code, Errc::DecodeFailed) << r.desc;
  pbm::MqttCompressMsg smaller = good;
  smaller.set_compressed_size(good.compressed_size() - 10);  // does not fit
  r = DecodePointCloud(smaller, &pc);
  EXPECT_EQ(r.code, Errc::DecodeFailed) << r.desc;
}

TEST(PointCloud, TruncatedAndEmptyDataAreErrors) {
  const pbm::MqttCompressMsg good = Pack(Report(1000, 50));
  PointCloud pc;
  pbm::MqttCompressMsg cut = good;
  cut.set_compressed_data(good.compressed_data().substr(0, good.compressed_data().size() / 2));
  EXPECT_EQ(DecodePointCloud(cut, &pc).code, Errc::DecodeFailed);
  pbm::MqttCompressMsg empty = good;
  empty.clear_compressed_data();
  EXPECT_EQ(DecodePointCloud(empty, &pc).code, Errc::DecodeFailed);
}

TEST(PointCloud, ValidLz4OfSomethingThatIsNotAReportIsAnError) {
  // 0x08 starts a varint field but the value never ends: not parseable as LioPclReport.
  pbm::LioPclReport dummy;
  const std::string junk(40, '\xff');
  std::string comp(static_cast<std::size_t>(LZ4_compressBound(40)), '\0');
  const int n = LZ4_compress_default(junk.data(), &comp[0], 40, static_cast<int>(comp.size()));
  comp.resize(static_cast<std::size_t>(n));
  pbm::MqttCompressMsg msg;
  msg.set_compressed_size(40);
  msg.set_compressed_data(comp);
  PointCloud pc;
  EXPECT_EQ(DecodePointCloud(msg, &pc).code, Errc::DecodeFailed);
}

TEST(PointCloud, ThePayloadMustBeAMqttCompressMsg) {
  PointCloud pc;
  EXPECT_EQ(DecodePointCloud(std::string("\xff\xff\xff", 3), &pc).code, Errc::DecodeFailed);
}

TEST(PointCloud, NullOutputIsRefused) {
  EXPECT_EQ(DecodePointCloud(Pack(Report(1000, 1)), nullptr).code, Errc::InvalidArgument);
  EXPECT_EQ(DecompressPointCloud(Pack(Report(1000, 1)), nullptr).code, Errc::InvalidArgument);
}

TEST(PointCloud, ARefusedFrameLeavesTheOutputAsItWas) {
  PointCloud pc;
  ASSERT_TRUE(DecodePointCloud(Pack(Report(1000, 4)), &pc).ok());
  pbm::MqttCompressMsg bad = Pack(Report(1000, 4));
  bad.set_compressed_size(0);
  EXPECT_FALSE(DecodePointCloud(bad, &pc).ok());
  EXPECT_EQ(pc.points.size(), 4u);
}

// Random bytes in every field: the decoder must answer with an error or a cloud, never crash or
// read outside the buffer (meaningful under AddressSanitizer).
TEST(PointCloud, RandomGarbageNeverCrashes) {
  std::mt19937 rng(12345);
  for (int round = 0; round < 3000; ++round) {
    pbm::MqttCompressMsg msg;
    msg.set_compressed_size(rng() % 4096);
    std::string data(rng() % 300, '\0');
    for (auto& c : data) c = static_cast<char>(rng());
    msg.set_compressed_data(data);
    PointCloud pc;
    (void)DecodePointCloud(msg, &pc);
  }
  SUCCEED();
}

// Flip single bytes of a valid block: any outcome except a crash is fine.
TEST(PointCloud, FlippedBytesNeverCrash) {
  const pbm::MqttCompressMsg good = Pack(Report(1000, 200));
  const std::string data = good.compressed_data();
  for (std::size_t i = 0; i < data.size(); ++i) {
    pbm::MqttCompressMsg msg = good;
    std::string d = data;
    d[i] = static_cast<char>(d[i] ^ 0xA5);
    msg.set_compressed_data(d);
    PointCloud pc;
    (void)DecodePointCloud(msg, &pc);
  }
  SUCCEED();
}
