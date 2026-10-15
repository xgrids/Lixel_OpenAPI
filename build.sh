#!/bin/bash
# OpenAPI-XPanel Docker build & packaging script (Linux) - uses Nuitka + PySide2
# Reuses the existing image ubuntu18-amd64:latest (for compatibility with older systems' glibc)
# Usage: ./build.sh [version]   version is optional, defaults to V0.0.0
#        e.g.: ./build.sh V1.0.0
# Output: dist/OpenAPI-XPanel

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
IMAGE_NAME="ubuntu18-amd64:latest"
CONTAINER_NAME="openapi-mqtt-build-$$"
APP_VERSION="${1:-V0.0.0}"

echo "=========================================="
echo " OpenAPI-XPanel ${APP_VERSION} Docker build & packaging (Nuitka + PySide2)"
echo " Image: ${IMAGE_NAME}"
echo "=========================================="

if ! docker image inspect "${IMAGE_NAME}" &>/dev/null; then
    echo "ERROR: Docker image '${IMAGE_NAME}' does not exist"
    exit 1
fi

echo "[1/3] Cleaning old artifacts..."
rm -rf "${SCRIPT_DIR}/dist" "${SCRIPT_DIR}/build"
rm -rf "${SCRIPT_DIR}/main.build" "${SCRIPT_DIR}/main.dist" "${SCRIPT_DIR}/main.onefile-build"

echo "[2/3] Packaging with Nuitka inside the Docker container..."
docker run --rm \
    --name "${CONTAINER_NAME}" \
    -e HOST_UID="$(id -u)" \
    -e HOST_GID="$(id -g)" \
    -e APP_VERSION="${APP_VERSION}" \
    -v "${SCRIPT_DIR}:/app" \
    -w /app \
    "${IMAGE_NAME}" \
    bash -c '
set -e
source /opt/build_venv/bin/activate 2>/dev/null || true

echo "--- Installing runtime dependencies (PySide2, paho-mqtt, grpcio-tools, nuitka) ---"
pip install --disable-pip-version-check -q \
    "PySide2>=5.15.0" "paho-mqtt>=2.0.0" "grpcio-tools>=1.50.0" nuitka ordered-set

echo "--- Generating Protobuf files (matching the current protobuf runtime version) ---"
python /app/src/generate_proto.py

echo "--- Writing version and build time ---"
BUILD_TIME=$(TZ=Asia/Shanghai date "+%Y/%m/%d %H:%M:%S")
sed -i "s|^APP_VERSION = .*|APP_VERSION = \"${APP_VERSION}\"|" /app/example/main.py
sed -i "s|^APP_BUILD_TIME = .*|APP_BUILD_TIME = \"${BUILD_TIME}\"|" /app/example/main.py

echo "--- Running Nuitka packaging ---"
python -m nuitka \
    --standalone \
    --onefile \
    --output-dir=/app/dist \
    --output-filename=OpenAPI-XPanel \
    --enable-plugin=pyside2 \
    --include-package=example \
    --include-package=example.core \
    --include-package=example.gui \
    --include-package=src \
    --include-package=src.proto_gen \
    --include-package=src.proto_gen.openapi \
    --include-module=google.protobuf \
    --include-module=google.protobuf.descriptor \
    --include-module=google.protobuf.message \
    --include-module=google.protobuf.json_format \
    --include-module=google.protobuf.symbol_database \
    --include-module=paho.mqtt.client \
    --include-data-dir=/app/assets=assets \
    --include-data-files=/app/example/i18n/openapi_zh.qm=example/i18n/openapi_zh.qm \
    --include-data-files=/app/fonts.conf=fonts.conf \
    --linux-icon=/app/assets/icon.png \
    --nofollow-import-to=grpc_tools \
    --nofollow-import-to=tkinter \
    --nofollow-import-to=unittest \
    --nofollow-import-to=pytest \
    --assume-yes-for-downloads \
    /app/example/main.py

echo "--- Cleaning intermediate artifacts ---"
rm -rf /app/dist/main.build /app/dist/main.dist /app/dist/main.onefile-build

echo "--- Restoring version and build time fields ---"
sed -i "s|^APP_VERSION = .*|APP_VERSION = \"V0.0.0\"  # Automatically filled in by the build script|" /app/example/main.py
sed -i "s|^APP_BUILD_TIME = .*|APP_BUILD_TIME = \"\"  # Automatically filled in by the build script|" /app/example/main.py

echo "--- Fixing file permissions ---"
chown -R ${HOST_UID}:${HOST_GID} /app/dist 2>/dev/null || true
'

echo "[3/3] Checking build result..."
EXE_PATH="${SCRIPT_DIR}/dist/OpenAPI-XPanel"
if [ -f "${EXE_PATH}" ]; then
    SIZE=$(du -h "${EXE_PATH}" | cut -f1)
    echo ""
    echo "=========================================="
    echo " Packaging complete!"
    echo " Output: ${EXE_PATH}"
    echo " Size: ${SIZE}"
    echo "=========================================="
else
    echo "ERROR: Packaging failed, no executable generated"
    exit 1
fi
