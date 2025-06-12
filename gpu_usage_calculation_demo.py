#!/usr/bin/env python3
"""
GPU Usage Calculation Demo
Shows the difference between the old (max) and new (sum) calculation methods
"""
import subprocess
import json
import time

def demo_calculation_methods():
    """Demonstrate old vs new GPU usage calculation"""
    print("🔍 GPU Usage Calculation Method Comparison")
    print("=" * 60)
    print()
    
    print("📊 Analyzing real Intel GPU engine data...")
    print()
    
    try:
        # Get real intel_gpu_top data
        result = subprocess.run(['sudo', 'intel_gpu_top', '-s', '2000', '-J'], 
                              capture_output=True, text=True, timeout=5)
        
        if result.returncode == 0:
            lines = result.stdout.strip().split('\n')
            for line in lines:
                if line.startswith('{'):
                    try:
                        data = json.loads(line)
                        if data.get('period', {}).get('duration', 0) > 500:
                            engines = data.get('engines', {})
                            
                            print("🎯 Engine Breakdown:")
                            total_sum = 0
                            max_single = 0
                            
                            for engine_name, engine_data in engines.items():
                                util = engine_data.get('busy', 0)
                                total_sum += util
                                max_single = max(max_single, util)
                                print(f"   {engine_name:15}: {util:6.2f}%")
                            
                            print()
                            print("📈 Calculation Results:")
                            print(f"   🔴 OLD Method (max):  {max_single:6.2f}% (highest single engine)")
                            print(f"   🟢 NEW Method (sum):  {total_sum:6.2f}% (all engines combined)")
                            print()
                            
                            improvement = total_sum - max_single
                            if improvement > 0.1:
                                print(f"✅ Improvement: +{improvement:.2f}% more accurate total utilization")
                            else:
                                print("ℹ️  Similar results (low GPU activity)")
                            
                            break
                    except json.JSONDecodeError:
                        continue
    except Exception as e:
        print(f"❌ Error getting intel_gpu_top data: {e}")
        print()
        
        # Show theoretical example
        print("📚 Theoretical Example:")
        print("   Render/3D Engine:  8.5%")
        print("   Video Engine:      2.1%")
        print("   Blitter Engine:    1.2%")
        print("   VideoEnhance:      0.3%")
        print()
        print("📈 Calculation Results:")
        print("   🔴 OLD Method (max):  8.5% (only Render engine)")
        print("   🟢 NEW Method (sum): 12.1% (all engines combined)")
        print("   ✅ Improvement: +3.6% more accurate total utilization")

def explain_why_sum_is_better():
    """Explain why summing is more accurate"""
    print("\n\n🧠 Why Sum is More Accurate")
    print("=" * 60)
    print()
    
    print("🎮 Real-world GPU Scenarios:")
    print("   • Gaming: Render (60%) + Video (5%) + Blitter (2%) = 67% total")
    print("   • Video editing: Video (45%) + Render (20%) + Blitter (8%) = 73% total")
    print("   • Desktop work: Render (5%) + Blitter (3%) + Video (1%) = 9% total")
    print()
    
    print("🔴 OLD Method Problems:")
    print("   • Only shows highest single engine")
    print("   • Underreports actual GPU usage")
    print("   • Misses multi-engine workloads")
    print("   • Gaming might show 60% instead of 67%")
    print()
    
    print("🟢 NEW Method Benefits:")
    print("   • Shows true total GPU utilization")
    print("   • Accounts for all active engines")
    print("   • More accurate power usage correlation")
    print("   • Better workload understanding")

def main():
    """Main demonstration"""
    print("🖥️ Intel GPU Usage Calculation Enhancement")
    print("Understanding the improved GPU utilization algorithm")
    print()
    
    demo_calculation_methods()
    explain_why_sum_is_better()
    
    print("\n\n🚀 Try Enhanced Monitoring:")
    print("   make real-power-terminal  # See improved calculations")
    print("   make real-power          # Graphical with new algorithm")

if __name__ == "__main__":
    main()
