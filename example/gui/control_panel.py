"""
OpenAPI control panel main window.

Top: a menu bar (Settings / About) and the connection bar; below, the "Publish"
panel on the left and the "Subscribe" panel on the right (same layout as the
OpenAPI-XPanel).

All user-visible text is wrapped with self.tr() for .ts/.qm localization. The
language can be switched live from the "Settings" menu (English by default).
"""
import os

try:
    from src.qt_compat import (
        QActionGroup,
        QApplication,
        QComboBox,
        QEvent,
        QHBoxLayout,
        QLabel,
        QLineEdit,
        QMainWindow,
        QMessageBox,
        QPushButton,
        QSpinBox,
        QSplitter,
        QStatusBar,
        Qt,
        QTranslator,
        QVBoxLayout,
        QWidget,
    )
except ImportError:  # pragma: no cover
    from qt_compat import (  # type: ignore
        QActionGroup,
        QApplication,
        QComboBox,
        QEvent,
        QHBoxLayout,
        QLabel,
        QLineEdit,
        QMainWindow,
        QMessageBox,
        QPushButton,
        QSpinBox,
        QSplitter,
        QStatusBar,
        Qt,
        QTranslator,
        QVBoxLayout,
        QWidget,
    )

try:
    from example.core import ClientOptions
    from example.gui.controller import PanelController
    from example.gui.publish_panel import PublishPanel
    from example.gui.subscribe_panel import SubscribePanel
except ImportError:  # pragma: no cover
    from core import ClientOptions  # type: ignore
    from gui.controller import PanelController  # type: ignore
    from gui.publish_panel import PublishPanel  # type: ignore
    from gui.subscribe_panel import SubscribePanel  # type: ignore


APP_VERSION = "V0.0.0"  # fallback only; the real version lives in example/main.py

# Supported languages: code -> (menu display name, .qm file or None for the
# English source language).
LANGUAGES = [
    ("en", "English", None),
    ("zh", "中文", "openapi_zh.qm"),
]
DEFAULT_LANG = "en"


class ControlPanel(QMainWindow):
    """Device OpenAPI control panel."""

    # Default connection parameters
    _DEFAULT_HOST = "192.168.55.1"
    # MQTTS username/password: enter the credentials provided by XGRIDS in the
    # connection bar.
    _MQTTS_USERNAME = ""
    _MQTTS_PASSWORD = ""

    def __init__(self, parent=None):
        super().__init__(parent)
        self._controller = PanelController()
        self._translator = QTranslator()
        self._i18n_dir = os.path.join(
            os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "i18n")
        self._connected = False
        self.resize(920, 760)
        self._build_menu()
        self._build_ui()
        # Default language
        self.set_language(DEFAULT_LANG)

    # ---- Menu -------------------------------------------------------------
    def _build_menu(self) -> None:
        menubar = self.menuBar()

        # Settings menu (language switching)
        self._settings_menu = menubar.addMenu("")
        self._language_menu = self._settings_menu.addMenu("")
        self._lang_group = QActionGroup(self)
        self._lang_group.setExclusive(True)
        self._lang_actions: dict[str, object] = {}
        for code, display, _qm in LANGUAGES:
            act = self._language_menu.addAction(display)
            act.setCheckable(True)
            act.setData(code)
            act.triggered.connect(lambda checked=False, c=code: self.set_language(c))
            self._lang_group.addAction(act)
            self._lang_actions[code] = act

        # About menu
        self._about_menu = menubar.addMenu("")
        self._about_action = self._about_menu.addAction("")
        self._about_action.triggered.connect(self._show_about)

    def _app_version(self) -> str:
        """Read the version stamped into the entry module by the build script."""
        try:
            from example.main import APP_VERSION as _v  # lazy to avoid import cycle
        except Exception:  # noqa: BLE001
            try:
                from main import APP_VERSION as _v  # type: ignore
            except Exception:  # noqa: BLE001
                _v = APP_VERSION
        return _v or APP_VERSION

    def _app_build_time(self) -> str:
        """Read the build timestamp stamped into the entry module, if any."""
        try:
            from example.main import APP_BUILD_TIME as _t  # lazy import
        except Exception:  # noqa: BLE001
            try:
                from main import APP_BUILD_TIME as _t  # type: ignore
            except Exception:  # noqa: BLE001
                _t = ""
        return _t or ""

    def _show_about(self) -> None:
        title = self.tr("About")
        product = self.tr("Lixel OpenAPI Control Panel")
        version_label = self.tr("Version")
        website_label = self.tr("Website")
        copyright_line = self.tr(
            "Copyright \u00a9 XGRIDS. All rights reserved."
        )
        url = "https://www.xgrids.com/"
        version = self._app_version()
        build_time = self._app_build_time()
        build_html = (
            f"<p>{self.tr('Build')}: {build_time}</p>" if build_time else ""
        )
        # Rich text so the website renders as a clickable hyperlink.
        body = (
            f"<h3>{product}</h3>"
            f"<p>{version_label}: {version}</p>"
            f"{build_html}"
            f'<p>{website_label}: <a href="{url}">{url}</a></p>'
            f"<p>{copyright_line}</p>"
        )
        box = QMessageBox(self)
        box.setWindowTitle(title)
        box.setTextFormat(Qt.RichText)
        box.setText(body)
        box.setIconPixmap(self.windowIcon().pixmap(64, 64))
        # Allow the hyperlink to be opened in the default browser.
        box.setTextInteractionFlags(
            Qt.TextBrowserInteraction | Qt.LinksAccessibleByMouse
        )
        box.exec_() if hasattr(box, "exec_") else box.exec()

    # ---- Language switching -----------------------------------------------
    def set_language(self, lang: str) -> None:
        """Switch the UI language and retranslate live."""
        app = QApplication.instance()
        # Remove the old translator first
        app.removeTranslator(self._translator)

        qm_name = next((q for c, _d, q in LANGUAGES if c == lang), None)
        if qm_name:
            qm_path = os.path.join(self._i18n_dir, qm_name)
            if os.path.exists(qm_path) and self._translator.load(qm_path):
                app.installTranslator(self._translator)
        # When lang has no qm (English source), no translator is installed;
        # removing it returns to the source language.

        # Check the current language
        act = self._lang_actions.get(lang)
        if act is not None:
            act.setChecked(True)

    # ---- UI ---------------------------------------------------------------
    def _build_ui(self) -> None:
        central = QWidget()
        self.setCentralWidget(central)
        root = QVBoxLayout(central)

        root.addWidget(self._build_connection_bar())

        # Left: publish panel; right: subscribe panel (draggable splitter)
        splitter = QSplitter(Qt.Horizontal)
        self._publish_panel = PublishPanel(self._controller)
        splitter.addWidget(self._publish_panel)
        self._subscribe_panel = SubscribePanel(self._controller)
        splitter.addWidget(self._subscribe_panel)
        splitter.setSizes([500, 700])
        root.addWidget(splitter, 1)

        self.setStatusBar(QStatusBar())
        self.retranslate_ui()

    def _build_connection_bar(self) -> QWidget:
        bar = QWidget()
        layout = QHBoxLayout(bar)
        layout.setContentsMargins(0, 0, 0, 0)

        self._host_label = QLabel()
        layout.addWidget(self._host_label)
        self._host_edit = QLineEdit(self._DEFAULT_HOST)
        self._host_edit.setMaximumWidth(140)
        layout.addWidget(self._host_edit)

        self._port_label = QLabel()
        layout.addWidget(self._port_label)
        self._port_spin = QSpinBox()
        self._port_spin.setRange(1, 65535)
        self._port_spin.setValue(1883)
        layout.addWidget(self._port_spin)

        self._proto_label = QLabel()
        layout.addWidget(self._proto_label)
        self._proto_combo = QComboBox()
        self._proto_combo.addItems(["MQTT", "MQTTS"])
        self._proto_combo.currentTextChanged.connect(self._on_proto_changed)
        layout.addWidget(self._proto_combo)

        # Username / password: shown only under MQTTS (label + field toggle together)
        self._user_label = QLabel()
        layout.addWidget(self._user_label)
        self._user_edit = QLineEdit()
        self._user_edit.setMaximumWidth(120)
        layout.addWidget(self._user_edit)

        self._pass_label = QLabel()
        layout.addWidget(self._pass_label)
        self._pass_edit = QLineEdit()
        self._pass_edit.setEchoMode(QLineEdit.Password)
        self._pass_edit.setMaximumWidth(120)
        layout.addWidget(self._pass_edit)

        self._key_label = QLabel()
        layout.addWidget(self._key_label)
        self._key_edit = QLineEdit()
        self._key_edit.setMaximumWidth(160)
        self._key_edit.textChanged.connect(self._controller.set_openapi_key)
        layout.addWidget(self._key_edit)

        self._conn_btn = QPushButton()
        self._conn_btn.clicked.connect(self._on_connect_clicked)
        layout.addWidget(self._conn_btn)

        layout.addStretch(1)
        # Initialize protocol-dependent visibility and defaults
        self._apply_protocol(self._proto_combo.currentText())
        return bar

    # ---- Retranslate ------------------------------------------------------
    def changeEvent(self, event):  # noqa: N802
        if event.type() == QEvent.LanguageChange:
            self.retranslate_ui()
        super().changeEvent(event)

    def retranslate_ui(self) -> None:
        self.setWindowTitle(self.tr("Lixel OpenAPI Control Panel"))
        # Menu
        self._settings_menu.setTitle(self.tr("Settings"))
        self._language_menu.setTitle(self.tr("Language"))
        self._about_menu.setTitle(self.tr("About"))
        self._about_action.setText(self.tr("About"))
        # Connection bar
        self._host_label.setText(self.tr("Host"))
        self._port_label.setText(self.tr("Port"))
        self._proto_label.setText(self.tr("Protocol"))
        self._user_label.setText(self.tr("Username"))
        self._pass_label.setText(self.tr("Password"))
        self._key_label.setText(self.tr("OpenAPI Key"))
        self._conn_btn.setText(self.tr("Disconnect") if self._connected else self.tr("Connect"))
        if not self._connected:
            self._set_status(self.tr("Disconnected"))

    # ---- Interaction ------------------------------------------------------
    def _on_proto_changed(self, text: str) -> None:
        self._apply_protocol(text)

    def _apply_protocol(self, text: str) -> None:
        """Adjust port and username/password visibility/defaults by protocol."""
        is_mqtts = text == "MQTTS"
        self._port_spin.setValue(8883 if is_mqtts else 1883)

        self._user_label.setVisible(is_mqtts)
        self._user_edit.setVisible(is_mqtts)
        self._pass_label.setVisible(is_mqtts)
        self._pass_edit.setVisible(is_mqtts)

        if is_mqtts:
            if not self._user_edit.text():
                self._user_edit.setText(self._MQTTS_USERNAME)
            if not self._pass_edit.text():
                self._pass_edit.setText(self._MQTTS_PASSWORD)
        else:
            self._user_edit.clear()
            self._pass_edit.clear()

    def _on_connect_clicked(self) -> None:
        if self._controller.is_connected():
            self._controller.disconnect()
            self._subscribe_panel.on_disconnected()
            self._connected = False
            self._conn_btn.setText(self.tr("Connect"))
            self._set_status(self.tr("Disconnected"))
            return

        options = ClientOptions(
            host=self._host_edit.text().strip(),
            port=self._port_spin.value(),
            use_tls=self._proto_combo.currentText() == "MQTTS",
            username=self._user_edit.text().strip(),
            password=self._pass_edit.text(),
            openapi_key=self._key_edit.text().strip(),
        )
        try:
            self._controller.connect(options)
        except Exception as e:  # noqa: BLE001
            QMessageBox.critical(self, self.tr("Connection failed"), str(e))
            self._set_status(self.tr("Connection failed"))
            return

        self._connected = True
        self._conn_btn.setText(self.tr("Disconnect"))
        self._set_status(self.tr("Connected: ") + f"{options.host}:{options.port}")

    def _set_status(self, text: str) -> None:
        self.statusBar().showMessage(text)

    def closeEvent(self, event):  # noqa: N802
        self._controller.disconnect()
        super().closeEvent(event)
