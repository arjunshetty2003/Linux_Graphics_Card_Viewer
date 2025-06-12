#!/usr/bin/env python3
"""
GPU Load Simulator
Simulates GPU load to generate changing data for real-time graphs
"""
import time
import random
import subprocess
import os

def simulate_gpu_activity():
    """Simulate GPU activity by running some computations"""
    print("🚀 Starting GPU load simulation...")
    print("This will run some computations to generate changing GPU metrics")
    print("Press Ctrl+C to stop simulation")
    
    try:
        # Simple CPU-intensive task to simulate load
        # This won't actually load the GPU but will create some system activity
        while True:
            # Simulate different workload patterns
            workload_type = random.choice(['light', 'medium', 'heavy', 'idle'])
            
            if workload_type == 'heavy':
                print("⚡ Heavy workload simulation...")
                # Simulate heavy computation
                for _ in range(1000000):
                    _ = random.random() ** 2
                time.sleep(2)
                
            elif workload_type == 'medium':
                print("⚡ Medium workload simulation...")
                # Simulate medium computation
                for _ in range(500000):
                    _ = random.random() ** 2
                time.sleep(1)
                
            elif workload_type == 'light':
                print("⚡ Light workload simulation...")
                # Simulate light computation
                for _ in range(100000):
                    _ = random.random() ** 2
                time.sleep(0.5)
                
            else:  # idle
                print("😴 Idle period...")
                time.sleep(3)
                
    except KeyboardInterrupt:
        print("\n🛑 GPU load simulation stopped")

def main():
    print("GPU Load Simulator")
    print("==================")
    print("This script simulates GPU activity to demonstrate real-time monitoring.")
    print("Run this in one terminal while running the graph monitors in another.")
    print()
    
    simulate_gpu_activity()

if __name__ == "__main__":
    main()
