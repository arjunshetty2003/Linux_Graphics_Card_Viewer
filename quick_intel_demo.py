#!/usr/bin/env python3
"""
Quick Intel GPU Data Demo
Shows the raw data being captured by the enhanced monitoring system
"""
import subprocess
import json
import time
import glob

def show_intel_gpu_top_data():
    """Show real intel_gpu_top JSON output"""
    print("📡 Real intel_gpu_top Data Capture:")
    print("=" * 50)
    
    try:
        # Run intel_gpu_top to get real data
        result = subprocess.run(['sudo', 'intel_gpu_top', '-s', '1000', '-J'], 
                              capture_output=True, text=True, timeout=4)
        
        if result.returncode == 0:
            lines = result.stdout.strip().split('\n')
            for line in lines:
                if line.startswith('{'):
                    try:
                        data = json.loads(line)
                        if data.get('period', {}).get('duration', 0) > 500:
                            print("Raw JSON from intel_gpu_top:")
                            print(json.dumps(data, indent=2))
                            
                            # Extract key metrics
                            engines = data.get('engines', {})
                            frequency = data.get('frequency', {})
                            power = data.get('power', {})
                            
                            print("\n📊 Extracted Metrics:")
                            print(f"🚀 GPU Frequency: {frequency.get('actual', 0)} MHz")
                            print(f"🔋 GPU Power: {power.get('GPU', 0)} W")
                            print(f"🔋 Package Power: {power.get('Package', 0)} W")
                            print(f"🔋 Total Power: {power.get('GPU', 0) + power.get('Package', 0)} W")
                            
                            print("\n⚡ Engine Utilization:")
                            for engine, data_eng in engines.items():
                                busy = data_eng.get('busy', 0)
                                print(f"   {engine}: {busy:.2f}%")
                            
                            break
                    except json.JSONDecodeError:
                        continue
        else:
            print("❌ Failed to get intel_gpu_top data")
            
    except Exception as e:
        print(f"❌ Error: {e}")

def show_interrupt_data():
    """Show interrupt data from /proc/interrupts"""
    print("\n\n📡 GPU Interrupt Data:")
    print("=" * 50)
    
    try:
        with open('/proc/interrupts', 'r') as f:
            lines = f.readlines()
        
        # Find i915 interrupt line
        for line in lines:
            if 'i915' in line:
                parts = line.split()
                
                # Sum interrupts across all CPUs
                total_irqs = 0
                cpu_count = 0
                for i in range(1, len(parts)):
                    try:
                        if parts[i].isdigit():
                            total_irqs += int(parts[i])
                            cpu_count += 1
                        else:
                            break
                    except:
                        break
                
                print(f"i915 Interrupt Line: {line.strip()}")
                print(f"📊 Total Interrupts: {total_irqs:,}")
                print(f"💻 CPU Cores: {cpu_count}")
                print(f"📡 Current IRQ Count: {total_irqs}")
                break
        else:
            print("❌ No i915 interrupt found")
            
    except Exception as e:
        print(f"❌ Error reading interrupts: {e}")

def show_frequency_data():
    """Show GPU frequency from sysfs"""
    print("\n\n🚀 GPU Frequency Data:")
    print("=" * 50)
    
    freq_paths = [
        "/sys/class/drm/card*/gt/gt0/rps_cur_freq_mhz",
        "/sys/class/drm/card*/gt_cur_freq_mhz"
    ]
    
    for pattern in freq_paths:
        files = glob.glob(pattern)
        for file_path in files:
            try:
                with open(file_path, 'r') as f:
                    freq = int(f.read().strip())
                print(f"sysfs Path: {file_path}")
                print(f"🚀 Current Frequency: {freq} MHz")
                return
            except Exception as e:
                print(f"❌ Error reading {file_path}: {e}")
    
    print("❌ No frequency data found")

def show_temperature_data():
    """Show temperature from hwmon"""
    print("\n\n🌡️ Temperature Data:")
    print("=" * 50)
    
    temp_paths = [
        "/sys/class/hwmon/hwmon*/temp*_input"
    ]
    
    found_temp = False
    for pattern in temp_paths:
        files = glob.glob(pattern)[:3]  # Show first 3 sensors
        for file_path in files:
            try:
                with open(file_path, 'r') as f:
                    temp = int(f.read().strip())
                    if temp > 1000:
                        temp_c = temp // 1000
                    else:
                        temp_c = temp
                    
                    print(f"Sensor: {file_path}")
                    print(f"🌡️ Temperature: {temp_c}°C")
                    found_temp = True
            except Exception as e:
                continue
    
    if not found_temp:
        print("❌ No temperature sensors found")

def main():
    """Main demonstration"""
    print("🔋 Intel GPU Enhanced Monitoring - Raw Data Demo")
    print("=" * 60)
    print("This shows the actual data sources used by the enhanced monitoring\n")
    
    show_intel_gpu_top_data()
    show_interrupt_data()
    show_frequency_data()
    show_temperature_data()
    
    print("\n\n🚀 Enhanced Monitoring Integration:")
    print("=" * 50)
    print("✅ All this data is automatically collected and displayed")
    print("✅ Real-time updates every 2 seconds")
    print("✅ Historical trends and sparklines")
    print("✅ Both terminal and graphical interfaces")
    print()
    print("🎯 Try the enhanced monitors:")
    print("   make real-power         # 6-panel graphical")
    print("   make real-power-terminal # Enhanced terminal")

if __name__ == "__main__":
    main()
