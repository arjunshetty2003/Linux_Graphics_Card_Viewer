#!/usr/bin/env python3
"""
Terminal-based Real-time GPU Monitor
Displays live GPU metrics in the terminal with ASCII graphs
"""
import time
import os
import sys
from collections import deque

class TerminalGPUMonitor:
    def __init__(self, proc_file="/proc/gpu_monitor", max_points=20):
        self.proc_file = proc_file
        self.max_points = max_points
        
        # Data storage for mini graphs
        self.temperatures = deque(maxlen=max_points)
        self.gpu_utilization = deque(maxlen=max_points)
        self.memory_used = deque(maxlen=max_points)
        self.power_usage = deque(maxlen=max_points)
        
        # Initialize with zeros
        for _ in range(max_points):
            self.temperatures.append(0)
            self.gpu_utilization.append(0)
            self.memory_used.append(0)
            self.power_usage.append(0)
    
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
            return {}
    
    def create_bar_graph(self, values, width=20, max_val=None):
        """Create ASCII bar graph"""
        if not values or max(values) == 0:
            return '█' * 0 + '░' * width
        
        if max_val is None:
            max_val = max(values)
        
        if max_val == 0:
            return '░' * width
        
        latest_val = values[-1]
        filled = int((latest_val / max_val) * width)
        return '█' * filled + '░' * (width - filled)
    
    def create_spark_line(self, values, width=30):
        """Create ASCII sparkline"""
        if not values or max(values) == 0:
            return '_' * width
        
        chars = ['_', '▁', '▂', '▃', '▄', '▅', '▆', '▇', '█']
        max_val = max(values)
        min_val = min(values)
        
        if max_val == min_val:
            return '_' * width
        
        result = ""
        step = len(values) / width
        
        for i in range(width):
            idx = int(i * step)
            if idx >= len(values):
                idx = len(values) - 1
            
            val = values[idx]
            normalized = (val - min_val) / (max_val - min_val)
            char_idx = int(normalized * (len(chars) - 1))
            result += chars[char_idx]
        
        return result
    
    def clear_screen(self):
        """Clear terminal screen"""
        os.system('clear' if os.name == 'posix' else 'cls')
    
    def display_data(self):
        """Display current GPU data"""
        data = self.read_gpu_data()
        
        if not data:
            print("❌ No GPU data available. Is the kernel module loaded?")
            return
        
        self.clear_screen()
        
        # Update data collections
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
        
        # Header
        print("🖥️  Real-time GPU Monitor")
        print("=" * 60)
        print(f"📅 {time.strftime('%Y-%m-%d %H:%M:%S')}")
        print()
        
        # GPU Info
        gpu_count = data.get('GPU_COUNT', '0')
        print(f"🔢 GPU Count: {gpu_count}")
        
        if gpu_count != '0':
            print(f"🏷️  GPU Name: {data.get('GPU_0_NAME', 'Unknown')}")
            print(f"🚀 Driver: {data.get('GPU_0_DRIVER', 'Unknown')}")
            print()
            
            # Current values with bars
            print("📊 Current Metrics:")
            print(f"🌡️  Temperature: {temp:6.1f}°C  {self.create_bar_graph([temp], max_val=100)}")
            print(f"⚡ GPU Usage:   {util:6.1f}%   {self.create_bar_graph([util], max_val=100)}")
            print(f"💾 Memory:      {mem:6.1f}MB  {self.create_bar_graph([mem], max_val=max(self.memory_used) if max(self.memory_used) > 0 else 1000)}")
            print(f"🔋 Power:       {power:6.1f}W   {self.create_bar_graph([power], max_val=max(self.power_usage) if max(self.power_usage) > 0 else 100)}")
            print()
            
            # Mini trend graphs
            print("📈 Trends (last 20 readings):")
            print(f"🌡️  Temp:  {self.create_spark_line(list(self.temperatures))}")
            print(f"⚡ GPU:   {self.create_spark_line(list(self.gpu_utilization))}")
            print(f"💾 Mem:   {self.create_spark_line(list(self.memory_used))}")
            print(f"🔋 Power: {self.create_spark_line(list(self.power_usage))}")
            print()
            
            # Additional info if available
            memory_total = data.get('GPU_0_MEMORY_TOTAL', '0')
            try:
                mem_total = float(memory_total)
                if mem_total > 0:
                    mem_percent = (mem / mem_total) * 100
                    print(f"💾 Memory Usage: {mem:.0f}MB / {mem_total:.0f}MB ({mem_percent:.1f}%)")
            except (ValueError, TypeError):
                pass
            
            clock_speed = data.get('GPU_0_CLOCK_MHZ', '0')
            if clock_speed != '0':
                print(f"⚡ Clock Speed: {clock_speed} MHz")
        
        print()
        print("Press Ctrl+C to stop monitoring...")
    
    def run(self):
        """Run the terminal monitor"""
        print("Starting terminal GPU monitor...")
        print("Press Ctrl+C to stop")
        
        try:
            while True:
                self.display_data()
                time.sleep(1)
        except KeyboardInterrupt:
            print("\n\n👋 Monitoring stopped by user")
        except Exception as e:
            print(f"\n❌ Error: {e}")

def main():
    """Main entry point"""
    try:
        monitor = TerminalGPUMonitor()
        monitor.run()
    except Exception as e:
        print(f"Error: {e}")
        print("Make sure the GPU monitor kernel module is loaded:")
        print("  sudo make install")
        print("  make test")

if __name__ == "__main__":
    main()
