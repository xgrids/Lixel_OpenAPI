#!/usr/bin/env python3
"""
Example: subscribe to real-time point cloud reports (lio_pcl)

- Report topic : lixel/application/report/lio_pcl  (MqttCompressMsg)

Point clouds are compressed and wrapped in MqttCompressMsg:
    1. Deserialize MqttCompressMsg to obtain the compression type, compressed_size
       and compressed_data
    2. Decompress compressed_data according to compression (Lz4 / Zstd)
    3. Deserialize the decompressed result as LioPclReport to obtain the point array

Note: Lz4 data is a raw LZ4 *block* (not the LZ4 frame format), so it must be
decompressed with lz4.block and the original size. MqttCompressMsg.compressed_size
carries the size *before* compression (the serialized LioPclReport size), which is
exactly the size lz4.block needs.

Decompression depends on lz4 / zstandard; if not installed, only the compression
metadata is printed along with a hint:
    pip install lz4 zstandard
"""
from __future__ import annotations

import time

from _common import build_arg_parser, dump, make_client
from example.core import ProtoCodec


def _decompress(compression, data: bytes, raw_size: int) -> bytes | None:
    """Decompress by compression type.

    raw_size is MqttCompressMsg.compressed_size, i.e. the size before compression.
    Returns None when the required library is not installed; raises ValueError when
    raw_size is missing for Lz4.
    """
    try:
        if compression in ("CompressionLz4", "0", 0):
            import lz4.block
            if not raw_size:
                raise ValueError("compressed_size (size before compression) is required for Lz4")
            return lz4.block.decompress(data, uncompressed_size=int(raw_size))
        else:  # CompressionZstd
            import zstandard
            if raw_size:
                return zstandard.ZstdDecompressor().decompress(data, max_output_size=int(raw_size))
            return zstandard.ZstdDecompressor().decompress(data)
    except ModuleNotFoundError:
        return None


def main() -> int:
    parser = build_arg_parser("subscribe to real-time point cloud")
    parser.add_argument("--seconds", type=float, default=10.0, help="subscription duration")
    args = parser.parse_args()

    codec = ProtoCodec()

    def on_pcl(topic, data):
        # data is already the decoded dict of MqttCompressMsg
        comp = data.get("compression", "CompressionLz4")
        size = data.get("compressed_size")
        raw_b64 = data.get("compressed_data", "")
        dump("MqttCompressMsg(meta)", {"compression": comp, "compressed_size": size,
                                       "b64_len": len(raw_b64)})
        # a protobuf bytes field is a base64 string in the dict
        import base64
        blob = base64.b64decode(raw_b64) if raw_b64 else b""
        try:
            plain = _decompress(comp, blob, size)
            if plain is None:
                print("  [hint] decompression library not installed, skipping point cloud decode: pip install lz4 zstandard")
                return
            pcl = codec.decode("LioPclReport", plain)
        except Exception as exc:  # report the reason instead of failing silently
            print(f"  [error] failed to decode point cloud: {type(exc).__name__}: {exc}")
            return
        pts = pcl.get("points", [])
        print(f"  decoded point cloud: {len(pts)} points (scaler={pcl.get('header', {}).get('scaler')})")

    with make_client(args) as client:
        # Pass MqttCompressMsg so the client automatically decodes the outer layer
        client.subscribe("lixel/application/report/lio_pcl", "MqttCompressMsg", on_pcl)
        print(f"Subscribing to real-time point cloud for {args.seconds}s ... (device must be scanning)")
        time.sleep(args.seconds)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
