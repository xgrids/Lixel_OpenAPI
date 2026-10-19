// Example: subscribe to real-time point cloud reports (lio_pcl)
// - Report topic: lixel/application/report/lio_pcl  (MqttCompressMsg)
// The point cloud is compressed and wrapped in a MqttCompressMsg:
//   1. parse the MqttCompressMsg: compression type, compressed_size, compressed_data
//   2. decompress compressed_data (LZ4 here; Zstd is reported as not supported)
//   3. parse the result as a LioPclReport and scale the integer coordinates
// DecodePointCloud does steps 2 and 3. The LZ4 data is a raw LZ4 *block*, not the LZ4 frame
// format; compressed_size is the size before compression, which the block decoder needs.
// The same as python/example/usage/lio_pcl.py.
#include <cstdio>

#include "common.h"
#include "lixel/openapi/pointcloud.h"
#include "openapi/aiot_msgs.pb.h"

namespace pbm = ::openapi::aiot_msgs;
using lixel::openapi::Client;
using lixel::openapi::Result;
using lixel_examples::Args;

int main(int argc, char** argv) {
  Args args("subscribe to real-time point cloud");
  args.AddOption("seconds", "10", "subscription duration");
  args.Parse(argc, argv);
  lixel_examples::InstallInterruptHandler();

  Client client(lixel_examples::ClientOptionsFrom(args));
  if (!lixel_examples::ConnectOrReport(&client, args)) return 1;
  Result sub = client.SubscribeMessage(
      "lixel/application/report/lio_pcl", "MqttCompressMsg",
      [](const std::string&, const google::protobuf::Message& m) {
        const auto& msg = static_cast<const pbm::MqttCompressMsg&>(m);
        char meta[160];
        std::snprintf(meta, sizeof(meta), "compression: %s, compressed_size: %u, data_bytes: %zu",
                      pbm::MqttCompressMsg_CompressionType_Name(msg.compression()).c_str(),
                      static_cast<unsigned>(msg.compressed_size()), msg.compressed_data().size());
        lixel_examples::DumpText("MqttCompressMsg(meta)", meta);
        lixel::openapi::PointCloud cloud;
        Result r = lixel::openapi::DecodePointCloud(msg, &cloud);
        if (!r.ok()) {
          // Say why instead of failing silently.
          std::printf("  [error] failed to decode point cloud: %s: %s\n",
                      lixel::openapi::ErrcName(r.code), r.desc.c_str());
          return;
        }
        std::printf("  decoded point cloud: %zu points (scaler=%lld)\n", cloud.points.size(),
                    static_cast<long long>(cloud.scaler));
        if (!cloud.points.empty()) {
          const auto& p = cloud.points.front();
          std::printf("  first point: x=%.3f y=%.3f z=%.3f rgbi=0x%08X\n", p.x, p.y, p.z,
                      static_cast<unsigned>(p.rgbi));
        }
      });
  if (!sub.ok()) {
    std::fprintf(stderr, "error: cannot subscribe: %s: %s\n", lixel::openapi::ErrcName(sub.code),
                 sub.desc.c_str());
    return 1;
  }
  std::printf("Subscribing to real-time point cloud for %ss ... (device must be scanning)\n",
              lixel_examples::Num(args.Double("seconds")).c_str());
  return lixel_examples::SleepUnlessInterrupted(args.Double("seconds")) ? 0 : 130;
}
