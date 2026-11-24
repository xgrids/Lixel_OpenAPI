#!/bin/bash
# Create the XPanel virtual environment (Linux) and install dependencies:
#   PySide6==6.6.3.1 (xpanel/requirements.txt) and the Python client library (pip install -e python).
# Usage: ./xpanel/build-venv.sh
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
VENV_DIR="${SCRIPT_DIR}/venv"

echo "=========================================="
echo " Create XPanel virtual environment (venv)"
echo "=========================================="

PYTHON=$(command -v python3 || true)
if [ -z "$PYTHON" ]; then
    echo "ERROR: python3 not found"
    exit 1
fi
echo "Python: $PYTHON ($($PYTHON --version))"

if [ -d "$VENV_DIR" ]; then
    echo "Virtual environment already exists: $VENV_DIR"
    read -p "Delete and rebuild? [y/N] " confirm
    if [ "$confirm" = "y" ] || [ "$confirm" = "Y" ]; then
        rm -rf "$VENV_DIR"
    else
        echo "Skip creation, only update dependencies..."
    fi
fi

if [ ! -d "$VENV_DIR" ]; then
    echo "Creating virtual environment..."
    $PYTHON -m venv "$VENV_DIR"
fi

echo "Installing dependencies..."
"${VENV_DIR}/bin/pip" install --upgrade pip -q
"${VENV_DIR}/bin/pip" install -r "${SCRIPT_DIR}/requirements.txt"
"${VENV_DIR}/bin/pip" install -e "${REPO_ROOT}/python[dev]"

echo "Generating Protobuf files..."
"${VENV_DIR}/bin/python" "${REPO_ROOT}/python/src/generate_proto.py"

echo ""
echo "=========================================="
echo " Done!"
echo " Activate: source \"${VENV_DIR}/bin/activate\""
echo " Run: python \"${SCRIPT_DIR}/main.py\""
echo " Note: the Qt xcb plugin needs libxcb-cursor0 and other system libraries, see xpanel/README.md"
echo "=========================================="
