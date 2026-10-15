"""
Publish panel.

Replicates the OpenAPI-XPanel's publish panel:
- Topic dropdown (editable, prefilled with all publishable request topics)
- Message type dropdown (Request types only) + Load template
- JSON editor
- Publish button

Selecting a Topic auto-syncs the message type; "Load template" generates a
default JSON and injects the header (seq/stamp/device_id/session_id +
openapi_key). Publishing only sends the request; view responses by subscribing
in the "Subscribe" panel on the right.

All user-visible text is wrapped with self.tr() for .ts/.qm localization.
"""
import json
import time

try:
    from src.qt_compat import (
        QComboBox,
        QEvent,
        QFont,
        QGroupBox,
        QHBoxLayout,
        QLabel,
        QPlainTextEdit,
        QPushButton,
        QVBoxLayout,
    )
except ImportError:  # pragma: no cover
    from qt_compat import (  # type: ignore
        QComboBox,
        QEvent,
        QFont,
        QGroupBox,
        QHBoxLayout,
        QLabel,
        QPlainTextEdit,
        QPushButton,
        QVBoxLayout,
    )

try:
    from example.core import (
        interface_for_topic,
        message_for_topic,
        publish_topics,
        request_messages,
    )
except ImportError:  # pragma: no cover
    from core import (  # type: ignore
        interface_for_topic,
        message_for_topic,
        publish_topics,
        request_messages,
    )


class PublishPanel(QGroupBox):
    """Publish panel."""

    def __init__(self, controller, parent=None):
        super().__init__(parent)
        self._controller = controller
        self._build_ui()
        self.retranslate_ui()

    # ---- UI ---------------------------------------------------------------
    def _build_ui(self) -> None:
        layout = QVBoxLayout(self)

        # Topic row
        topic_row = QHBoxLayout()
        topic_row.addWidget(QLabel("Topic:"))
        self._topic_combo = QComboBox()
        self._topic_combo.setEditable(True)
        self._topic_combo.addItems(publish_topics())
        self._topic_combo.currentTextChanged.connect(self._on_topic_changed)
        topic_row.addWidget(self._topic_combo, 1)
        layout.addLayout(topic_row)

        # Message type row (Request types only; Load template supports Request only)
        msg_row = QHBoxLayout()
        self._msg_type_label = QLabel()
        msg_row.addWidget(self._msg_type_label)
        self._msg_combo = QComboBox()
        self._msg_combo.setEditable(True)
        self._request_messages = set(request_messages())
        self._msg_combo.addItems(request_messages())
        msg_row.addWidget(self._msg_combo, 1)
        self._load_btn = QPushButton()
        self._load_btn.clicked.connect(self._load_template)
        msg_row.addWidget(self._load_btn)
        layout.addLayout(msg_row)

        # Interface description + response-topic subscription hint
        self._desc_label = QLabel("")
        self._desc_label.setWordWrap(True)
        self._desc_label.setStyleSheet("color:#555;")
        layout.addWidget(self._desc_label)

        # JSON editor
        self._editor = QPlainTextEdit()
        self._editor.setFont(QFont("Monospace", 10))
        layout.addWidget(self._editor, 1)

        # Status + publish
        btn_row = QHBoxLayout()
        self._status = QLabel("")
        self._status.setWordWrap(True)
        btn_row.addWidget(self._status, 1)
        self._publish_btn = QPushButton()
        self._publish_btn.clicked.connect(self._on_publish)
        btn_row.addWidget(self._publish_btn)
        layout.addLayout(btn_row)

        # Initial sync once
        self._on_topic_changed(self._topic_combo.currentText())

    # ---- Retranslate ------------------------------------------------------
    def changeEvent(self, event):  # noqa: N802
        if event.type() == QEvent.LanguageChange:
            self.retranslate_ui()
        super().changeEvent(event)

    def retranslate_ui(self) -> None:
        self.setTitle(self.tr("Publish messages"))
        self._msg_type_label.setText(self.tr("Message type:"))
        self._load_btn.setText(self.tr("Load template"))
        self._editor.setPlaceholderText(self.tr("Enter message content in JSON..."))
        self._publish_btn.setText(self.tr("Publish"))
        # Refresh the description on language change (some text is composed via tr)
        self._update_description(self._topic_combo.currentText().strip())

    # ---- Sync / template --------------------------------------------------
    def _on_topic_changed(self, topic: str) -> None:
        topic = topic.strip()
        msg_name = message_for_topic(topic)
        if msg_name:
            idx = self._msg_combo.findText(msg_name)
            if idx >= 0:
                self._msg_combo.setCurrentIndex(idx)
            else:
                self._msg_combo.setEditText(msg_name)
        self._update_description(topic)

    def _update_description(self, topic: str) -> None:
        """Update the interface description and the response-topic subscribe hint."""
        itf = interface_for_topic(topic)
        if itf is None:
            self._desc_label.setText("")
            return
        parts = [f"📖 {self.tr(itf.name)}: {self.tr(itf.description)}"]
        if itf.notes:
            parts.append(f"📌 {self.tr(itf.notes)}")
        if itf.response is not None:
            parts.append(
                "💡 " + self.tr("Subscribe to the response in the \"Subscribe\" "
                                "panel on the right: ")
                + itf.response.topic)
        elif itf.report is not None:
            parts.append(
                "💡 " + self.tr("Subscribe to the report in the \"Subscribe\" "
                                "panel on the right: ")
                + itf.report.topic)
        self._desc_label.setText("\n".join(parts))

    def _load_template(self) -> None:
        msg_name = self._msg_combo.currentText().strip()
        if not msg_name:
            return
        # Load template supports Request types only
        if msg_name not in self._request_messages:
            self._set_status(
                self.tr("Load template supports Request-type messages only."), True)
            return
        try:
            default_json = self._controller.default_json(msg_name)
            data = json.loads(default_json) if default_json.strip() != "{}" else {}
        except Exception as e:  # noqa: BLE001
            self._set_status(self.tr("Load template failed: ") + str(e), True)
            return

        # Inject header
        header = dict(data.get("header") or {})
        header.setdefault("seq", 0)
        header["stamp"] = int(time.time() * 1000)
        header.setdefault("device_id", "openapi-example")
        header["session_id"] = f"example-{int(time.time())}"
        key = self._controller.openapi_key()
        if key:
            header["openapi_key"] = key
        data["header"] = header

        self._editor.setPlainText(json.dumps(data, indent=2, ensure_ascii=False))
        self._set_status("", False)

    # ---- Publish ----------------------------------------------------------
    def _on_publish(self) -> None:
        if not self._controller.is_connected():
            self._set_status(self.tr("Device not connected. Please connect first."), True)
            return
        topic = self._topic_combo.currentText().strip()
        msg_name = self._msg_combo.currentText().strip()
        text = self._editor.toPlainText().strip()
        if not topic:
            self._set_status(self.tr("Please enter a Topic"), True)
            return
        if not msg_name:
            self._set_status(self.tr("Please select a message type"), True)
            return
        try:
            payload = json.loads(text) if text else {}
        except json.JSONDecodeError as e:
            self._set_status(self.tr("JSON parse error: ") + str(e), True)
            return
        try:
            self._controller.publish(topic, msg_name, payload)
        except Exception as e:  # noqa: BLE001
            self._set_status(self.tr("Publish failed: ") + str(e), True)
            return
        self._set_status(self.tr("Published: ") + topic, False)

    def _set_status(self, text: str, is_error: bool) -> None:
        color = "#c0392b" if is_error else "#2e7d32"
        self._status.setStyleSheet(f"color:{color};")
        self._status.setText(text)
