#!/usr/bin/env python3
"""
GPU Engine Usage Breakdown Demo
Shows GPU utilization by engine type (3D, Video, Blitter, etc.)
"""
import subprocess
import json
import time

def show_gpu_engine_breakdown():
    """Show GPU usage broken down by engine type"""
    print("📊 Intel GPU Engine Usage Breakdown")
    print("=" * 50)
    print()
    
    print("🔍 GPU Engines Explained:")
    print("   🎮 Render/3D (RCS): 3D graphics, games, desktop effects")
    print("   🎬 Video (VCS): Hardware video decode/encode")
    print("   📋 Blitter (BCS): 2D operations, memory copies") 
    print("   ✨ Video Enhance (VECS): Video scaling, effects")
    print()
    
    try:
        # Get real-time engine data
        result = subprocess.run(['sudo', 'intel_gpu_top', '-s', '2000', '-J'], 
                              capture_output=True, text=True, timeout=5)
        
        if result.returncode == 0:
            lines = result.stdout.strip().split('\n')
            for line in lines:
                if line.startswith('{'):
                    try:
                        data = json.loads(line)
                        if data.get('period', {}).get('duration', 0) > 1500:
                            engines = data.get('engines', {})
                            
                            print("📈 Current Engine Utilization:")
                            total_activity = 0
                            
                            # Show each engine
                            for engine_name, engine_data in engines.items():
                                busy = engine_data.get('busy', 0)
                                total_activity = max(total_activity, busy)
                                
                                # Create bar visualization
                                bar_length = 20
                                filled = int((busy / 100) * bar_length)
                                bar = '█' * filled + '░' * (bar_length - filled)
                                
                                # Add emoji based on engine type
                                if 'Render' in engine_name or '3D' in engine_name:
                                    emoji = "🎮"
                                elif 'Video' in engine_name and 'Enhance' not in engine_name:
                                    emoji = "🎬"
                                elif 'Blitter' in engine_name:
                                    emoji = "📋"
                                elif 'VideoEnhance' in engine_name or 'VECS' in engine_name:
                                    emoji = "✨"
                                else:
                                    emoji = "⚙️"
                                
                                print(f"   {emoji} {engine_name:12}: {busy:5.1f}% {bar}")
                            
                            print()
                            print(f"📊 Overall GPU Activity: {total_activity:.1f}%")
                            
                            # Show what this means
                            print()
                            print("💡 What This Means:")
                            if total_activity < 1:
                                print("   • GPU is mostly idle (normal for desktop)")
                                print("   • Power saving mode active")
                            elif total_activity < 10:
                                print("   • Light desktop activity (window effects, etc.)")
                                print("   • Normal for typical desktop use")
                            elif total_activity < 50:
                                print("   • Moderate GPU workload")
                                print("   • Could be video playback or light gaming")
                            else:
                                print("   • Heavy GPU workload")
                                print("   • Gaming, video editing, or compute tasks")
                            
                            break
                    except json.JSONDecodeError:
                        continue
        else:
            print("❌ Could not get GPU engine data")
            
    except Exception as e:
        print(f"❌ Error: {e}")

def show_video_vs_3d_comparison():
    """Compare video vs 3D GPU usage"""
    print("\n\n🎬 Video vs 🎮 3D GPU Usage")
    print("=" * 50)
    print()
    
    print("🎬 Video Engine Usage (VCS):")
    print("   • Hardware video decoding (YouTube, Netflix)")
    print("   • Hardware video encoding (streaming, recording)")
    print("   • Minimal power usage")
    print("   • Offloads CPU for video tasks")
    print()
    
    print("🎮 Render/3D Engine Usage (RCS):")
    print("   • 3D graphics rendering (games)")
    print("   • Desktop compositing (window effects)")
    print("   • GPGPU compute workloads")
    print("   • Higher power usage")
    print()
    
    print("📋 Blitter Engine Usage (BCS):")
    print("   • 2D graphics operations")
    print("   • Memory-to-memory copies")
    print("   • GUI rendering")
    print("   • Very low power usage")

def demonstrate_workloads():
    """Show different workload examples"""
    print("\n\n⚡ GPU Workload Examples")
    print("=" * 50)
    print()
    
    print("To see VIDEO engine usage:")
    print("   firefox youtube.com  # Hardware video decode")
    print("   mpv --hwdec=auto video.mp4")
    print("   obs-studio  # Video encoding")
    print()
    
    print("To see 3D/RENDER engine usage:")
    print("   glxgears  # Simple OpenGL test")
    print("   steam games  # 3D gaming")
    print("   blender  # 3D rendering")
    print()
    
    print("To see BLITTER engine usage:")
    print("   Moving windows around")
    print("   Image editing software")
    print("   GUI applications")

def main():
    """Main demonstration"""
    print("🖥️ Intel GPU Engine Usage Analysis")
    print("=" * 60)
    print("Understanding what GPU usage means - it's NOT just video!")
    print()
    
    show_gpu_engine_breakdown()
    show_video_vs_3d_comparison()
    demonstrate_workloads()
    
    print("\n\n🎯 Summary:")
    print("✅ GPU usage combines ALL engines (3D + Video + Blitter)")
    print("✅ Most desktop activity uses 3D/Render engine")
    print("✅ Video engine only active during hardware video decode/encode")
    print("✅ 0-10% usage is normal for typical desktop work")
    print()
    print("🚀 Try our enhanced monitoring:")
    print("   make real-power-terminal  # See all metrics")

if __name__ == "__main__":
    main()
