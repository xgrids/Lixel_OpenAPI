#!/usr/bin/env python3
"""
OpenAPI control panel entry point

Run:
    python example/main.py            # English UI by default
    python example/main.py --lang zh  # switch to Chinese on startup

The UI language can also be switched at runtime via the menu "Settings -> Language".

Localization: source text is written in English and wrapped with tr(); the
translation into Chinese is stored in example/i18n/openapi_zh.ts and compiled to .qm:
    bash example/i18n/build_i18n.sh release
"""
from __future__ import annotations

import os
import sys

os.environ.setdefault("PROTOCOL_BUFFERS_PYTHON_IMPLEMENTATION", "python")

# Add the project root to the search path, unifying the example.xxx / src.xxx package paths
_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)

from src.qt_compat import QApplication, QIcon, run_app  # noqa: E402
from example.gui.control_panel import ControlPanel  # noqa: E402

# Filled in automatically by the build script (build.sh / build.bat).
APP_VERSION = "V0.0.0"  # Automatically filled in by the build script
APP_BUILD_TIME = ""  # Automatically filled in by the build script


def _asset_dir() -> str:
    """Locate the bundled assets directory.

    Works both when running from source (``<root>/assets``) and when packaged
    with Nuitka --onefile, where data dirs are unpacked next to the executable.
    """
    candidates = [
        os.path.join(_ROOT, "assets"),  # running from source
        os.path.join(os.path.dirname(os.path.abspath(sys.argv[0])), "assets"),
        os.path.join(getattr(sys, "_MEIPASS", ""), "assets"),  # generic bundlers
    ]
    for path in candidates:
        if path and os.path.isdir(path):
            return path
    return os.path.join(_ROOT, "assets")


def _app_icon() -> "QIcon":
    """Return the application icon, preferring .ico then .png."""
    assets = _asset_dir()
    for name in ("icon.ico", "icon.png"):
        path = os.path.join(assets, name)
        if os.path.isfile(path):
            return QIcon(path)
    return QIcon()


def _parse_lang(argv) -> str | None:
    """Parse the --lang argument; returns None if unspecified (uses default English)"""
    for i, arg in enumerate(argv):
        if arg == "--lang" and i + 1 < len(argv):
            return argv[i + 1]
        if arg.startswith("--lang="):
            return arg.split("=", 1)[1]
    return None


def main() -> int:
    app = QApplication(sys.argv)
    app.setApplicationName("Lixel OpenAPI Control Panel")
    app.setApplicationVersion(APP_VERSION.lstrip("Vv"))
    app.setWindowIcon(_app_icon())

    window = ControlPanel()
    window.setWindowIcon(_app_icon())
    lang = _parse_lang(sys.argv)
    if lang:
        window.set_language(lang)
    window.show()
    return run_app(app)


if __name__ == "__main__":
    sys.exit(main())
