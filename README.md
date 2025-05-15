# Linux Graphics Card Viewer

This project is a simple graphics card information viewer for Linux systems. It consists of two main components:
1.  A **Loadable Kernel Module (LKM)**: This module runs in kernel space, accesses PCI configuration data to find information about the graphics card(s), and provides this information to a user-space application via a Netlink socket.
2.  A **GTK-based GUI Application**: This user-space application communicates with the kernel module to request GPU information and displays it in a tabular, real-time updating window.

## Project Structure

It's assumed your project is structured as follows:


Linux_Graphics_Card_Viewer/  (Your main project root)
├── gpu_viewer_v3/
│   ├── gui/
│   │   ├── gpu_viewer_gui.c   # GUI application source
│   │   └── (Makefile for GUI - optional, can compile directly)
│   ├── include/
│   │   └── gpu_proto.h        # Shared header for kernel & GUI
│   └── kernel/
│       ├── gpu_viewer_lkm.c   # Kernel module source
│       └── Makefile           # Makefile for the kernel module
└── README.md                  # This file


## Prerequisites

Before you begin, ensure you have the following installed on your Linux system:

1.  **GCC (GNU Compiler Collection)**: For compiling C code.
    ```bash
    sudo apt update
    sudo apt install build-essential
    ```
2.  **GTK Development Libraries (GTK3)**: For compiling the GUI application.
    ```bash
    sudo apt install libgtk-3-dev
    ```
3.  **Linux Kernel Headers**: For compiling the kernel module. These must match your currently running kernel version.
    ```bash
    sudo apt install linux-headers-$(uname -r)
    ```
4.  **`pci.ids` file**: For looking up vendor and device names. This file is usually part of the `pciutils` or `hwdata` package.
    ```bash
    sudo apt install pciutils # (Often includes hwdata or pci.ids)
    # Verify its presence, common paths:
    # ls /usr/share/hwdata/pci.ids
    # ls /usr/share/misc/pci.ids
    ```
    The GUI application currently looks for it at `/usr/share/hwdata/pci.ids`. If it's elsewhere, you may need to update the `PCI_IDS_PATH` macro in `gpu_viewer_gui.c`.

## Compilation Steps

You need to compile both the kernel module and the GUI application.

### 1. Compile the Kernel Module

Navigate to the kernel module's directory and use `make`:

```bash
cd path/to/Linux_Graphics_Card_Viewer/gpu_viewer_v3/kernel/
make clean # Optional: cleans previous builds
make

This will produce a file named gpu_viewer_lkm.ko. If you encounter errors, ensure your kernel headers are correctly installed and the Makefile has correct TAB indentation for command lines.

2. Compile the GUI Application
Navigate to the GUI application's directory and compile using gcc and pkg-config:

cd path/to/Linux_Graphics_Card_Viewer/gpu_viewer_v3/gui/
gcc gpu_viewer_gui.c -o gpu_viewer $(pkg-config --cflags --libs gtk+-3.0)

This will produce an executable file named gpu_viewer.

Running the Application
Load the Kernel Module:
Before running the GUI, you must load the compiled kernel module. Navigate to the kernel/ directory where gpu_viewer_lkm.ko was created:

cd path/to/Linux_Graphics_Card_Viewer/gpu_viewer_v3/kernel/
sudo insmod gpu_viewer_lkm.ko

Verify loading: Check for messages from the module:

sudo dmesg | tail -n 10

You should see messages like "GPU_LKM_REAL: Initializing Netlink GPU Module..." and "GPU_LKM_REAL: Netlink socket created successfully."

You can also check if the module is listed:

lsmod | grep gpu_viewer_lkm

Run the GUI Application:
Once the kernel module is loaded, navigate to the gui/ directory and run the executable:

cd path/to/Linux_Graphics_Card_Viewer/gpu_viewer_v3/gui/
./gpu_viewer

Important: Run the GUI from a standard system terminal, not from an integrated terminal within a Snap-installed IDE (like VS Code), to avoid potential library conflict issues.

The GUI window should appear. Click "Start Real-time Scan". The table should populate with your GPU's information, including vendor and device names looked up from pci.ids. The display will refresh periodically.

Unloading the Kernel Module
When you are done, you can unload the kernel module:

sudo rmmod gpu_viewer_lkm

Check sudo dmesg | tail -n 5 to see the exit messages from the module.

Troubleshooting
make: *** missing separator. Stop. (Kernel Module Compilation):
Ensure lines under targets (like all:, clean:) in the kernel/Makefile start with a TAB character, not spaces.

gpu_viewer_lkm.ko: module verification failed: signature and/or required key missing - tainting kernel (dmesg):
This is normal for out-of-tree, unsigned modules on systems with Secure Boot or module signature enforcement. It generally doesn't affect functionality for development purposes.

./gpu_viewer: socket: Protocol not supported (GUI Runtime):
This means the gpu_viewer_lkm.ko kernel module is not loaded, or it didn't initialize the Netlink socket correctly. Ensure you've run sudo insmod gpu_viewer_lkm.ko and check dmesg for any errors from the module during its loading.

GUI shows "Unknown Vendor" or "Unknown Device" but IDs are correct:
The pci.ids file might not be found at the path specified in gpu_viewer_gui.c (PCI_IDS_PATH), or your specific vendor/device ID might not be in the pci.ids file on your system (though this is rare for common hardware). Verify the PCI_IDS_PATH and ensure pciutils or hwdata is installed.

GUI freezes or shows "not responding":
This usually indicates that the GUI is stuck waiting for a response from the kernel module (recvfrom is blocking). Check sudo dmesg for any errors or messages from the kernel module (GPU_LKM_REAL:) after you click the scan button. The module might be crashing or failing to send a reply.

GUI library errors (e.g., GLIBCXX_... not found):
Try running the ./gpu_viewer executable from a standard system terminal, not one embedded in a Snap-packaged application (like VS Code).

Further Development
Real GPU Data in Kernel Module: The current kernel module (gpu_viewer_lkm.c with "Real Data Logic") attempts to read actual PCI configuration space for the first VGA-compatible device. For more detailed or specific GPU metrics (like temperature, clock speeds, memory usage), the kernel module would need to interact with more specific kernel DRM (Direct Rendering Manager) interfaces or other GPU driver sysfs entries, which is significantly more complex.

Multiple GPUs: The current system is designed to show information for the first GPU found. Supporting multiple GPUs would require changes in both the kernel module (to send a list or allow querying by index) and the GUI (to display multiple rows or select a GPU).

Error Handling: Both the kernel module and GUI application could benefit from more robust error handling and reporting.

Non-Blocking GUI: For a truly responsive GUI, the netlink communication (especially recvfrom) in the GUI could be moved