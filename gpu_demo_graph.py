#!/usr/bin/env python3
"""
Demo Real-time GPU Graph with Simulated Data
Shows how the real-time graphs work with changing data
"""
import time
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from collections import deque
import numpy as np
import random
import math

class DemoGPUMonitor:
    def __init__(self, max_points=50):
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
        self.counter = 0
        
    def generate_simulated_data(self):
        """Generate realistic simulated GPU data"""
        self.counter += 1
        
        # Simulate different workload patterns
        base_temp = 45 + 15 * math.sin(self.counter * 0.1) + random.uniform(-3, 3)
        base_util = max(0, min(100, 50 + 30 * math.sin(self.counter * 0.05) + random.uniform(-10, 10)))
        base_memory = max(0, 2048 + 1024 * math.sin(self.counter * 0.03) + random.uniform(-200, 200))
        base_power = max(0, 75 + 25 * math.sin(self.counter * 0.07) + random.uniform(-5, 5))
        
        return {
            'TEMPERATURE': str(base_temp),
            'UTILIZATION': str(base_util),
            'MEMORY_USAGE': str(base_memory),
            'POWER_USAGE': str(base_power),
            'GPU_NAME': 'Demo GPU (Simulated)',
            'DRIVER': 'demo-driver'
        }
    
    def update_data(self):
        """Update data collections with new readings"""
        data = self.generate_simulated_data()
        current_time = time.time() - self.start_time
        
        self.times.append(current_time)
        
        # Extract GPU data
        try:
            temp = float(data.get('TEMPERATURE', '0'))
            util = float(data.get('UTILIZATION', '0'))
            mem = float(data.get('MEMORY_USAGE', '0'))
            power = float(data.get('POWER_USAGE', '0'))
        except (ValueError, TypeError):
            temp = util = mem = power = 0
        
        self.temperatures.append(temp)
        self.gpu_utilization.append(util)
        self.memory_used.append(mem)
        self.power_usage.append(power)

class DemoRealTimeGraphs:
    def __init__(self):
        self.monitor = DemoGPUMonitor()
        
        # Set up the figure and subplots
        self.fig, self.axes = plt.subplots(2, 2, figsize=(14, 10))
        self.fig.suptitle('🖥️ Real-time GPU Monitor Demo - Live Data Visualization', fontsize=16, fontweight='bold')
        
        # Temperature plot
        self.ax1 = self.axes[0, 0]
        self.ax1.set_title('🌡️ Temperature (°C)', fontsize=12, fontweight='bold')
        self.ax1.set_ylabel('Temperature (°C)')
        self.ax1.grid(True, alpha=0.3)
        self.line1, = self.ax1.plot([], [], 'r-', linewidth=2, label='Temperature')
        self.ax1.legend()
        self.ax1.set_ylim(20, 80)
        
        # GPU Utilization plot
        self.ax2 = self.axes[0, 1]
        self.ax2.set_title('⚡ GPU Utilization (%)', fontsize=12, fontweight='bold')
        self.ax2.set_ylabel('Utilization (%)')
        self.ax2.set_ylim(0, 100)
        self.ax2.grid(True, alpha=0.3)
        self.line2, = self.ax2.plot([], [], 'g-', linewidth=2, label='GPU Usage')
        self.ax2.legend()
        
        # Memory Usage plot
        self.ax3 = self.axes[1, 0]
        self.ax3.set_title('💾 Memory Usage (MB)', fontsize=12, fontweight='bold')
        self.ax3.set_ylabel('Memory (MB)')
        self.ax3.grid(True, alpha=0.3)
        self.line3, = self.ax3.plot([], [], 'b-', linewidth=2, label='Memory Used')
        self.ax3.legend()
        self.ax3.set_ylim(0, 4000)
        
        # Power Usage plot
        self.ax4 = self.axes[1, 1]
        self.ax4.set_title('🔋 Power Usage (W)', fontsize=12, fontweight='bold')
        self.ax4.set_ylabel('Power (W)')
        self.ax4.grid(True, alpha=0.3)
        self.line4, = self.ax4.plot([], [], 'orange', linewidth=2, label='Power')
        self.ax4.legend()
        self.ax4.set_ylim(0, 150)
        
        plt.tight_layout()
        
        # Add current values text
        self.temp_text = self.ax1.text(0.02, 0.98, '', transform=self.ax1.transAxes, 
                                      verticalalignment='top', bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.8))
        self.util_text = self.ax2.text(0.02, 0.98, '', transform=self.ax2.transAxes, 
                                      verticalalignment='top', bbox=dict(boxstyle='round', facecolor='lightgreen', alpha=0.8))
        self.mem_text = self.ax3.text(0.02, 0.98, '', transform=self.ax3.transAxes, 
                                     verticalalignment='top', bbox=dict(boxstyle='round', facecolor='lightblue', alpha=0.8))
        self.power_text = self.ax4.text(0.02, 0.98, '', transform=self.ax4.transAxes, 
                                       verticalalignment='top', bbox=dict(boxstyle='round', facecolor='lightyellow', alpha=0.8))
        
    def animate(self, frame):
        """Animation function called by matplotlib"""
        self.monitor.update_data()
        
        times = list(self.monitor.times)
        
        # Update temperature plot
        temps = list(self.monitor.temperatures)
        self.line1.set_data(times, temps)
        self.ax1.relim()
        self.ax1.autoscale_view()
        current_temp = temps[-1] if temps else 0
        self.temp_text.set_text(f'Current: {current_temp:.1f}°C')
        
        # Update GPU utilization plot
        utils = list(self.monitor.gpu_utilization)
        self.line2.set_data(times, utils)
        self.ax2.relim()
        self.ax2.autoscale_view()
        current_util = utils[-1] if utils else 0
        self.util_text.set_text(f'Current: {current_util:.1f}%')
        
        # Update memory usage plot
        mems = list(self.monitor.memory_used)
        self.line3.set_data(times, mems)
        self.ax3.relim()
        self.ax3.autoscale_view()
        current_mem = mems[-1] if mems else 0
        self.mem_text.set_text(f'Current: {current_mem:.0f}MB')
        
        # Update power usage plot
        powers = list(self.monitor.power_usage)
        self.line4.set_data(times, powers)
        self.ax4.relim()
        self.ax4.autoscale_view()
        current_power = powers[-1] if powers else 0
        self.power_text.set_text(f'Current: {current_power:.1f}W')
        
        return self.line1, self.line2, self.line3, self.line4
    
    def start(self):
        """Start the real-time animation"""
        print("🚀 Starting Demo Real-time GPU Monitoring...")
        print("📊 This shows how the graphs work with changing data")
        print("🔄 Data updates every second with realistic simulated values")
        print("❌ Close the window or press Ctrl+C to stop")
        print()
        
        # Create animation with 1000ms interval (1 second)
        self.ani = animation.FuncAnimation(
            self.fig, self.animate, interval=1000, blit=False, cache_frame_data=False
        )
        
        try:
            plt.show()
        except KeyboardInterrupt:
            print("\n🛑 Demo monitoring stopped by user")

def main():
    """Main entry point"""
    print("🎮 GPU Real-time Monitoring Demo")
    print("=" * 40)
    print("This demo shows real-time GPU monitoring with simulated data.")
    print("In a real scenario, this would display actual GPU metrics.")
    print()
    
    try:
        grapher = DemoRealTimeGraphs()
        grapher.start()
    except Exception as e:
        print(f"❌ Error: {e}")

if __name__ == "__main__":
    main()
