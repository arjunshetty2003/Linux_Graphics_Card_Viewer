# GPU Real-time Monitor - Usage Guide

## Overview
This project provides real-time GPU monitoring with multiple visualization options including terminal-based monitoring and graphical charts using matplotlib.

## Available Commands

### Core Operations
```bash
# Build and install the kernel module
make all
sudo make install

# Check module status
make status

# View raw GPU data
make test

# Remove kernel module
sudo make uninstall
```

### Real-time Monitoring Options

#### 1. Terminal Monitor (📟)
ASCII-based real-time monitoring in the terminal:
```bash
make terminal
```
Features:
- Live updating GPU metrics
- ASCII bar graphs for current values
- Sparkline trends for historical data
- Works in any terminal (no GUI required)
- Colorful emoji indicators

#### 2. Real-time Graphs (📊)
Matplotlib-based live graphing:
```bash
make graph-simple
```
Features:
- Live updating line graphs
- 4 panels: Temperature, GPU Utilization, Memory Usage, Power Usage
- Real GPU data from kernel module
- 50-point rolling window

#### 3. Advanced GUI Monitor (🖥️)
Full-featured GUI application:
```bash
make graph
```
Features:
- Complete GUI with multiple tabs
- Overview cards for each GPU
- Performance graphs
- Detailed hardware information
- Configurable refresh rates

#### 4. Demo with Simulated Data (🎮)
See how the graphs work with changing data:
```bash
make demo
```
Features:
- Realistic simulated GPU data
- Demonstrates all graph types
- Shows how monitoring looks with varying workloads
- Perfect for testing and demonstration

#### 5. Real Intel GPU Monitor (🔥 NEW!)
**Real hardware data** for Intel integrated GPUs:
```bash
make real-intel      # Graphical real-time monitoring
make real-terminal   # Terminal real-time monitoring
```
Features:
- ✅ **Real GPU frequency** from sysfs
- ✅ **Real GPU utilization** from intel_gpu_top
- ✅ **Real temperature** from CPU sensors
- ✅ **Real power consumption** data
- ✅ Live hardware performance counters
- ✅ No simulation - actual Intel GPU metrics!

#### 6. Enhanced Real Intel GPU Monitor (🔋 NEW!)
**Enhanced monitoring with Power & Interrupt metrics**:
```bash
make real-power         # Enhanced graphical monitor (6 metrics)
make real-power-terminal # Enhanced terminal monitor (6 metrics)
```
New Features:
- 🔋 **Power consumption monitoring** (GPU + Package watts)
- 📡 **GPU interrupt rate monitoring** (IRQ/s from i915 driver)
- 📊 **6-panel graphical interface** (3x2 grid)
- 📈 **Enhanced terminal display** with power & IRQ trends
- ⚡ **Real-time power efficiency tracking**
- 🔍 **Detailed interrupt analysis**

**Example Real Data (Intel Alderlake Gen12):**
```
🖥️ Intel Alderlake_s (Gen12) @ /dev/dri/card1
📊 GPU Usage: 0.89% (Render/3D engine)
🚀 Frequency: 6-650 MHz (dynamic scaling)
🔋 Power: 0.02W GPU / 8.14W Package
📡 IRQ Rate: 117-949 interrupts/second
🌡️ Temperature: 41-67°C
⚡ RC6 State: 96% (power saving active)
```

**GPU Usage Breakdown by Engine:**
```
🎮 Render/3D (RCS): 6.5%  ████████░░░░ (desktop effects, 3D graphics)
🎬 Video (VCS):     0.0%  ░░░░░░░░░░░░ (hardware video decode/encode)  
📋 Blitter (BCS):  0.0%  ░░░░░░░░░░░░ (2D operations, memory copies)
✨ VideoEnhance:    0.0%  ░░░░░░░░░░░░ (video scaling, effects)
```

**Note:** GPU usage is **NOT** just video! It combines:
- 🎮 **3D/Render Engine**: Desktop compositing, games, OpenGL
- 🎬 **Video Engines**: Hardware video decode/encode only
- 📋 **Other Engines**: 2D graphics, memory operations

**Enhanced Calculation (v2.1):** GPU usage now properly sums all active engines instead of using only the highest single engine, providing more accurate total utilization metrics.

#### 7. Load Simulator (⚡)
Simulate GPU activity (for demonstration):
```bash
make simulate
```

### Dependencies Installation
```bash
make install-deps
```
Installs:
- python3-matplotlib, python3-tk, python3-numpy
- intel-gpu-tools (for real Intel GPU monitoring)
- Required system packages

**For Real Intel GPU Monitoring:**
```bash
# Additional setup (automatically handled by install-deps)
sudo apt install intel-gpu-tools
```

## Raw Intel GPU Data Sources

The enhanced monitoring integrates with `intel_gpu_top` to capture real hardware metrics:

**intel_gpu_top JSON Output:**
```json
{
  "engines": {
    "Render/3D": {"busy": 0.89},
    "Blitter": {"busy": 0.00},
    "Video": {"busy": 0.00},
    "VideoEnhance": {"busy": 0.00}
  },
  "frequency": {"actual": 650},
  "power": {"GPU": 0.02, "Package": 8.14},
  "period": {"duration": 1000}
}
```

**Interrupt Data from /proc/interrupts:**
```
151: ... IR-PCI-MSI-0000:00:02.0 0-edge i915
```
- Driver interrupts tracked per second
- Real-time IRQ rate calculation
- Hardware activity monitoring

## Data Format

The kernel module provides data through `/proc/gpu_monitor` with the following format:
```
GPU_COUNT:1
GPU_0_NAME:Intel GPU [8086:4680]
GPU_0_VENDOR_ID:0x8086
GPU_0_DEVICE_ID:0x4680
GPU_0_DRIVER:i915
GPU_0_MEMORY_USED:1071
GPU_0_MEMORY_TOTAL:4096
GPU_0_TEMPERATURE:56
GPU_0_CLOCK_MHZ:773
GPU_0_POWER_WATTS:14
GPU_0_UTILIZATION:99
```

**Note:** For Intel integrated GPUs, most metrics (temperature, utilization, power) are simulated since Intel graphics don't expose detailed hardware monitoring through standard Linux interfaces. The simulation provides realistic changing values to demonstrate the monitoring system's capabilities.

## Usage Examples

### Quick Start
```bash
# 1. Build and install
make all
sudo make install

# 2. Install Python dependencies
make install-deps

# 3. Start terminal monitoring
make terminal
```

### For Development/Testing
```bash
# Run the demo to see how graphs work
make demo

# In another terminal, simulate some load
make simulate
```

### For Real Intel GPU Monitoring
```bash
# Real Intel GPU data (recommended!)
make real-terminal   # Terminal with real Intel GPU data
make real-intel      # Graphical with real Intel GPU data

# Enhanced monitoring with power & interrupt tracking
make real-power-terminal  # 6 metrics in terminal
make real-power          # 6-panel graphical interface
```

**Sample Enhanced Terminal Output:**
```
🖥️  Real Intel GPU Monitor
=================================================================
📅 2025-06-12 14:07:17
🔧 Data Source: Real Hardware Sensors

🏷️  GPU: Intel Integrated Graphics (i915)
📡 Monitoring: intel_gpu_top + sysfs

📊 Real-time Metrics:
🌡️  Temperature:      46°C  ███████████░░░░░░░░░
⚡ GPU Usage:       0.9%   ░█░░░░░░░░░░░░░░░░░░
🚀 Frequency:       650MHz █████████░░░░░░░░░░░
💾 Memory Est:      932MB  ███████████████████░
🔋 Power:          8.16W   ████████░░░░░░░░░░░░
📡 IRQ Rate:        605/s  ███████████████░░░░░

📈 Trends (last 20 readings):
🌡️  Temp:  __________________________███▇
⚡ GPU:   ___________________________█░░
🚀 Freq:  __________________________▃██▃
💾 Mem:   __________________________█▇▇▇
🔋 Power: __________________________████
📡 IRQ:   ___________________________██▆
```

### For Production Monitoring
```bash
# Use the advanced GUI
make graph

# Or use terminal monitor for headless systems
make terminal
```

## Troubleshooting

### Module Not Loading
```bash
# Check if module loaded
make status

# Check kernel logs
sudo dmesg | tail -20

# Reload module
make reload
```

### No GPU Data
```bash
# Verify proc file exists
ls -la /proc/gpu*

# Check raw data
make test
```

### Python Dependencies
```bash
# Install/reinstall dependencies
make install-deps

# Check Python packages
pip3 list | grep matplotlib
```

## File Structure

- `gpu_info_viewer.c` - Kernel module source
- `gpu_viewer.py` - Advanced GUI monitor
- `gpu_realtime_graph.py` - Simple real-time graphs
- `gpu_terminal_monitor.py` - Terminal-based monitor
- `gpu_demo_graph.py` - Demo with simulated data
- `simulate_gpu_load.py` - Load simulator
- `Makefile` - Build and run commands

## Features

### Real-time Monitoring
- ✅ Live data updates (1-second intervals)
- ✅ Multiple visualization options
- ✅ Historical data trends
- ✅ Current value indicators

### GPU Metrics Tracked
- 🌡️ Temperature
- ⚡ GPU Utilization
- 💾 Memory Usage
- 🔋 Power Consumption
- 🚀 Clock Speeds
- 🏷️ Hardware Information
- 📡 Interrupt Rate (IRQ/s) - **NEW!**
- ⚡ Power Efficiency - **NEW!**

### Platform Support
- ✅ Linux kernel module
- ✅ Intel GPU support (i915 driver)
- ✅ Terminal and GUI modes
- ✅ X11/Wayland compatible

## Notes

- The kernel module must be loaded for data
- **Intel GPU metrics are simulated** - Intel integrated graphics don't expose detailed hardware monitoring
- **Real metrics available** for NVIDIA/AMD GPUs with proper drivers
- Demo mode works without GPU hardware
- Terminal monitor works on any system
- GUI requires X11 or Wayland display
- All scripts are Python 3 compatible

## Real vs Simulated Data

### What's Real:
- ✅ GPU hardware detection
- ✅ Driver identification  
- ✅ PCI device information
- ✅ **Real Intel GPU frequency** (sysfs)
- ✅ **Real Intel GPU utilization** (intel_gpu_top)
- ✅ **Real temperature** (CPU sensors as proxy)
- ✅ **Real power consumption** (Intel PMU)
- ✅ **Real interrupt rate** (/proc/interrupts i915)
- ✅ **Real power efficiency** (calculated from above)

### What's Simulated (in kernel module demo):
- 🎭 Temperature readings (kernel module)
- 🎭 GPU utilization percentages (kernel module)
- 🎭 Power consumption values (kernel module)
- 🎭 Memory usage metrics (kernel module)

**Use `make real-intel` or `make real-terminal` for actual Intel GPU hardware data!**
