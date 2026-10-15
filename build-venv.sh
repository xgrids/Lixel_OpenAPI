#!/bin/bash
# Create a Linux virtual environment and install dependencies (GUI uses PySide2 for compatibility with older systems)
# Usage: ./build-venv.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
VENV_DIR="${SCRIPT_DIR}/venv"

echo "=========================================="
echo " Create Linux virtual environment (venv)"
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
"${VENV_DIR}/bin/pip" install -r "${SCRIPT_DIR}/requirements-linux.txt"

echo "Generating Protobuf files..."
"${VENV_DIR}/bin/python" "${SCRIPT_DIR}/src/generate_proto.py"

echo ""
echo "=========================================="
echo " Done!"
echo " Activate: source venv/bin/activate"
echo " Run: python example/main.py"
echo "=========================================="
