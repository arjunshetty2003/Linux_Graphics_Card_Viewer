🖥️ GPU Viewer – Project Setup & Execution Summary

This document outlines the complete setup, build, and run process for the GPU Viewer project – a Linux Kernel Module (LKM) and PyQt5-based GUI that visualizes GPU info (VRAM, Temperature, etc.).
📁 Project Structure

gpu_viewer_project/
├── gpU-VIewer/
│   ├── gpu_viewer.c              # Kernel module
│   ├── Makefile                  # For building the .ko
│   ├── gpu_viewer_gui.py         # PyQt5 GUI
│   ├── pci_ids_parser.py         # Vendor ID mapping helper
│   ├── debian/                   # Debian packaging folder
│   └── ...

⚙️ 1. Compile the Kernel Module

cd ~/gpu_viewer_project/gpU-VIewer
make

Expected output:

LD [M]  gpu_viewer.ko

🧠 2. Load the Kernel Module

sudo insmod gpu_viewer.ko

Check dmesg for logs:

sudo dmesg | grep gpu_viewer

You should see:

gpu_viewer: module verification failed: signature and/or required key missing - tainting kernel
gpu_viewer module loaded

Verify proc entry:

cat /proc/gpu_viewer

❌ (Optional) Unload the Kernel Module

sudo rmmod gpu_viewer

🧪 3. Test GUI Before Packaging

If pci_ids_parser.py and gpu_viewer_gui.py are in the same folder:

python3 gpu_viewer_gui.py

If running from /usr/bin/gpu-viewer after installation:

python3 /usr/bin/gpu-viewer/gpu_viewer_gui.py

📦 4. Debian Package Creation (.deb)
A. Ensure debian/ folder exists with:

    control

    changelog

    rules

    install

    compat

    source/format

B. Build the package:

dpkg-buildpackage -us -uc

If successful, it produces:

../gpu-viewer_1.0-1_all.deb

📥 5. Install the .deb Package

sudo dpkg -i ../gpu-viewer_1.0-1_all.deb

✅ 6. Verify Installation

Check that package is installed:

dpkg -l | grep gpu-viewer

List installed files:

dpkg -L gpu-viewer

▶️ 7. Run the GUI

If installed into /usr/bin/gpu-viewer:

python3 /usr/bin/gpu-viewer/gpu_viewer_gui.py

Or create an alias or symlink:

sudo ln -s /usr/bin/gpu-viewer/gpu_viewer_gui.py /usr/local/bin/gpu-viewer
gpu-viewer

🧹 8. Cleanup (Optional)













# GPU Viewer Project

## Build kernel module:
sudo apt install build-essential linux-headers-$(uname -r)
make
sudo insmod gpu_viewer.ko

## Run GUI:
sudo apt install python3-pyqt5
./gpu_viewer_gui.py

## Build .deb:
sudo apt install debhelper
dpkg-buildpackage -us -uc
sudo dpkg -i ../gpu-viewer_*.deb

Unload module and remove:

sudo rmmod gpu_viewer
sudo rm -rf /usr/bin/gpu-viewer
sudo dpkg -r gpu-viewer
