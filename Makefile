obj-m := gpu_info_viewer.o

# Kernel build directory (adjust for your system)
KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean

install:
	sudo insmod gpu_info_viewer.ko

uninstall:
	sudo rmmod gpu_info_viewer

reload: uninstall install

status:
	lsmod | grep gpu_info_viewer

log:
	dmesg | tail -20

test:
	cat /proc/gpu_monitor

graph:
	python3 gpu_viewer.py

graph-simple:
	python3 gpu_realtime_graph.py

demo:
	python3 gpu_demo_graph.py

enhanced-demo:
	@echo "🔋 Enhanced GPU Monitoring Demo"
	python3 enhanced_demo.py

quick-intel-demo:
	@echo "📡 Quick Intel GPU Raw Data Demo"
	python3 quick_intel_demo.py

gpu-engine-demo:
	@echo "📊 GPU Engine Usage Breakdown Demo"
	python3 gpu_engine_demo.py

terminal:
	python3 gpu_terminal_monitor.py

real-intel:
	python3 real_intel_gpu_monitor.py

real-terminal:
	python3 real_intel_terminal.py

real-power:
	@echo "🔋 Enhanced Intel GPU monitoring with Power & IRQ metrics"
	python3 real_intel_gpu_monitor.py

real-power-terminal:
	@echo "🔋 Enhanced Intel GPU terminal monitoring with Power & IRQ metrics"
	python3 real_intel_terminal.py

simulate:
	python3 simulate_gpu_load.py

install-deps:
	sudo apt-get update
	sudo apt-get install -y python3-pip python3-tk python3-matplotlib intel-gpu-tools
	pip3 install --user matplotlib numpy
	@echo "Setting up intel_gpu_top permissions..."
	@echo "$(USER) ALL=(ALL) NOPASSWD: /usr/bin/intel_gpu_top" | sudo tee /etc/sudoers.d/intel-gpu-tools > /dev/null || true

help:
	@echo "📊 GPU Monitoring System - Available Commands:"
	@echo ""
	@echo "🔧 Kernel Module (Simulated Data):"
	@echo "   make all          - Build the kernel module"
	@echo "   make install      - Install kernel module"
	@echo "   make uninstall    - Remove kernel module"
	@echo "   make test         - View simulated GPU data"
	@echo ""
	@echo "📈 Simulated Monitoring:"
	@echo "   make graph        - Advanced GUI monitor (simulated)"
	@echo "   make graph-simple - Simple real-time graphs (simulated)"
	@echo "   make demo         - Demo with changing data"
	@echo "   make terminal     - Terminal ASCII monitor (simulated)"
	@echo ""
	@echo "🖥️ Real Intel GPU Monitoring:"
	@echo "   make real-intel   - Real Intel GPU graphical monitor"
	@echo "   make real-terminal- Real Intel GPU terminal monitor"
	@echo ""
	@echo "🔋 Enhanced Real Monitoring (with Power & IRQ):"
	@echo "   make real-power         - Enhanced graphical monitor (6 metrics)"
	@echo "   make real-power-terminal- Enhanced terminal monitor (6 metrics)"
	@echo ""
	@echo "🔧 Utilities:"
	@echo "   make simulate     - GPU load simulator"
	@echo "   make enhanced-demo- Enhanced monitoring demo"
	@echo "   make quick-intel-demo - Raw Intel GPU data demo"
	@echo "   make gpu-engine-demo - GPU engine breakdown demo"
	@echo "   make install-deps - Install dependencies"
	@echo "   make clean        - Clean build files"

.PHONY: all clean install uninstall reload status log test graph graph-simple demo enhanced-demo quick-intel-demo terminal real-intel real-terminal real-power real-power-terminal simulate install-deps help