#!/usr/bin/env python3
"""
OpenAPI control panel entry point

Run (Linux only; see xpanel/README.md for the setup):
    python xpanel/main.py            # English UI by default
    python xpanel/main.py --lang zh  # switch to Chinese on startup

The UI language can also be switched at runtime via the menu "Settings -> Language".

Localization: source text is written in English and wrapped with tr(); the
translation into Chinese is stored in xpanel/openapi_xpanel/i18n/openapi_zh.ts and compiled to .qm:
    bash xpanel/openapi_xpanel/i18n/build_i18n.sh release
"""
from __future__ import annotations

import os
import sys

os.environ.setdefault("PROTOCOL_BUFFERS_PYTHON_IMPLEMENTATION", "python")

# Directory of this file: assets/ sits next to it. The OpenAPI client library comes
# from the installed package (pip install -e python), so sys.path is not changed.
_ROOT = os.path.dirname(os.path.abspath(__file__))

from PySide6.QtGui import QIcon  # noqa: E402
from PySide6.QtWidgets import QApplication  # noqa: E402

from openapi_xpanel._build_info import APP_VERSION  # noqa: E402
from openapi_xpanel.gui.control_panel import ControlPanel  # noqa: E402


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


def _self_test(window: "ControlPanel") -> int:
    """Hidden ``--self-test``: print one marker line after the main window is built, then exit.

    Used by the packaging check on a clean system. It neither shows the window nor
    starts the event loop, so it also works with QT_QPA_PLATFORM=offscreen. Pending
    events are processed once so that a language chosen with --lang is already applied
    (Qt delivers the language change as a queued event); with ``--lang zh`` the printed
    key_label is Chinese only if the .qm file was found.
    """
    import PySide6

    QApplication.processEvents()
    print(
        "XPANEL_SELF_TEST_OK version=%s qt=%s key_label=%s"
        % (APP_VERSION, PySide6.__version__, window._key_label.text())
    )
    return 0


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
    if "--self-test" in sys.argv[1:]:
        return _self_test(window)
    window.show()
    return app.exec()


if __name__ == "__main__":
    sys.exit(main())
