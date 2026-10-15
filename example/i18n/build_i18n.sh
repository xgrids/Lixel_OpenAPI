#!/usr/bin/env bash
# Extract / compile GUI translation files.
#
# Usage:
#   bash example/i18n/build_i18n.sh update   # extract translatable strings into .ts
#   bash example/i18n/build_i18n.sh release  # compile .ts into .qm
#   bash example/i18n/build_i18n.sh          # defaults to release
#
# The source language is English; the .ts translates English -> Chinese.
# Requires the system Qt tools lupdate / lrelease (/usr/bin/lupdate, /usr/bin/lrelease).
set -e

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GUI_DIR="$(dirname "$HERE")/gui"
TS="$HERE/openapi_zh.ts"
QM="$HERE/openapi_zh.qm"

cmd="${1:-release}"

case "$cmd" in
    update)
        echo "[i18n] extract strings -> $TS"
        lupdate "$GUI_DIR/control_panel.py" "$GUI_DIR/publish_panel.py" \
                "$GUI_DIR/subscribe_panel.py" -ts "$TS"
        ;;
    release)
        echo "[i18n] compile -> $QM"
        lrelease "$TS" -qm "$QM"
        ;;
    *)
        echo "unknown command: $cmd (available: update | release)" >&2
        exit 1
        ;;
esac
