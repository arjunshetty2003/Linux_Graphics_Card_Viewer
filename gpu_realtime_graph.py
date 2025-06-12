#!/usr/bin/env python3
"""
Simple Real-time GPU Monitor Graph
Creates live updating graphs for GPU metrics
"""
import time
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from collections import deque
import numpy as np

class SimpleGPUMonitor:
    def __init__(self, proc_file="/proc/gpu_monitor", max_points=50):
        self.proc_file = proc_file
        self.max_points = max_points
        
        # Data storage
        self.times = deque(maxlen=max_points)
        self.temperatures = deque(maxlen=max_points)
        self.gpu_utilization = deque(maxlen=max_points)
        self.memory_used = deque(maxlen=max_points)
        self.power_usage = deque(maxlen=max_points)
        
        # Initialize with zeros
        for _ in range(max_points):
            self.times.append(0)
            self.temperatures.append(0)
            self.gpu_utilization.append(0)
            self.memory_used.append(0)
            self.power_usage.append(0)
        
        self.start_time = time.time()
        
    def read_gpu_data(self):
        """Read GPU data from proc file"""
        try:
            with open(self.proc_file, 'r') as f:
                lines = f.readlines()
            
            data = {}
            for line in lines:
                line = line.strip()
                if ':' in line:
                    key, value = line.split(':', 1)
                    data[key.strip()] = value.strip()
            
            return data
        except Exception as e:
            print(f"Error reading {self.proc_file}: {e}")
            return {}
    
    def update_data(self):
        """Update data collections with new readings"""
        data = self.read_gpu_data()
        current_time = time.time() - self.start_time
        
        self.times.append(current_time)
        
        # Extract GPU data using the correct format
        try:
            temp = float(data.get('GPU_0_TEMPERATURE', '0'))
            util = float(data.get('GPU_0_UTILIZATION', '0'))
            mem = float(data.get('GPU_0_MEMORY_USED', '0'))
            power = float(data.get('GPU_0_POWER_WATTS', '0'))
        except (ValueError, TypeError):
            temp = util = mem = power = 0
        
        self.temperatures.append(temp)
        self.gpu_utilization.append(util)
        self.memory_used.append(mem)
        self.power_usage.append(power)

class RealTimeGraphs:
    def __init__(self):
        self.monitor = SimpleGPUMonitor()
        
        # Set up the figure and subplots
        self.fig, self.axes = plt.subplots(2, 2, figsize=(12, 8))
        self.fig.suptitle('Real-time GPU Monitor', fontsize=16)
        
        # Temperature plot
        self.ax1 = self.axes[0, 0]
        self.ax1.set_title('Temperature (°C)')
        self.ax1.set_ylabel('Temperature')
        self.ax1.grid(True, alpha=0.3)
        self.line1, = self.ax1.plot([], [], 'r-', linewidth=2, label='Temperature')
        self.ax1.legend()
        
        # GPU Utilization plot
        self.ax2 = self.axes[0, 1]
        self.ax2.set_title('GPU Utilization (%)')
        self.ax2.set_ylabel('Utilization')
        self.ax2.set_ylim(0, 100)
        self.ax2.grid(True, alpha=0.3)
        self.line2, = self.ax2.plot([], [], 'g-', linewidth=2, label='GPU Usage')
        self.ax2.legend()
        
        # Memory Usage plot
        self.ax3 = self.axes[1, 0]
        self.ax3.set_title('Memory Usage (MB)')
        self.ax3.set_ylabel('Memory (MB)')
        self.ax3.grid(True, alpha=0.3)
        self.line3, = self.ax3.plot([], [], 'b-', linewidth=2, label='Memory Used')
        self.ax3.legend()
        
        # Power Usage plot
        self.ax4 = self.axes[1, 1]
        self.ax4.set_title('Power Usage (W)')
        self.ax4.set_ylabel('Power (W)')
        self.ax4.grid(True, alpha=0.3)
        self.line4, = self.ax4.plot([], [], 'orange', linewidth=2, label='Power')
        self.ax4.legend()
        
        plt.tight_layout()
        
    def animate(self, frame):
        """Animation function called by matplotlib"""
        self.monitor.update_data()
        
        times = list(self.monitor.times)
        
        # Update temperature plot
        self.line1.set_data(times, list(self.monitor.temperatures))
        self.ax1.relim()
        self.ax1.autoscale_view()
        
        # Update GPU utilization plot
        self.line2.set_data(times, list(self.monitor.gpu_utilization))
        self.ax2.relim()
        self.ax2.autoscale_view()
        
        # Update memory usage plot
        self.line3.set_data(times, list(self.monitor.memory_used))
        self.ax3.relim()
        self.ax3.autoscale_view()
        
        # Update power usage plot
        self.line4.set_data(times, list(self.monitor.power_usage))
        self.ax4.relim()
        self.ax4.autoscale_view()
        
        return self.line1, self.line2, self.line3, self.line4
    
    def start(self):
        """Start the real-time animation"""
        print("Starting real-time GPU monitoring...")
        print("Close the window or press Ctrl+C to stop")
        
        # Create animation with 1000ms interval (1 second)
        self.ani = animation.FuncAnimation(
            self.fig, self.animate, interval=1000, blit=False, cache_frame_data=False
        )
        
        try:
            plt.show()
        except KeyboardInterrupt:
            print("\nMonitoring stopped by user")

def main():
    """Main entry point"""
    try:
        grapher = RealTimeGraphs()
        grapher.start()
    except Exception as e:
        print(f"Error: {e}")
        print("Make sure the GPU monitor kernel module is loaded:")
        print("  sudo make install")
        print("  make test")

if __name__ == "__main__":
    main()
