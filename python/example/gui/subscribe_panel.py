"""
Subscribe panel.

Replicates the OpenAPI-XPanel's subscribe panel:
- Topic dropdown (editable) + Subscribe / Subscribe all
- Wildcard subscription
- Current subscription list + Unsub selected / Unsub all
- Received messages (summary list, auto scroll) + View detail / Clear
- Double-click a message to view the full decoded content

Messages arrive on a background thread and are marshalled to the UI thread via a
Qt signal. All user-visible text is wrapped with self.tr() for .ts/.qm
localization.
"""
import json
from datetime import datetime

try:
    from src.qt_compat import (
        QCheckBox,
        QComboBox,
        QEvent,
        QDialog,
        QDialogButtonBox,
        QFont,
        QGroupBox,
        QHBoxLayout,
        QLabel,
        QLineEdit,
        QListWidget,
        QPlainTextEdit,
        QPushButton,
        QVBoxLayout,
        QWidget,
        Signal,
    )
except ImportError:  # pragma: no cover
    from qt_compat import (  # type: ignore
        QCheckBox,
        QComboBox,
        QEvent,
        QDialog,
        QDialogButtonBox,
        QFont,
        QGroupBox,
        QHBoxLayout,
        QLabel,
        QLineEdit,
        QListWidget,
        QPlainTextEdit,
        QPushButton,
        QVBoxLayout,
        QWidget,
        Signal,
    )

try:
    from example.core import message_for_topic, subscribe_topics
except ImportError:  # pragma: no cover
    from core import message_for_topic, subscribe_topics  # type: ignore


_JSON_TOPICS = {"lixel/application/report/heartbeat"}


class MessageDetailDialog(QDialog):
    """Message detail dialog."""

    def __init__(self, title: str, content: str, parent=None):
        super().__init__(parent)
        self.setWindowTitle(title)
        self.setMinimumSize(600, 400)
        layout = QVBoxLayout(self)
        editor = QPlainTextEdit()
        editor.setFont(QFont("Monospace", 10))
        editor.setReadOnly(True)
        editor.setPlainText(content)
        layout.addWidget(editor)
        btn_box = QDialogButtonBox(QDialogButtonBox.Close)
        btn_box.rejected.connect(self.close)
        layout.addWidget(btn_box)


class SubscribePanel(QGroupBox):
    """Subscribe panel."""

    MAX_LOG_LINES = 2000
    MAX_BYTES_DISPLAY = 256

    # background thread -> UI thread: topic, payload(bytes)
    message_arrived = Signal(str, bytes)

    def __init__(self, controller, parent=None):
        super().__init__(parent)
        self._controller = controller
        self._received: list[tuple[str, str, str]] = []  # (time, topic, detail)
        self._build_ui()
        self.retranslate_ui()
        self.message_arrived.connect(self._on_message_ui)

    # ---- UI ---------------------------------------------------------------
    def _build_ui(self) -> None:
        layout = QVBoxLayout(self)

        # Topic subscribe row
        sub_row = QHBoxLayout()
        sub_row.addWidget(QLabel("Topic:"))
        self._topic_combo = QComboBox()
        self._topic_combo.setEditable(True)
        self._topic_combo.addItems(subscribe_topics())
        sub_row.addWidget(self._topic_combo, 1)
        self._sub_btn = QPushButton()
        self._sub_btn.clicked.connect(self._on_subscribe)
        sub_row.addWidget(self._sub_btn)
        self._sub_all_btn = QPushButton()
        self._sub_all_btn.clicked.connect(self._on_subscribe_all)
        sub_row.addWidget(self._sub_all_btn)
        layout.addLayout(sub_row)

        # Wildcard subscribe row
        wc_row = QHBoxLayout()
        self._wildcard_label = QLabel()
        wc_row.addWidget(self._wildcard_label)
        self._wildcard_edit = QLineEdit("lixel/#")
        wc_row.addWidget(self._wildcard_edit, 1)
        self._wc_btn = QPushButton()
        self._wc_btn.clicked.connect(self._on_subscribe_wildcard)
        wc_row.addWidget(self._wc_btn)
        layout.addLayout(wc_row)

        # Current subscription list
        list_header = QHBoxLayout()
        self._cur_sub_label = QLabel()
        list_header.addWidget(self._cur_sub_label)
        list_header.addStretch()
        self._unsub_sel_btn = QPushButton()
        self._unsub_sel_btn.clicked.connect(self._on_unsub_selected)
        list_header.addWidget(self._unsub_sel_btn)
        self._unsub_all_btn = QPushButton()
        self._unsub_all_btn.clicked.connect(self._on_unsub_all)
        list_header.addWidget(self._unsub_all_btn)
        layout.addLayout(list_header)

        self._sub_list = QListWidget()
        self._sub_list.setMaximumHeight(80)
        self._sub_list.setSelectionMode(QListWidget.ExtendedSelection)
        layout.addWidget(self._sub_list)

        # Received messages
        opt_row = QHBoxLayout()
        self._recv_label = QLabel()
        opt_row.addWidget(self._recv_label)
        self._auto_scroll = QCheckBox()
        self._auto_scroll.setChecked(True)
        opt_row.addWidget(self._auto_scroll)
        opt_row.addStretch()
        self._detail_btn = QPushButton()
        self._detail_btn.clicked.connect(self._show_detail)
        opt_row.addWidget(self._detail_btn)
        self._clear_btn = QPushButton()
        self._clear_btn.clicked.connect(self._clear_messages)
        opt_row.addWidget(self._clear_btn)
        layout.addLayout(opt_row)

        self._msg_list = QListWidget()
        self._msg_list.setFont(QFont("Monospace", 9))
        self._msg_list.itemDoubleClicked.connect(lambda _i: self._show_detail())
        layout.addWidget(self._msg_list, 1)

    # ---- Retranslate ------------------------------------------------------
    def changeEvent(self, event):  # noqa: N802
        if event.type() == QEvent.LanguageChange:
            self.retranslate_ui()
        super().changeEvent(event)

    def retranslate_ui(self) -> None:
        self.setTitle(self.tr("Subscribe messages"))
        self._sub_btn.setText(self.tr("Subscribe"))
        self._sub_all_btn.setText(self.tr("Subscribe all"))
        self._wildcard_label.setText(self.tr("Wildcard:"))
        self._wc_btn.setText(self.tr("Subscribe"))
        self._cur_sub_label.setText(self.tr("Current subscriptions:"))
        self._unsub_sel_btn.setText(self.tr("Unsub selected"))
        self._unsub_all_btn.setText(self.tr("Unsub all"))
        self._recv_label.setText(self.tr("Received messages:"))
        self._auto_scroll.setText(self.tr("Auto scroll"))
        self._detail_btn.setText(self.tr("View detail"))
        self._clear_btn.setText(self.tr("Clear"))

    # ---- Subscribe operations ---------------------------------------------
    def _guard_connected(self) -> bool:
        if not self._controller.is_connected():
            self._append_summary_text(self.tr("Device not connected. Please connect first."))
            return False
        return True

    def _do_subscribe(self, topic: str) -> None:
        # Always use raw subscription (wildcard-aware); the panel decodes itself
        self._controller.subscribe_raw(topic, self._on_raw_message)
        self._add_to_sub_list(topic)

    def _on_subscribe(self) -> None:
        if not self._guard_connected():
            return
        topic = self._topic_combo.currentText().strip()
        if topic:
            self._do_subscribe(topic)

    def _on_subscribe_all(self) -> None:
        if not self._guard_connected():
            return
        for topic in subscribe_topics():
            self._do_subscribe(topic)

    def _on_subscribe_wildcard(self) -> None:
        if not self._guard_connected():
            return
        topic = self._wildcard_edit.text().strip()
        if topic:
            self._do_subscribe(topic)

    def _on_unsub_selected(self) -> None:
        for item in self._sub_list.selectedItems():
            self._controller.unsubscribe(item.text())
            self._sub_list.takeItem(self._sub_list.row(item))

    def _on_unsub_all(self) -> None:
        for i in range(self._sub_list.count()):
            item = self._sub_list.item(i)
            if item:
                self._controller.unsubscribe(item.text())
        self._sub_list.clear()

    def _add_to_sub_list(self, topic: str) -> None:
        for i in range(self._sub_list.count()):
            if self._sub_list.item(i).text() == topic:
                return
        self._sub_list.addItem(topic)

    # ---- Message receiving ------------------------------------------------
    def _on_raw_message(self, topic: str, payload: bytes) -> None:
        """Background-thread callback: forward to the UI thread."""
        self.message_arrived.emit(topic, bytes(payload))

    def _on_message_ui(self, topic: str, payload: bytes) -> None:
        timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        detail = self._build_detail(topic, payload)

        self._received.append((timestamp, topic, detail))
        if len(self._received) > self.MAX_LOG_LINES:
            self._received = self._received[-self.MAX_LOG_LINES:]

        self._msg_list.addItem(f"[{timestamp}] {topic} ({len(payload)}B)")
        while self._msg_list.count() > self.MAX_LOG_LINES:
            self._msg_list.takeItem(0)

        if self._auto_scroll.isChecked():
            self._msg_list.scrollToBottom()

    def _build_detail(self, topic: str, payload: bytes) -> str:
        msg_name = message_for_topic(topic)
        if msg_name:
            try:
                data = self._controller.decode(msg_name, payload)
                self._truncate_recursive(data)
                body = json.dumps(data, indent=2, ensure_ascii=False)
                return f"[{msg_name}]\n{body}"
            except Exception as e:  # noqa: BLE001
                return f"[{self.tr('Parse error')}: {e}]\nHEX: {payload[:64].hex()}..."
        # Unknown type or JSON payload
        try:
            text = payload.decode("utf-8")
            return text
        except UnicodeDecodeError:
            return (f"[{self.tr('Binary')}] {len(payload)} bytes\n"
                    f"HEX: {payload[:64].hex()}...")

    def _truncate_recursive(self, obj) -> None:
        if isinstance(obj, dict):
            for key, value in obj.items():
                if isinstance(value, str) and len(value) > self.MAX_BYTES_DISPLAY:
                    obj[key] = value[:32] + self.tr("...<omitted, {n} chars total>").format(
                        n=len(value))
                elif isinstance(value, (dict, list)):
                    self._truncate_recursive(value)
        elif isinstance(obj, list):
            for item in obj:
                if isinstance(item, (dict, list)):
                    self._truncate_recursive(item)

    # ---- Detail / clear ---------------------------------------------------
    def _show_detail(self) -> None:
        row = self._msg_list.currentRow()
        if row < 0 or row >= len(self._received):
            return
        ts, topic, detail = self._received[row]
        dlg = MessageDetailDialog(f"[{ts}] {topic}", detail, self)
        if hasattr(dlg, "exec"):
            dlg.exec()
        else:  # PySide2
            dlg.exec_()

    def _clear_messages(self) -> None:
        self._msg_list.clear()
        self._received.clear()

    def _append_summary_text(self, text: str) -> None:
        self._msg_list.addItem(text)

    # ---- Called by the main window ----------------------------------------
    def on_disconnected(self) -> None:
        """Clear the subscription list on disconnect."""
        self._sub_list.clear()
