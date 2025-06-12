#!/usr/bin/env python3
"""
Enhanced GPU Monitoring Demo
Demonstrates the new power and interrupt rate monitoring features
"""
import subprocess
import time
import os

def print_header():
    """Print demo header"""
    print("🔋 Enhanced Intel GPU Monitoring Demo")
    print("=" * 60)
    print("This demo shows the new power and interrupt rate monitoring")
    print("capabilities added to the real Intel GPU monitoring system.")
    print()

def check_prerequisites():
    """Check if required tools are available"""
    print("🔍 Checking prerequisites...")
    
    # Check intel_gpu_top
    try:
        subprocess.run(['intel_gpu_top', '--help'], capture_output=True, timeout=2)
        print("✅ intel_gpu_top: Available")
    except:
        print("❌ intel_gpu_top: Not found")
        print("   Install with: sudo apt install intel-gpu-tools")
        return False
    
    # Check proc interfaces
    if os.path.exists('/proc/interrupts'):
        print("✅ /proc/interrupts: Available")
    else:
        print("❌ /proc/interrupts: Not found")
        return False
    
    if os.path.exists('/sys/class/drm'):
        print("✅ /sys/class/drm: Available")
    else:
        print("❌ /sys/class/drm: Not found")
        return False
    
    print()
    return True

def show_monitoring_comparison():
    """Show the difference between old and new monitoring"""
    print("📊 Monitoring Capabilities Comparison:")
    print()
    
    print("📈 Original Monitoring (4 metrics):")
    print("   🌡️  Temperature")
    print("   ⚡ GPU Utilization") 
    print("   💾 Memory Usage")
    print("   🚀 GPU Frequency")
    print()
    
    print("🔋 Enhanced Monitoring (6 metrics):")
    print("   🌡️  Temperature")
    print("   ⚡ GPU Utilization")
    print("   💾 Memory Usage") 
    print("   🚀 GPU Frequency")
    print("   🔋 Power Consumption (NEW!)")
    print("   📡 Interrupt Rate (NEW!)")
    print()

def demo_power_monitoring():
    """Demonstrate power monitoring"""
    print("🔋 Power Monitoring Demo:")
    print("   • Reads GPU power from Intel PMU")
    print("   • Shows both GPU and Package power")
    print("   • Tracks power efficiency trends")
    print("   • Useful for battery life optimization")
    print()

def demo_interrupt_monitoring():
    """Demonstrate interrupt monitoring"""
    print("📡 Interrupt Rate Monitoring Demo:")
    print("   • Reads i915 driver interrupts from /proc/interrupts")
    print("   • Calculates interrupt rate (IRQ/s)")
    print("   • Helps identify GPU driver activity")
    print("   • Useful for performance debugging")
    print()

def run_enhanced_terminal_demo():
    """Run the enhanced terminal monitor briefly"""
    print("🖥️  Starting Enhanced Terminal Monitor Demo...")
    print("   (Running for 15 seconds to show new features)")
    print()
    
    try:
        subprocess.run(['timeout', '15s', 'python3', 'real_intel_terminal.py'], 
                      cwd='/home/cselab3/gpU-VIewer')
        print("\n✅ Enhanced terminal demo completed!")
    except Exception as e:
        print(f"❌ Demo failed: {e}")

def show_data_sources():
    """Show where the new data comes from"""
    print("📡 Enhanced Data Sources:")
    print()
    print("Power Consumption:")
    print("   • Source: intel_gpu_top JSON output")
    print("   • Fields: power.GPU, power.Package")
    print("   • Units: Watts (W)")
    print("   • Update: Every 2 seconds")
    print()
    print("Interrupt Rate:")
    print("   • Source: /proc/interrupts")
    print("   • Driver: i915 (Intel GPU)")
    print("   • Calculation: IRQ count delta / time delta")
    print("   • Units: Interrupts per second (IRQ/s)")
    print()

def show_usage_examples():
    """Show how to use the enhanced monitoring"""
    print("🚀 Usage Examples:")
    print()
    print("Enhanced Graphical Monitor (6 panels):")
    print("   make real-power")
    print()
    print("Enhanced Terminal Monitor (6 metrics):")
    print("   make real-power-terminal")
    print()
    print("Help with all options:")
    print("   make help")
    print()

def main():
    """Main demo function"""
    print_header()
    
    if not check_prerequisites():
        print("❌ Prerequisites not met. Please install required tools.")
        return
    
    show_monitoring_comparison()
    demo_power_monitoring() 
    demo_interrupt_monitoring()
    show_data_sources()
    show_usage_examples()
    
    print("🎮 Would you like to see a live demo? (y/n): ", end="")
    choice = input().lower().strip()
    
    if choice in ['y', 'yes']:
        run_enhanced_terminal_demo()
        print()
        print("🔋 Enhanced monitoring demo complete!")
        print("🚀 Try the full enhanced monitor with: make real-power")
    else:
        print("👋 Demo complete! Try: make real-power or make real-power-terminal")

if __name__ == "__main__":
    main()
