#!/usr/bin/env python3
import sys, re
from PyQt5 import QtWidgets, QtCore, QtGui
from pci_ids_parser import load_pci_ids

PROC_PATH = "/proc/gpu_viewer"
REFRESH_MS = 3000

class GPUViewer(QtWidgets.QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("GPU Viewer")
        self.resize(700, 500)
        self.ids = load_pci_ids()

        layout = QtWidgets.QVBoxLayout(self)

        title = QtWidgets.QLabel("GPU Viewer")
        title.setFont(QtGui.QFont("Arial", 18, QtGui.QFont.Bold))
        layout.addWidget(title)

        refresh_btn = QtWidgets.QPushButton("🔁 Refresh")
        refresh_btn.clicked.connect(self.load)
        layout.addWidget(refresh_btn)

        self.scroll = QtWidgets.QScrollArea()
        self.scroll.setWidgetResizable(True)
        self.scroll_widget = QtWidgets.QWidget()
        self.scroll_layout = QtWidgets.QVBoxLayout(self.scroll_widget)
        self.scroll.setWidget(self.scroll_widget)
        layout.addWidget(self.scroll)

        self.timer = QtCore.QTimer(self)
        self.timer.timeout.connect(self.load)
        self.timer.start(REFRESH_MS)

        self.load()

    def load(self):
        data = {}
        gpu_list = []
        try:
            with open(PROC_PATH) as f:
                lines = f.readlines()
        except Exception as e:
            self.scroll_layout.addWidget(QtWidgets.QLabel(f"Error: {e}"))
            return

        for l in lines:
            l = l.strip()
            if not l:
                continue
            if l.startswith("Bus:"):
                data = {}
                gpu_list.append(data)
                data["Bus"] = l.split(":", 1)[1].strip()
            else:
                if ':' in l:
                    k, v = l.split(":", 1)
                    data[k.strip()] = v.strip()

        # Clear previous GPU blocks
        for i in reversed(range(self.scroll_layout.count())):
            item = self.scroll_layout.itemAt(i)
            widget = item.widget()
            if widget:
                widget.deleteLater()

        for idx, g in enumerate(gpu_list):
            vendor_line = g.get("Vendor ID", "")
            try:
                vendor_id_hex = vendor_line.split()[0]
                vid = vendor_id_hex.replace("0x", "").lower()
            except IndexError:
                vid = "0000"

            vendor_name = self.ids.get(vid, "Unknown Vendor")
            gpu_name = f"[{vendor_name}] GPU #{idx + 1}"

            box = QtWidgets.QGroupBox(gpu_name)
            box_layout = QtWidgets.QVBoxLayout()

            # VRAM Highlighted
            vram = g.get("VRAM", "N/A")
            temp = g.get("Temp", "N/A")

            vram_label = QtWidgets.QLabel(f"VRAM: {vram}")
            vram_label.setFont(QtGui.QFont("Arial", 14, QtGui.QFont.Bold))
            box_layout.addWidget(vram_label)

            temp_label = QtWidgets.QLabel(f"Temperature: {temp}")
            temp_label.setFont(QtGui.QFont("Arial", 14, QtGui.QFont.Bold))
            box_layout.addWidget(temp_label)

            # Additional info in form layout
            form = QtWidgets.QFormLayout()
            for key in ["PCIe Link", "Power", "Driver", "Device ID", "Bus"]:
                val = g.get(key, "N/A")
                form.addRow(key + ":", QtWidgets.QLabel(val))
            box_layout.addLayout(form)

            box.setLayout(box_layout)
            self.scroll_layout.addWidget(box)

        self.scroll_layout.addStretch()

if __name__ == "__main__":
    app = QtWidgets.QApplication(sys.argv)
    w = GPUViewer()
    w.show()
    sys.exit(app.exec_())

