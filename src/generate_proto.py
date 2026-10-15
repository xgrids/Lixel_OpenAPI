#!/usr/bin/env python3
"""
Generate Python protobuf modules from the .proto definitions.

Compiles ``proto/openapi/*.proto`` into ``src/proto_gen/openapi/*_pb2.py`` so that
application code can import them via the stable package path
``src.proto_gen.openapi``.

Run from anywhere:
    python src/generate_proto.py

Requires ``grpcio-tools`` (provides ``grpc_tools.protoc``) in the current
environment. The protobuf runtime version used at build time should match the
one that ships with the packaged application to avoid descriptor mismatches.
"""
from __future__ import annotations

import os
import sys

# Repo layout:
#   <root>/proto/openapi/aiot_msgs.proto   (source)
#   <root>/src/proto_gen/openapi/          (generated output)
_THIS = os.path.abspath(__file__)
_SRC_DIR = os.path.dirname(_THIS)
_ROOT = os.path.dirname(_SRC_DIR)

PROTO_ROOT = os.path.join(_ROOT, "proto")
OUT_ROOT = os.path.join(_SRC_DIR, "proto_gen")
OUT_PKG_DIR = os.path.join(OUT_ROOT, "openapi")


def _find_protos() -> list[str]:
    src_dir = os.path.join(PROTO_ROOT, "openapi")
    protos = [
        os.path.join(src_dir, name)
        for name in sorted(os.listdir(src_dir))
        if name.endswith(".proto")
    ]
    if not protos:
        raise SystemExit(f"No .proto files found under {src_dir}")
    return protos


def _ensure_package(path: str) -> None:
    os.makedirs(path, exist_ok=True)
    init_file = os.path.join(path, "__init__.py")
    if not os.path.exists(init_file):
        with open(init_file, "w", encoding="utf-8") as fh:
            fh.write('"""Generated protobuf code. Do not edit by hand."""\n')


def main() -> int:
    try:
        from grpc_tools import protoc
    except ImportError as exc:
        raise SystemExit(
            "grpc_tools.protoc not available. Install it with: "
            "pip install grpcio-tools"
        ) from exc

    protos = _find_protos()
    _ensure_package(OUT_ROOT)
    _ensure_package(OUT_PKG_DIR)

    # Using PROTO_ROOT as the include path means "openapi/aiot_msgs.proto" is the
    # proto's logical name, so the generated file lands at
    # <out>/openapi/aiot_msgs_pb2.py, matching the src.proto_gen.openapi import.
    #
    # The input files are passed as absolute paths on purpose: protoc resolves
    # input files relative to the *current working directory*, not to
    # --proto_path, so a relative name fails with "Could not make proto path
    # relative" whenever this script is run from outside PROTO_ROOT. Absolute
    # paths still map onto --proto_path, keeping the generated layout unchanged.
    args = [
        "protoc",
        f"--proto_path={PROTO_ROOT}",
        f"--python_out={OUT_ROOT}",
    ]
    args.extend(protos)

    print(f"[proto] source : {PROTO_ROOT}")
    print(f"[proto] output : {OUT_ROOT}")
    for p in protos:
        print(f"[proto] compile: {os.path.relpath(p, PROTO_ROOT)}")

    # grpc_tools.protoc.main resolves its own bundled well-known types; run it
    # from PROTO_ROOT so relative proto names resolve consistently.
    rc = protoc.main(args)
    if rc != 0:
        raise SystemExit(f"protoc failed with exit code {rc}")

    print("[proto] done.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
