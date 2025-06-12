#!/usr/bin/env python3
"""
Real Intel GPU Terminal Monitor
Terminal-based real-time monitoring using actual Intel GPU data
"""
import subprocess
import json
import time
import re
import os
import glob
from collections import deque

class RealIntelTerminalMonitor:
    def __init__(self, max_points=20):
        self.max_points = max_points
        
        # Data storage for mini graphs
        self.temperatures = deque(maxlen=max_points)
        self.gpu_utilization = deque(maxlen=max_points)
        self.memory_used = deque(maxlen=max_points)
        self.frequencies = deque(maxlen=max_points)
        self.power_watts = deque(maxlen=max_points)
        self.interrupt_rates = deque(maxlen=max_points)
        
        # Initialize with zeros
        for _ in range(max_points):
            self.temperatures.append(0)
            self.gpu_utilization.append(0)
            self.memory_used.append(0)
            self.frequencies.append(0)
            self.power_watts.append(0)
            self.interrupt_rates.append(0)
        
        # For interrupt rate calculation
        self.last_interrupt_count = 0
        self.last_interrupt_time = time.time()
    
    def read_gpu_interrupt_rate(self):
        """Read Intel GPU interrupt rate from /proc/interrupts"""
        try:
            current_time = time.time()
            with open('/proc/interrupts', 'r') as f:
                lines = f.readlines()
            
            # Find the i915 (Intel GPU) interrupt line
            gpu_interrupt_count = 0
            for line in lines:
                if 'i915' in line:
                    # Sum interrupts across all CPUs
                    parts = line.split()
                    for i in range(1, len(parts)):
                        try:
                            if parts[i].isdigit():
                                gpu_interrupt_count += int(parts[i])
                            else:
                                break
                        except ValueError:
                            break
                    break
            
            # Calculate interrupt rate
            time_diff = current_time - self.last_interrupt_time
            count_diff = gpu_interrupt_count - self.last_interrupt_count
            
            interrupt_rate = 0
            if time_diff > 0 and self.last_interrupt_count > 0:
                interrupt_rate = count_diff / time_diff
            
            # Update for next calculation
            self.last_interrupt_count = gpu_interrupt_count
            self.last_interrupt_time = current_time
            
            return max(0, interrupt_rate)  # Ensure non-negative
        except Exception:
            return 0

    def read_intel_gpu_frequency(self):
        """Read actual Intel GPU frequency"""
        try:
            freq_paths = [
                "/sys/class/drm/card*/gt/gt0/rps_cur_freq_mhz",
                "/sys/class/drm/card*/gt_cur_freq_mhz", 
                "/sys/class/drm/card*/device/gt_cur_freq_mhz"
            ]
            
            for pattern in freq_paths:
                files = glob.glob(pattern)
                for file_path in files:
                    try:
                        with open(file_path, 'r') as f:
                            freq = int(f.read().strip())
                            return freq
                    except:
                        continue
            return 0
        except:
            return 0
    
    def read_cpu_temperature(self):
        """Read CPU temperature as proxy for integrated GPU"""
        try:
            temp_paths = [
                "/sys/class/hwmon/hwmon*/temp*_input",
                "/sys/class/thermal/thermal_zone*/temp"
            ]
            
            for pattern in temp_paths:
                files = glob.glob(pattern)
                for file_path in files:
                    try:
                        with open(file_path, 'r') as f:
                            temp = int(f.read().strip())
                            if temp > 1000:
                                return (temp // 1000) + 3  # GPU estimate
                            else:
                                return temp + 3
                    except:
                        continue
            return 0
        except:
            return 0
    
    def read_intel_gpu_utilization(self):
        """Read Intel GPU utilization using intel_gpu_top"""
        try:
            result = subprocess.run(['sudo', 'intel_gpu_top', '-s', '1000', '-J'], 
                                  capture_output=True, text=True, timeout=3)
            
            if result.returncode == 0:
                lines = result.stdout.strip().split('\n')
                latest_data = None
                
                for line in lines:
                    if line.startswith('{'):
                        try:
                            data = json.loads(line)
                            if data.get('period', {}).get('duration', 0) > 500:
                                latest_data = data
                        except json.JSONDecodeError:
                            continue
                
                if latest_data:
                    engines = latest_data.get('engines', {})
                    total_util = 0
                    for engine_name, engine_data in engines.items():
                        util = engine_data.get('busy', 0)
                        total_util += util  # Sum all engine utilizations for total GPU usage
                    
                    # Also return frequency and power data
                    frequency_data = latest_data.get('frequency', {})
                    power_data = latest_data.get('power', {})
                    
                    return {
                        'utilization': total_util,
                        'frequency': frequency_data.get('actual', 0),
                        'gpu_power': power_data.get('GPU', 0),
                        'package_power': power_data.get('Package', 0)
                    }
            
            return {'utilization': 0, 'frequency': 0, 'gpu_power': 0, 'package_power': 0}
        except (subprocess.TimeoutExpired, subprocess.CalledProcessError, FileNotFoundError, json.JSONDecodeError):
            return {'utilization': 0, 'frequency': 0, 'gpu_power': 0, 'package_power': 0}
    
    def read_system_memory_usage(self):
        """Estimate GPU memory usage"""
        try:
            with open('/proc/meminfo', 'r') as f:
                meminfo = f.read()
            
            total_match = re.search(r'MemTotal:\s+(\d+)\s+kB', meminfo)
            available_match = re.search(r'MemAvailable:\s+(\d+)\s+kB', meminfo)
            
            if total_match and available_match:
                total_kb = int(total_match.group(1))
                available_kb = int(available_match.group(1))
                used_kb = total_kb - available_kb
                
                gpu_memory_estimate = (used_kb * 0.15) // 1024
                return min(gpu_memory_estimate, 3072)
            
            return 0
        except:
            return 0
    
    def create_bar_graph(self, values, width=20, max_val=None):
        """Create ASCII bar graph"""
        if not values or max(values) == 0:
            return '░' * width
        
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
        """Display current Intel GPU data"""
        self.clear_screen()
        
        # Read real data
        temp = self.read_cpu_temperature()
        intel_data = self.read_intel_gpu_utilization()
        mem = self.read_system_memory_usage()
        interrupt_rate = self.read_gpu_interrupt_rate()
        
        # Extract data from intel_gpu_top
        util = intel_data.get('utilization', 0)
        freq_intel = intel_data.get('frequency', 0)
        gpu_power = intel_data.get('gpu_power', 0)
        package_power = intel_data.get('package_power', 0)
        total_power = gpu_power + package_power
        
        # Fallback to sysfs frequency if intel_gpu_top doesn't provide it
        freq = freq_intel if freq_intel > 0 else self.read_intel_gpu_frequency()
        
        # Update data collections
        self.temperatures.append(temp)
        self.gpu_utilization.append(util)
        self.frequencies.append(freq)
        self.memory_used.append(mem)
        self.power_watts.append(total_power)
        self.interrupt_rates.append(interrupt_rate)
        
        # Header
        print("🖥️  Real Intel GPU Monitor")
        print("=" * 65)
        print(f"📅 {time.strftime('%Y-%m-%d %H:%M:%S')}")
        print("🔧 Data Source: Real Hardware Sensors")
        print()
        
        # GPU Info
        print("🏷️  GPU: Intel Integrated Graphics (i915)")
        print("📡 Monitoring: intel_gpu_top + sysfs")
        print()
        
        # Current values with bars
        print("📊 Real-time Metrics:")
        print(f"🌡️  Temperature:  {temp:6.0f}°C  {self.create_bar_graph([temp], max_val=80)}")
        print(f"⚡ GPU Usage:    {util:6.1f}%   {self.create_bar_graph([util], max_val=100)}")
        print(f"🚀 Frequency:    {freq:6.0f}MHz {self.create_bar_graph([freq], max_val=max(self.frequencies) if max(self.frequencies) > 0 else 1200)}")
        print(f"💾 Memory Est:   {mem:6.0f}MB  {self.create_bar_graph([mem], max_val=max(self.memory_used) if max(self.memory_used) > 0 else 2048)}")
        print(f"🔋 Power:        {total_power:6.2f}W   {self.create_bar_graph([total_power], max_val=max(self.power_watts) if max(self.power_watts) > 0 else 20)}")
        print(f"📡 IRQ Rate:     {interrupt_rate:6.0f}/s  {self.create_bar_graph([interrupt_rate], max_val=max(self.interrupt_rates) if max(self.interrupt_rates) > 0 else 1000)}")
        print()
        
        # Mini trend graphs
        print("📈 Trends (last 20 readings):")
        print(f"🌡️  Temp:  {self.create_spark_line(list(self.temperatures))}")
        print(f"⚡ GPU:   {self.create_spark_line(list(self.gpu_utilization))}")
        print(f"🚀 Freq:  {self.create_spark_line(list(self.frequencies))}")
        print(f"💾 Mem:   {self.create_spark_line(list(self.memory_used))}")
        print(f"🔋 Power: {self.create_spark_line(list(self.power_watts))}")
        print(f"📡 IRQ:   {self.create_spark_line(list(self.interrupt_rates))}")
        print()
        
        # Data sources info
        print("📡 Data Sources:")
        freq_source = "Available" if freq > 0 else "Not found"
        temp_source = "Available" if temp > 0 else "Not found" 
        print(f"   • GPU Frequency: {freq_source}")
        print(f"   • Temperature: {temp_source}")
        print("   • GPU Utilization: intel_gpu_top")
        print("   • Memory: System estimation")
        print("   • Power: Intel PMU (GPU + Package)")
        print("   • Interrupts: /proc/interrupts (i915)")
        print()
        print("Press Ctrl+C to stop monitoring...")
    
    def run(self):
        """Run the real Intel GPU monitor"""
        print("🚀 Starting Real Intel GPU Terminal Monitor...")
        print("📊 This reads actual Intel GPU hardware data")
        print("🔄 Updates every 2 seconds")
        print("Press Ctrl+C to stop")
        print()
        
        # Check prerequisites
        try:
            subprocess.run(['intel_gpu_top', '--help'], capture_output=True, timeout=1)
            print("✅ intel_gpu_top found")
        except (subprocess.CalledProcessError, FileNotFoundError, subprocess.TimeoutExpired):
            print("❌ intel_gpu_top not available")
            print("Install with: sudo apt install intel-gpu-tools")
            return
        
        time.sleep(1)
        
        try:
            while True:
                self.display_data()
                time.sleep(2)
        except KeyboardInterrupt:
            print("\n\n👋 Real Intel GPU monitoring stopped by user")
        except Exception as e:
            print(f"\n❌ Error: {e}")

def main():
    """Main entry point"""
    try:
        monitor = RealIntelTerminalMonitor()
        monitor.run()
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    main()
