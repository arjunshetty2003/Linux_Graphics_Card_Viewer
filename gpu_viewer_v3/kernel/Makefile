# Makefile for the GPU Netlink Kernel Module

# obj-m specifies the kernel module object file to be built.
# If your C file is gpu_viewer_lkm.c, then the object will be gpu_viewer_lkm.o,
# and the final module will be gpu_viewer_lkm.ko.
obj-m += gpu_viewer_lkm.o

# KDIR: Path to the Linux kernel source/build directory.
# This is usually /lib/modules/$(shell uname -r)/build
KDIR := /lib/modules/$(shell uname -r)/build

# PWD: Current working directory
PWD := $(shell pwd)

# Specify additional CFLAGS to include the directory where gpu_proto.h is located.
# Assuming your project structure is something like:
# LKP/
# ├── gpu_viewer_v3/
# │   ├── gui/
# │   │   └── gpu_viewer_gui.c
# │   ├── include/
# │   │   └── gpu_proto.h  <-- Shared header
# │   └── kernel/          <-- This Makefile and gpu_viewer_lkm.c are here
# │       ├── Makefile
# │       └── gpu_viewer_lkm.c
#
# So, gpu_proto.h would be in ../include/ relative to this Makefile's directory.
# Adjust the path if your gpu_proto.h is located elsewhere.
EXTRA_CFLAGS += -I$(PWD)/../include

# Default target: 'all'
# IMPORTANT: The next line MUST start with a TAB character, not spaces.
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules EXTRA_CFLAGS="$(EXTRA_CFLAGS)"

# Target to clean up build files
# IMPORTANT: The next line MUST start with a TAB character, not spaces.
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

# Phony targets (targets that don't represent actual files)
.PHONY: all clean
