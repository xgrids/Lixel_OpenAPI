// Decoding of the real-time point cloud report (topic lixel/application/report/lio_pcl).
//
// The device sends a MqttCompressMsg. Its compressed_data is the serialized LioPclReport,
// compressed as a raw LZ4 *block* (not the LZ4 frame format). compressed_size is the size before
// compression, which LZ4 needs to know how much to produce. All functions are thread safe.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "lixel/openapi/result.h"
#include "openapi/aiot_msgs.pb.h"
namespace lixel {
namespace openapi {
struct PointCloud {
  struct Point {
    // Coordinates after applying the scale factor of the report (see DecodePointCloud).
    double x = 0;
    double y = 0;
    double z = 0;
    // The device packs red, green, blue and intensity into this value. The byte order is not
    // documented in aiot_msgs.proto beyond "R+G+B+Intensity", so it is passed on unchanged.
    std::uint32_t rgbi = 0;
  };
  std::int64_t stamp = 0;   // LioPclReport.header.stamp
  std::int64_t scaler = 0;  // LioPclReport.header.scaler as received
  std::vector<Point> points;
};
// Largest size before compression that is accepted. A frame of 100000 points is about 2 MB;
// the limit stops a corrupt compressed_size from asking for gigabytes.
constexpr std::uint32_t kMaxPointCloudBytes = 64u * 1024u * 1024u;
// Decompresses MqttCompressMsg.compressed_data into the serialized LioPclReport.
//   InvalidArgument  compressed_size is 0 or larger than kMaxPointCloudBytes
//   Unsupported      Zstd, which is not implemented in this library
//   DecodeFailed     the data is not a valid LZ4 block, or it does not decompress to exactly
//                    compressed_size bytes
Result DecompressPointCloud(const ::openapi::aiot_msgs::MqttCompressMsg& msg,
                            std::string* serialized);
// Decompresses and parses the report in `msg`.
//
// Scale factor: with header.scaler > 0 the coordinates are the received integers divided by
// scaler (the device sends 1000 and truncates to millimetres). With scaler == 0 the integers are
// used as they are, as the comment of Header.scaler says ("0: disable scaling"). A negative
// scaler returns Errc::Unsupported, because no device sends one and the direction of the
// conversion has not been verified.
Result DecodePointCloud(const ::openapi::aiot_msgs::MqttCompressMsg& msg, PointCloud* out);
// Same, starting from the payload of the lio_pcl topic. DecodeFailed when it is not a
// MqttCompressMsg.
Result DecodePointCloud(const std::string& payload, PointCloud* out);
}  // namespace openapi
}  // namespace lixel
