#!/usr/bin/env python3
import sys, re
from PyQt5 import QtWidgets, QtCore
from pci_ids_parser import load_pci_ids

PROC_PATH = "/proc/gpu_viewer"
REFRESH_MS = 3000

class GPUViewer(QtWidgets.QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("GPU Viewer")
        self.resize(600, 400)
        self.ids = load_pci_ids()

        self.container = QtWidgets.QVBoxLayout(self)
        btn = QtWidgets.QPushButton("Refresh Now")
        btn.clicked.connect(self.load)
        self.container.addWidget(btn)

        self.scroll = QtWidgets.QScrollArea()
        self.scroll_widget = QtWidgets.QWidget()
        self.scroll_layout = QtWidgets.QVBoxLayout(self.scroll_widget)
        self.scroll.setWidget(self.scroll_widget)
        self.scroll.setWidgetResizable(True)
        self.container.addWidget(self.scroll)

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
            self.scroll_layout.addWidget(QtWidgets.QLabel(str(e)))
            return

        for l in lines:
            l = l.strip()
            if l.startswith("Bus:"):
                data = {}
                gpu_list.append(data)
                data["Bus"] = l.split(":", 1)[1].strip()
            else:
                k, v = l.split(":", 1)
                data[k.strip()] = v.strip()

        # clear
        for i in reversed(range(self.scroll_layout.count())):
            self.scroll_layout.itemAt(i).widget().deleteLater()

        # add per GPU widget
        for idx, g in enumerate(gpu_list):
            vid = g.get("Vendor ID", "").split()[1][2:]
            name = self.ids.get(vid, "Unknown Vendor")
            title = f"[{name}] (GPU {idx})"

            box = QtWidgets.QGroupBox(title)
            form = QtWidgets.QFormLayout()
            for key in ["VRAM", "PCIe Link", "Temp", "Power", "Driver"]:
                form.addRow(key + ":", QtWidgets.QLabel(g.get(key, "N/A")))
            box.setLayout(form)
            self.scroll_layout.addWidget(box)

    def show(self):
        super().show()
        self.load()

if __name__ == "__main__":
    app = QtWidgets.QApplication(sys.argv)
    w = GPUViewer()
    w.show()
    sys.exit(app.exec_())
