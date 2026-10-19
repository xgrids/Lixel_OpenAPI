"""
Qt binding compatibility layer.

The GUI is written once and runs against either PySide6 (preferred, used for the
Windows build) or PySide2 (used for the Linux build to stay compatible with
older glibc). This module hides the differences between the two bindings and
re-exports a single, stable set of Qt symbols used across the application.

Import from here everywhere in the app, e.g.:
    from src.qt_compat import QApplication, QWidget, Signal, run_app
"""
from __future__ import annotations

# ---------------------------------------------------------------------------
# Select the Qt binding: prefer PySide6, fall back to PySide2.
# ---------------------------------------------------------------------------
_BINDING = None
try:  # pragma: no cover - environment dependent
    import PySide6  # noqa: F401

    _BINDING = "PySide6"
except Exception:  # noqa: BLE001  # pragma: no cover - environment dependent
    # A broken/partial PySide6 install can raise errors other than ImportError
    # (e.g. ValueError on corrupt files); fall back to PySide2 regardless.
    try:
        import PySide2  # noqa: F401

        _BINDING = "PySide2"
    except Exception as exc:  # noqa: BLE001  # pragma: no cover
        raise ImportError(
            "No supported Qt binding found. Install PySide6 (Windows) or "
            "PySide2 (Linux)."
        ) from exc

QT_BINDING = _BINDING

if QT_BINDING == "PySide6":  # pragma: no cover - environment dependent
    from PySide6.QtCore import (  # noqa: F401
        QEvent,
        Qt,
        QTimer,
        QTranslator,
        Signal,
        Slot,
    )
    from PySide6.QtGui import (  # noqa: F401
        QAction,
        QActionGroup,
        QFont,
        QIcon,
    )
    from PySide6.QtWidgets import (  # noqa: F401
        QApplication,
        QCheckBox,
        QComboBox,
        QDialog,
        QDialogButtonBox,
        QGridLayout,
        QGroupBox,
        QHBoxLayout,
        QLabel,
        QLineEdit,
        QListWidget,
        QMainWindow,
        QMenu,
        QMessageBox,
        QPlainTextEdit,
        QPushButton,
        QSpinBox,
        QSplitter,
        QStatusBar,
        QTextEdit,
        QVBoxLayout,
        QWidget,
    )
else:  # PySide2  # pragma: no cover - environment dependent
    from PySide2.QtCore import (  # noqa: F401
        QEvent,
        Qt,
        QTimer,
        QTranslator,
        Signal,
        Slot,
    )
    from PySide2.QtGui import (  # noqa: F401
        QFont,
        QIcon,
    )
    # In PySide2 QAction / QActionGroup live in QtWidgets, in PySide6 in QtGui.
    from PySide2.QtWidgets import (  # noqa: F401
        QAction,
        QActionGroup,
        QApplication,
        QCheckBox,
        QComboBox,
        QDialog,
        QDialogButtonBox,
        QGridLayout,
        QGroupBox,
        QHBoxLayout,
        QLabel,
        QLineEdit,
        QListWidget,
        QMainWindow,
        QMenu,
        QMessageBox,
        QPlainTextEdit,
        QPushButton,
        QSpinBox,
        QSplitter,
        QStatusBar,
        QTextEdit,
        QVBoxLayout,
        QWidget,
    )


def run_app(app: "QApplication") -> int:
    """Start the Qt event loop, hiding the exec()/exec_() naming difference.

    PySide6 uses ``QApplication.exec`` while PySide2 uses ``QApplication.exec_``.
    Returns the application's exit code.
    """
    runner = getattr(app, "exec", None)
    if runner is None:  # PySide2 fallback
        runner = app.exec_
    return runner()


__all__ = [
    "QT_BINDING",
    # QtCore
    "QEvent",
    "Qt",
    "QTimer",
    "QTranslator",
    "Signal",
    "Slot",
    # QtGui
    "QAction",
    "QActionGroup",
    "QFont",
    "QIcon",
    # QtWidgets
    "QApplication",
    "QCheckBox",
    "QComboBox",
    "QDialog",
    "QDialogButtonBox",
    "QGridLayout",
    "QGroupBox",
    "QHBoxLayout",
    "QLabel",
    "QLineEdit",
    "QListWidget",
    "QMainWindow",
    "QMenu",
    "QMessageBox",
    "QPlainTextEdit",
    "QPushButton",
    "QSpinBox",
    "QSplitter",
    "QStatusBar",
    "QTextEdit",
    "QVBoxLayout",
    "QWidget",
    # helpers
    "run_app",
]
