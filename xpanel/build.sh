#!/bin/bash
# OpenAPI-XPanel Docker build & packaging script (Linux x86_64) - uses Nuitka + PySide6
# Builds inside an Ubuntu 20.04 image (glibc 2.31) so the result also runs on Ubuntu 22.04 / 24.04.
# The image xpanel-build-focal:latest is built from xpanel/Dockerfile if it does not exist yet.
# Requires Docker and network access (apt, pip).
# Usage: ./xpanel/build.sh [version]   version is optional, defaults to V0.0.0
#        e.g.: ./xpanel/build.sh V2.0.0
# Optional environment variable: XPANEL_APT_MIRROR=<your apt mirror>/ubuntu
#        (apt mirror used only when the image is built, for hosts that cannot reach archive.ubuntu.com)
# Output: xpanel/dist/OpenAPI-XPanel, plus xpanel/dist/LICENSE.txt and xpanel/dist/THIRD-PARTY-NOTICES.txt
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"  # proto/, python/ and LICENSE.txt live here
IMAGE_NAME="xpanel-build-focal:latest"
CONTAINER_NAME="openapi-xpanel-build-$$"
APP_VERSION="${1:-V0.0.0}"

if ! [[ "${APP_VERSION}" =~ ^[A-Za-z0-9._+-]+$ ]]; then
    echo "ERROR: invalid version '${APP_VERSION}' (allowed characters: letters, digits, . _ + -)"
    exit 1
fi

echo "=========================================="
echo " OpenAPI-XPanel ${APP_VERSION} Docker build & packaging (Nuitka + PySide6)"
echo " Image: ${IMAGE_NAME}"
echo "=========================================="

if ! docker image inspect "${IMAGE_NAME}" &>/dev/null; then
    echo "[0/3] Image ${IMAGE_NAME} not found, building it from xpanel/Dockerfile..."
    # The Dockerfile is sent through stdin, so no build context (e.g. xpanel/venv) is uploaded.
    docker build -t "${IMAGE_NAME}" --build-arg "APT_MIRROR=${XPANEL_APT_MIRROR:-}" - < "${SCRIPT_DIR}/Dockerfile"
fi

echo "[1/3] Preparing a staging copy of the sources (the working tree is not modified)..."
rm -rf "${SCRIPT_DIR}/dist"
mkdir -p "${SCRIPT_DIR}/dist"
STAGE_DIR="$(mktemp -d)"
trap 'rm -rf "${STAGE_DIR}"' EXIT
# Generated *_pb2.py files are excluded: they may come from a newer protobuf than the one that
# Python 3.8 can install, so they are regenerated inside the container.
tar -C "${REPO_ROOT}" \
    --exclude=venv --exclude=dist --exclude=build --exclude=__pycache__ \
    --exclude='*.egg-info' --exclude='*_pb2.py' --exclude='*_pb2_grpc.py' \
    -cf - xpanel python proto LICENSE.txt | tar -C "${STAGE_DIR}" -xf -
# Stamp the version and build time into the staging copy only.
BUILD_TIME="$(TZ=Asia/Shanghai date "+%Y/%m/%d %H:%M:%S")"
cat > "${STAGE_DIR}/xpanel/openapi_xpanel/_build_info.py" <<PYEOF
"""Build information stamped by xpanel/build.sh."""
APP_VERSION = "${APP_VERSION}"
APP_BUILD_TIME = "${BUILD_TIME}"
PYEOF

echo "[2/3] Packaging with Nuitka inside the Docker container..."
docker run --rm \
    --name "${CONTAINER_NAME}" \
    -e HOST_UID="$(id -u)" \
    -e HOST_GID="$(id -g)" \
    -v "${STAGE_DIR}:/work" \
    -v "${SCRIPT_DIR}/dist:/out" \
    -w /work \
    "${IMAGE_NAME}" \
    bash -c '
set -e
# The container runs as root; hand every file back to the host user on exit, also after a failure,
# so that the staging directory can be removed by the host.
trap '"'"'chown -R ${HOST_UID}:${HOST_GID} /work /out 2>/dev/null || true'"'"' EXIT
source /opt/xpanel-venv/bin/activate
echo "--- Installing the Python client library (non-editable) and grpcio-tools ---"
pip install --disable-pip-version-check -q "grpcio-tools>=1.60.0"
echo "--- Generating Protobuf files (matching the protobuf runtime in this image) ---"
python /work/python/src/generate_proto.py
pip install --disable-pip-version-check -q /work/python
pip list
echo "--- Running Nuitka packaging ---"
python -m nuitka \
    --standalone \
    --onefile \
    --output-dir=/out \
    --output-filename=OpenAPI-XPanel \
    --enable-plugin=pyside6 \
    --include-package=openapi_xpanel \
    --include-package=example.core \
    --include-package=src.proto_gen \
    --include-module=google.protobuf \
    --include-module=google.protobuf.descriptor \
    --include-module=google.protobuf.message \
    --include-module=google.protobuf.json_format \
    --include-module=google.protobuf.symbol_database \
    --include-module=paho.mqtt.client \
    --include-data-dir=/work/xpanel/assets=assets \
    --include-data-files=/work/xpanel/openapi_xpanel/i18n/openapi_zh.qm=openapi_xpanel/i18n/openapi_zh.qm \
    --include-data-files=/work/xpanel/fonts.conf=fonts.conf \
    --linux-icon=/work/xpanel/assets/icon.png \
    --nofollow-import-to=grpc_tools \
    --nofollow-import-to=tkinter \
    --nofollow-import-to=unittest \
    --nofollow-import-to=pytest \
    --assume-yes-for-downloads \
    /work/xpanel/main.py
echo "--- Cleaning intermediate artifacts ---"
rm -rf /out/main.build /out/main.dist /out/main.onefile-build
'

echo "[3/3] Checking build result..."
EXE_PATH="${SCRIPT_DIR}/dist/OpenAPI-XPanel"
if [ -f "${EXE_PATH}" ]; then
    # Every release package must ship LICENSE.txt and THIRD-PARTY-NOTICES.txt next to the executable.
    LICENSE_SRC="${REPO_ROOT}/LICENSE.txt"
    NOTICES_SRC="${SCRIPT_DIR}/THIRD-PARTY-NOTICES.txt"
    for f in "${LICENSE_SRC}" "${NOTICES_SRC}"; do
        if [ ! -f "${f}" ]; then
            echo "ERROR: required license file not found: ${f}"
            exit 1
        fi
    done
    cp "${LICENSE_SRC}" "${NOTICES_SRC}" "${SCRIPT_DIR}/dist/"
    SIZE=$(du -h "${EXE_PATH}" | cut -f1)
    echo ""
    echo "=========================================="
    echo " Packaging complete!"
    echo " Output: ${EXE_PATH}"
    echo " Size: ${SIZE}"
    echo " Also in dist/: LICENSE.txt, THIRD-PARTY-NOTICES.txt"
    echo "=========================================="
else
    echo "ERROR: Packaging failed, no executable generated"
    exit 1
fi
