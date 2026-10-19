#include "lixel/openapi/pointcloud.h"

#include <lz4.h>

#include <climits>

namespace lixel {
namespace openapi {

namespace pbm = ::openapi::aiot_msgs;

Result DecompressPointCloud(const pbm::MqttCompressMsg& msg, std::string* serialized) {
  if (serialized == nullptr) return Result::Make(Errc::InvalidArgument, "output is null");
  const std::uint32_t raw_size = msg.compressed_size();
  if (raw_size == 0) {
    return Result::Make(Errc::InvalidArgument, "compressed_size (size before compression) is 0");
  }
  if (raw_size > kMaxPointCloudBytes) {
    return Result::Make(Errc::InvalidArgument, "compressed_size " + std::to_string(raw_size) +
                                                   " is larger than " +
                                                   std::to_string(kMaxPointCloudBytes));
  }
  if (msg.compression() != pbm::MqttCompressMsg::CompressionLz4) {
    return Result::Make(Errc::Unsupported, "only LZ4 compression is implemented");
  }
  const std::string& data = msg.compressed_data();
  if (data.empty() || data.size() > static_cast<std::size_t>(INT_MAX)) {
    return Result::Make(Errc::DecodeFailed, "compressed_data is empty or too large");
  }
  std::string plain(raw_size, '\0');
  // LZ4_decompress_safe never writes beyond raw_size bytes, whatever the input contains.
  const int n = LZ4_decompress_safe(data.data(), &plain[0], static_cast<int>(data.size()),
                                    static_cast<int>(raw_size));
  if (n < 0) return Result::Make(Errc::DecodeFailed, "the data is not a valid LZ4 block");
  if (static_cast<std::uint32_t>(n) != raw_size) {
    return Result::Make(Errc::DecodeFailed, "the data decompressed to " + std::to_string(n) +
                                                " bytes, compressed_size says " +
                                                std::to_string(raw_size));
  }
  serialized->swap(plain);
  return Result();
}

Result DecodePointCloud(const pbm::MqttCompressMsg& msg, PointCloud* out) {
  if (out == nullptr) return Result::Make(Errc::InvalidArgument, "output is null");
  std::string plain;
  Result r = DecompressPointCloud(msg, &plain);
  if (!r.ok()) return r;
  pbm::LioPclReport report;
  if (!report.ParseFromString(plain)) {
    return Result::Make(Errc::DecodeFailed, "the decompressed data is not a LioPclReport");
  }
  const std::int64_t scaler = report.header().scaler();
  if (scaler < 0) {
    return Result::Make(Errc::Unsupported, "a negative header.scaler (" + std::to_string(scaler) +
                                               ") is not supported");
  }
  const double divisor = scaler == 0 ? 1.0 : static_cast<double>(scaler);
  PointCloud cloud;
  cloud.stamp = report.header().stamp();
  cloud.scaler = scaler;
  cloud.points.reserve(static_cast<std::size_t>(report.points_size()));
  for (const auto& p : report.points()) {
    PointCloud::Point q;
    q.x = static_cast<double>(p.x()) / divisor;
    q.y = static_cast<double>(p.y()) / divisor;
    q.z = static_cast<double>(p.z()) / divisor;
    q.rgbi = p.rgbi();
    cloud.points.push_back(q);
  }
  *out = std::move(cloud);
  return Result();
}

Result DecodePointCloud(const std::string& payload, PointCloud* out) {
  pbm::MqttCompressMsg msg;
  if (!msg.ParseFromString(payload)) {
    return Result::Make(Errc::DecodeFailed, "the payload is not a MqttCompressMsg");
  }
  return DecodePointCloud(msg, out);
}

}  // namespace openapi
}  // namespace lixel
