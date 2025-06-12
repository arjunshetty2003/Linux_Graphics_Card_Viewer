#!/usr/bin/env python3
"""
Real Intel GPU Monitor
Reads actual Intel GPU metrics using intel_gpu_top and system interfaces
"""
import subprocess
import json
import time
import re
import os
from collections import deque
import matplotlib.pyplot as plt
import matplotlib.animation as animation

class RealIntelGPUMonitor:
    def __init__(self, max_points=50):
        self.max_points = max_points
        
        # Data storage
        self.times = deque(maxlen=max_points)
        self.temperatures = deque(maxlen=max_points)
        self.gpu_utilization = deque(maxlen=max_points)
        self.memory_used = deque(maxlen=max_points)
        self.frequencies = deque(maxlen=max_points)
        self.power_watts = deque(maxlen=max_points)
        self.interrupt_rates = deque(maxlen=max_points)
        
        # Initialize with zeros
        for _ in range(max_points):
            self.times.append(0)
            self.temperatures.append(0)
            self.gpu_utilization.append(0)
            self.memory_used.append(0)
            self.frequencies.append(0)
            self.power_watts.append(0)
            self.interrupt_rates.append(0)
        
        self.start_time = time.time()
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
                        except:
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
        except:
            return 0

    def read_intel_gpu_frequency(self):
        """Read actual Intel GPU frequency"""
        try:
            # Try multiple frequency sources
            freq_paths = [
                "/sys/class/drm/card*/gt/gt0/rps_cur_freq_mhz",
                "/sys/class/drm/card*/gt_cur_freq_mhz", 
                "/sys/class/drm/card*/device/gt_cur_freq_mhz"
            ]
            
            for pattern in freq_paths:
                import glob
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
        """Read CPU temperature as proxy for integrated GPU temperature"""
        try:
            # Read from coretemp sensor
            temp_paths = [
                "/sys/class/hwmon/hwmon*/temp*_input",
                "/sys/class/thermal/thermal_zone*/temp"
            ]
            
            import glob
            for pattern in temp_paths:
                files = glob.glob(pattern)
                for file_path in files:
                    try:
                        with open(file_path, 'r') as f:
                            temp = int(f.read().strip())
                            # Convert millidegrees to degrees, add offset for GPU
                            if temp > 1000:
                                return (temp // 1000) + 5  # GPU usually 5°C higher
                            else:
                                return temp + 5
                    except:
                        continue
            return 0
        except:
            return 0
    
    def read_intel_gpu_top_data(self):
        """Read Intel GPU utilization using intel_gpu_top"""
        try:
            # Run intel_gpu_top for 1 second and parse output (requires sudo)
            result = subprocess.run(['sudo', 'intel_gpu_top', '-s', '1000', '-J'], 
                                  capture_output=True, text=True, timeout=4)
            
            if result.returncode == 0:
                # Parse JSON output - get the last complete measurement
                lines = result.stdout.strip().split('\n')
                latest_data = None
                
                for line in lines:
                    if line.startswith('{'):
                        try:
                            data = json.loads(line)
                            # Skip the first measurement (usually incomplete)
                            if data.get('period', {}).get('duration', 0) > 500:
                                latest_data = data
                        except json.JSONDecodeError:
                            continue
                
                if latest_data:
                    engines = latest_data.get('engines', {})
                    
                    # Get utilization from all engines
                    total_util = 0
                    render_util = 0
                    video_util = 0
                    blitter_util = 0
                    
                    for engine_name, engine_data in engines.items():
                        util = engine_data.get('busy', 0)
                        total_util += util  # Sum all engine utilizations for total GPU usage
                        
                        # Track specific engine types
                        if 'Render' in engine_name or 'render' in engine_name or '3D' in engine_name:
                            render_util = max(render_util, util)
                        elif 'Video' in engine_name or 'video' in engine_name or 'VCS' in engine_name:
                            video_util = max(video_util, util)
                        elif 'Blitter' in engine_name or 'blitter' in engine_name or 'BCS' in engine_name:
                            blitter_util = max(blitter_util, util)
                    
                    # Get frequency and power info
                    frequency_data = latest_data.get('frequency', {})
                    power_data = latest_data.get('power', {})
                    
                    return {
                        'utilization': total_util,
                        'render_util': render_util,
                        'video_util': video_util,
                        'blitter_util': blitter_util,
                        'frequency': frequency_data.get('actual', 0),
                        'gpu_power': power_data.get('GPU', 0),
                        'package_power': power_data.get('Package', 0)
                    }
            
            return {'utilization': 0, 'render_util': 0, 'video_util': 0, 'blitter_util': 0, 'frequency': 0, 'gpu_power': 0, 'package_power': 0}
            
        except (subprocess.TimeoutExpired, subprocess.CalledProcessError, FileNotFoundError):
            return {'utilization': 0, 'render_util': 0, 'video_util': 0, 'blitter_util': 0, 'frequency': 0, 'gpu_power': 0, 'package_power': 0}
    
    def read_system_memory_usage(self):
        """Estimate GPU memory usage from system memory"""
        try:
            with open('/proc/meminfo', 'r') as f:
                meminfo = f.read()
            
            # Extract memory values
            total_match = re.search(r'MemTotal:\s+(\d+)\s+kB', meminfo)
            available_match = re.search(r'MemAvailable:\s+(\d+)\s+kB', meminfo)
            
            if total_match and available_match:
                total_kb = int(total_match.group(1))
                available_kb = int(available_match.group(1))
                used_kb = total_kb - available_kb
                
                # Estimate GPU memory as ~10-15% of used system memory
                gpu_memory_estimate = (used_kb * 0.12) // 1024  # Convert to MB
                return min(gpu_memory_estimate, 2048)  # Cap at 2GB
            
            return 0
        except:
            return 0
    
    def update_data(self):
        """Update data collections with real Intel GPU readings"""
        current_time = time.time() - self.start_time
        self.times.append(current_time)
        
        # Get real Intel GPU data
        temperature = self.read_cpu_temperature()
        intel_data = self.read_intel_gpu_top_data()
        memory_estimate = self.read_system_memory_usage()
        interrupt_rate = self.read_gpu_interrupt_rate()
        
        # Use actual frequency from intel_gpu_top if available, fallback to sysfs
        frequency = intel_data.get('frequency', 0)
        if frequency == 0:
            frequency = self.read_intel_gpu_frequency()
        
        utilization = intel_data.get('utilization', 0)
        
        # Power data (GPU power + Package power for total)
        gpu_power = intel_data.get('gpu_power', 0)
        package_power = intel_data.get('package_power', 0)
        total_power = gpu_power + package_power
        
        self.frequencies.append(frequency)
        self.temperatures.append(temperature)
        self.gpu_utilization.append(utilization)
        self.memory_used.append(memory_estimate)
        self.power_watts.append(total_power)
        self.interrupt_rates.append(interrupt_rate)
        
        return {
            'frequency': frequency,
            'temperature': temperature,
            'utilization': utilization,
            'memory': memory_estimate,
            'gpu_power': gpu_power,
            'package_power': package_power,
            'total_power': total_power,
            'interrupt_rate': interrupt_rate
        }

class RealIntelGPUGraphs:
    def __init__(self):
        self.monitor = RealIntelGPUMonitor()
        
        # Set up the figure and subplots (3x2 grid for 6 metrics)
        self.fig, self.axes = plt.subplots(3, 2, figsize=(14, 12))
        self.fig.suptitle('🖥️ Real Intel GPU Monitor - Live Hardware Data', fontsize=16, fontweight='bold')
        
        # Temperature plot
        self.ax1 = self.axes[0, 0]
        self.ax1.set_title('🌡️ Temperature (°C)', fontsize=12, fontweight='bold')
        self.ax1.set_ylabel('Temperature (°C)')
        self.ax1.grid(True, alpha=0.3)
        self.line1, = self.ax1.plot([], [], 'r-', linewidth=2, label='CPU+GPU Temp')
        self.ax1.legend()
        
        # GPU Utilization plot
        self.ax2 = self.axes[0, 1]
        self.ax2.set_title('⚡ GPU Utilization (%)', fontsize=12, fontweight='bold')
        self.ax2.set_ylabel('Utilization (%)')
        self.ax2.set_ylim(0, 100)
        self.ax2.grid(True, alpha=0.3)
        self.line2, = self.ax2.plot([], [], 'g-', linewidth=2, label='Render Engine')
        self.ax2.legend()
        
        # Memory Usage plot
        self.ax3 = self.axes[1, 0]
        self.ax3.set_title('💾 Memory Usage (MB)', fontsize=12, fontweight='bold')
        self.ax3.set_ylabel('Memory (MB)')
        self.ax3.grid(True, alpha=0.3)
        self.line3, = self.ax3.plot([], [], 'b-', linewidth=2, label='Estimated Usage')
        self.ax3.legend()
        
        # Frequency plot
        self.ax4 = self.axes[1, 1]
        self.ax4.set_title('🚀 GPU Frequency (MHz)', fontsize=12, fontweight='bold')
        self.ax4.set_ylabel('Frequency (MHz)')
        self.ax4.grid(True, alpha=0.3)
        self.line4, = self.ax4.plot([], [], 'orange', linewidth=2, label='Current Freq')
        self.ax4.legend()
        
        # Power plot
        self.ax5 = self.axes[2, 0]
        self.ax5.set_title('🔋 Power Consumption (W)', fontsize=12, fontweight='bold')
        self.ax5.set_ylabel('Power (W)')
        self.ax5.grid(True, alpha=0.3)
        self.line5, = self.ax5.plot([], [], 'purple', linewidth=2, label='Total Power')
        self.ax5.legend()
        
        # Interrupt Rate plot
        self.ax6 = self.axes[2, 1]
        self.ax6.set_title('📡 GPU Interrupt Rate (IRQ/s)', fontsize=12, fontweight='bold')
        self.ax6.set_ylabel('Interrupts/sec')
        self.ax6.grid(True, alpha=0.3)
        self.line6, = self.ax6.plot([], [], 'brown', linewidth=2, label='i915 IRQ Rate')
        self.ax6.legend()
        
        plt.tight_layout()
        
        # Add current values text
        self.temp_text = self.ax1.text(0.02, 0.98, '', transform=self.ax1.transAxes, 
                                      verticalalignment='top', bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.8))
        self.util_text = self.ax2.text(0.02, 0.98, '', transform=self.ax2.transAxes, 
                                      verticalalignment='top', bbox=dict(boxstyle='round', facecolor='lightgreen', alpha=0.8))
        self.mem_text = self.ax3.text(0.02, 0.98, '', transform=self.ax3.transAxes, 
                                     verticalalignment='top', bbox=dict(boxstyle='round', facecolor='lightblue', alpha=0.8))
        self.freq_text = self.ax4.text(0.02, 0.98, '', transform=self.ax4.transAxes, 
                                       verticalalignment='top', bbox=dict(boxstyle='round', facecolor='lightyellow', alpha=0.8))
        self.power_text = self.ax5.text(0.02, 0.98, '', transform=self.ax5.transAxes, 
                                        verticalalignment='top', bbox=dict(boxstyle='round', facecolor='plum', alpha=0.8))
        self.irq_text = self.ax6.text(0.02, 0.98, '', transform=self.ax6.transAxes, 
                                      verticalalignment='top', bbox=dict(boxstyle='round', facecolor='tan', alpha=0.8))
        
        self.last_data = {}
        
    def animate(self, frame):
        """Animation function called by matplotlib"""
        self.last_data = self.monitor.update_data()
        
        times = list(self.monitor.times)
        
        # Update temperature plot
        temps = list(self.monitor.temperatures)
        self.line1.set_data(times, temps)
        self.ax1.relim()
        self.ax1.autoscale_view()
        current_temp = temps[-1] if temps else 0
        self.temp_text.set_text(f'Current: {current_temp:.0f}°C')
        
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
        
        # Update frequency plot
        freqs = list(self.monitor.frequencies)
        self.line4.set_data(times, freqs)
        self.ax4.relim()
        self.ax4.autoscale_view()
        current_freq = freqs[-1] if freqs else 0
        self.freq_text.set_text(f'Current: {current_freq:.0f}MHz')
        
        # Update power plot
        powers = list(self.monitor.power_watts)
        self.line5.set_data(times, powers)
        self.ax5.relim()
        self.ax5.autoscale_view()
        current_power = powers[-1] if powers else 0
        self.power_text.set_text(f'Current: {current_power:.2f}W')
        
        # Update interrupt rate plot
        irqs = list(self.monitor.interrupt_rates)
        self.line6.set_data(times, irqs)
        self.ax6.relim()
        self.ax6.autoscale_view()
        current_irq = irqs[-1] if irqs else 0
        self.irq_text.set_text(f'Current: {current_irq:.0f} IRQ/s')
        
        return self.line1, self.line2, self.line3, self.line4, self.line5, self.line6
    
    def start(self):
        """Start the real-time animation"""
        print("🚀 Starting Real Intel GPU Monitoring...")
        print("📊 Reading actual hardware data from:")
        print("   • intel_gpu_top for GPU utilization")
        print("   • /sys/class/drm for GPU frequency") 
        print("   • /sys/class/hwmon for temperature")
        print("   • /proc/meminfo for memory estimation")
        print("🔄 Data updates every 2 seconds")
        print("❌ Close the window or press Ctrl+C to stop")
        print()
        
        # Test data sources
        test_data = self.monitor.update_data()
        print(f"📈 Initial readings:")
        print(f"   Temperature: {test_data['temperature']}°C")
        print(f"   GPU Usage: {test_data['utilization']:.1f}%")
        print(f"   Frequency: {test_data['frequency']}MHz")
        print(f"   Memory Est: {test_data['memory']:.0f}MB")
        print(f"   Power: {test_data['total_power']:.2f}W")
        print(f"   IRQ Rate: {test_data['interrupt_rate']:.0f}/s")
        print()
        
        # Create animation with 2000ms interval (2 seconds)
        self.ani = animation.FuncAnimation(
            self.fig, self.animate, interval=2000, blit=False, cache_frame_data=False
        )
        
        try:
            plt.show()
        except KeyboardInterrupt:
            print("\n🛑 Real Intel GPU monitoring stopped by user")

def main():
    """Main entry point"""
    print("🎮 Real Intel GPU Monitoring")
    print("=" * 40)
    print("This reads actual Intel GPU hardware data.")
    print("Requires: intel_gpu_top, sysfs access")
    print()
    
    # Check prerequisites
    try:
        subprocess.run(['intel_gpu_top', '--help'], capture_output=True, timeout=2)
        print("✅ intel_gpu_top found")
    except:
        print("❌ intel_gpu_top not found. Install with: sudo apt install intel-gpu-tools")
        return
    
    if not os.path.exists('/sys/class/drm'):
        print("❌ DRM interface not found")
        return
    else:
        print("✅ DRM interface available")
    
    print()
    
    try:
        grapher = RealIntelGPUGraphs()
        grapher.start()
    except Exception as e:
        print(f"❌ Error: {e}")

if __name__ == "__main__":
    main()
