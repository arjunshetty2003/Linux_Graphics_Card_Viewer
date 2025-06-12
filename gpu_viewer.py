#!/usr/bin/env python3
"""
Advanced GPU Hardware Monitor Viewer
Reads real-time data from the GPU Monitor kernel module
Supports multiple GPUs with dynamic discovery
"""
import sys
import time
import threading
from collections import deque, defaultdict
from datetime import datetime
import tkinter as tk
from tkinter import ttk, messagebox
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.animation import FuncAnimation
import numpy as np

class GPUMonitorReader:
    def __init__(self, proc_file="/proc/gpu_monitor"):
        self.proc_file = proc_file
        self.gpu_count = 0
        self.gpu_data = {}
        
    def read_gpu_data(self):
        """Read GPU data from kernel module"""
        try:
            with open(self.proc_file, 'r') as f:
                lines = f.readlines()
            
            data = {'global': {}, 'gpus': {}}
            
            for line in lines:
                line = line.strip()
                if not line or ':' not in line:
                    continue
                    
                key, value = line.split(':', 1)
                value = value.strip()
                
                # Global parameters
                if key in ['GPU_COUNT', 'LAST_UPDATE', 'DATA_SOURCE', 'MODULE_VERSION']:
                    data['global'][key] = value
                    if key == 'GPU_COUNT':
                        self.gpu_count = int(value)
                
                # GPU-specific parameters
                elif key.startswith('GPU_'):
                    parts = key.split('_', 2)
                    if len(parts) >= 3:
                        gpu_id = int(parts[1])
                        param_name = '_'.join(parts[2:])
                        
                        if gpu_id not in data['gpus']:
                            data['gpus'][gpu_id] = {}
                        
                        # Convert numeric values
                        try:
                            if param_name in ['VENDOR_ID', 'DEVICE_ID']:
                                data['gpus'][gpu_id][param_name] = int(value, 16)
                            elif param_name in ['MEMORY_USED', 'MEMORY_TOTAL', 'TEMPERATURE',
                                              'CLOCK_CORE', 'CLOCK_MEMORY', 'POWER_USAGE',
                                              'FAN_SPEED', 'UTILIZATION_GPU', 'UTILIZATION_MEMORY']:
                                data['gpus'][gpu_id][param_name] = float(value)
                            else:
                                data['gpus'][gpu_id][param_name] = value
                        except (ValueError, TypeError):
                            data['gpus'][gpu_id][param_name] = value
            
            return data
            
        except FileNotFoundError:
            print(f"Error: {self.proc_file} not found. Is the GPU monitor kernel module loaded?")
            return None
        except PermissionError:
            print(f"Error: Permission denied reading {self.proc_file}. Try running as root.")
            return None
        except Exception as e:
            print(f"Error reading GPU data: {e}")
            return None

class GPUMonitorGUI:
    def __init__(self):
        self.root = tk.Tk()
        self.root.title("Advanced GPU Hardware Monitor")
        self.root.geometry("1200x800")
        
        self.reader = GPUMonitorReader()
        self.running = False
        self.update_thread = None
        
        # Data storage for graphs
        self.history_length = 100
        self.gpu_history = defaultdict(lambda: {
            'temperature': deque(maxlen=self.history_length),
            'utilization_gpu': deque(maxlen=self.history_length),
            'utilization_memory': deque(maxlen=self.history_length),
            'power_usage': deque(maxlen=self.history_length),
            'memory_used': deque(maxlen=self.history_length),
            'timestamps': deque(maxlen=self.history_length)
        })
        
        self.setup_ui()
        self.start_monitoring()
        
    def setup_ui(self):
        """Create the main UI"""
        # Create main frame
        main_frame = ttk.Frame(self.root)
        main_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # Control panel
        control_frame = ttk.LabelFrame(main_frame, text="Control Panel")
        control_frame.pack(fill=tk.X, pady=(0, 10))
        
        self.start_button = ttk.Button(control_frame, text="Start Monitoring", 
                                     command=self.toggle_monitoring)
        self.start_button.pack(side=tk.LEFT, padx=5, pady=5)
        
        self.refresh_rate_var = tk.StringVar(value="1.0")
        ttk.Label(control_frame, text="Refresh Rate (s):").pack(side=tk.LEFT, padx=(20, 5), pady=5)
        refresh_spin = ttk.Spinbox(control_frame, from_=0.1, to=10.0, increment=0.1,
                                 textvariable=self.refresh_rate_var, width=10)
        refresh_spin.pack(side=tk.LEFT, padx=5, pady=5)
        
        # Status label
        self.status_var = tk.StringVar(value="Initializing...")
        status_label = ttk.Label(control_frame, textvariable=self.status_var)
        status_label.pack(side=tk.RIGHT, padx=5, pady=5)
        
        # Create notebook for tabs
        self.notebook = ttk.Notebook(main_frame)
        self.notebook.pack(fill=tk.BOTH, expand=True)
        
        # Overview tab
        self.overview_frame = ttk.Frame(self.notebook)
        self.notebook.add(self.overview_frame, text="Overview")
        self.setup_overview_tab()
        
        # Graphs tab
        self.graphs_frame = ttk.Frame(self.notebook)
        self.notebook.add(self.graphs_frame, text="Performance Graphs")
        self.setup_graphs_tab()
        
        # Details tab
        self.details_frame = ttk.Frame(self.notebook)
        self.notebook.add(self.details_frame, text="Detailed Info")
        self.setup_details_tab()
        
    def setup_overview_tab(self):
        """Setup the overview tab with GPU cards"""
        # Scrollable frame for GPU cards
        canvas = tk.Canvas(self.overview_frame)
        scrollbar = ttk.Scrollbar(self.overview_frame, orient="vertical", command=canvas.yview)
        self.scrollable_frame = ttk.Frame(canvas)
        
        self.scrollable_frame.bind(
            "<Configure>",
            lambda e: canvas.configure(scrollregion=canvas.bbox("all"))
        )
        
        canvas.create_window((0, 0), window=self.scrollable_frame, anchor="nw")
        canvas.configure(yscrollcommand=scrollbar.set)
        
        canvas.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")
        
        self.gpu_cards = {}
        
    def setup_graphs_tab(self):
        """Setup the performance graphs tab"""
        # Create matplotlib figure
        self.fig, self.axes = plt.subplots(2, 2, figsize=(12, 8))
        self.fig.tight_layout(pad=3.0)
        
        # Temperature plot
        self.temp_ax = self.axes[0, 0]
        self.temp_ax.set_title('Temperature (°C)')
        self.temp_ax.set_ylabel('Temperature')
        self.temp_ax.grid(True, alpha=0.3)
        
        # GPU Utilization plot
        self.gpu_util_ax = self.axes[0, 1]
        self.gpu_util_ax.set_title('GPU Utilization (%)')
        self.gpu_util_ax.set_ylabel('Utilization')
        self.gpu_util_ax.set_ylim(0, 100)
        self.gpu_util_ax.grid(True, alpha=0.3)
        
        # Memory Utilization plot
        self.mem_util_ax = self.axes[1, 0]
        self.mem_util_ax.set_title('Memory Usage (MB)')
        self.mem_util_ax.set_ylabel('Memory')
        self.mem_util_ax.grid(True, alpha=0.3)
        
        # Power Usage plot
        self.power_ax = self.axes[1, 1]
        self.power_ax.set_title('Power Usage (W)')
        self.power_ax.set_ylabel('Power')
        self.power_ax.grid(True, alpha=0.3)
        
        # Embed in tkinter
        self.canvas = FigureCanvasTkAgg(self.fig, self.graphs_frame)
        self.canvas.draw()
        self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)
        
    def setup_details_tab(self):
        """Setup the detailed information tab"""
        # Create treeview for detailed data
        self.details_tree = ttk.Treeview(self.details_frame, columns=('Value',), show='tree headings')
        self.details_tree.heading('#0', text='Parameter')
        self.details_tree.heading('Value', text='Value')
        self.details_tree.column('#0', width=300)
        self.details_tree.column('Value', width=200)
        
        # Scrollbars for treeview
        tree_scrollbar_y = ttk.Scrollbar(self.details_frame, orient="vertical", command=self.details_tree.yview)
        tree_scrollbar_x = ttk.Scrollbar(self.details_frame, orient="horizontal", command=self.details_tree.xview)
        self.details_tree.configure(yscrollcommand=tree_scrollbar_y.set, xscrollcommand=tree_scrollbar_x.set)
        
        self.details_tree.pack(side="left", fill="both", expand=True)
        tree_scrollbar_y.pack(side="right", fill="y")
        tree_scrollbar_x.pack(side="bottom", fill="x")
        
    def create_gpu_card(self, gpu_id, gpu_data):
        """Create a card widget for a GPU"""
        card_frame = ttk.LabelFrame(self.scrollable_frame, text=f"GPU {gpu_id}")
        card_frame.pack(fill=tk.X, padx=5, pady=5)
        
        # Create grid for GPU info
        info_frame = ttk.Frame(card_frame)
        info_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # GPU name and basic info
        name = gpu_data.get('NAME', 'Unknown GPU')
        ttk.Label(info_frame, text=name, font=('TkDefaultFont', 10, 'bold')).grid(
            row=0, column=0, columnspan=4, sticky=tk.W, pady=(0, 10))
        
        # Create info labels
        labels = {}
        
        # Temperature
        ttk.Label(info_frame, text="Temperature:").grid(row=1, column=0, sticky=tk.W, padx=(0, 5))
        labels['temp'] = ttk.Label(info_frame, text="-- °C")
        labels['temp'].grid(row=1, column=1, sticky=tk.W, padx=(0, 20))
        
        # GPU Utilization
        ttk.Label(info_frame, text="GPU Usage:").grid(row=1, column=2, sticky=tk.W, padx=(0, 5))
        labels['gpu_util'] = ttk.Label(info_frame, text="-- %")
        labels['gpu_util'].grid(row=1, column=3, sticky=tk.W)
        
        # Memory
        ttk.Label(info_frame, text="Memory:").grid(row=2, column=0, sticky=tk.W, padx=(0, 5))
        labels['memory'] = ttk.Label(info_frame, text="-- / -- MB")
        labels['memory'].grid(row=2, column=1, sticky=tk.W, padx=(0, 20))
        
        # Power
        ttk.Label(info_frame, text="Power:").grid(row=2, column=2, sticky=tk.W, padx=(0, 5))
        labels['power'] = ttk.Label(info_frame, text="-- W")
        labels['power'].grid(row=2, column=3, sticky=tk.W)
        
        # Clock speeds
        ttk.Label(info_frame, text="Core Clock:").grid(row=3, column=0, sticky=tk.W, padx=(0, 5))
        labels['core_clock'] = ttk.Label(info_frame, text="-- MHz")
        labels['core_clock'].grid(row=3, column=1, sticky=tk.W, padx=(0, 20))
        
        ttk.Label(info_frame, text="Memory Clock:").grid(row=3, column=2, sticky=tk.W, padx=(0, 5))
        labels['mem_clock'] = ttk.Label(info_frame, text="-- MHz")
        labels['mem_clock'].grid(row=3, column=3, sticky=tk.W)
        
        # Progress bars
        progress_frame = ttk.Frame(card_frame)
        progress_frame.pack(fill=tk.X, padx=10, pady=(0, 10))
        
        # GPU utilization progress bar
        ttk.Label(progress_frame, text="GPU:").pack(anchor=tk.W)
        labels['gpu_progress'] = ttk.Progressbar(progress_frame, maximum=100)
        labels['gpu_progress'].pack(fill=tk.X, pady=(2, 5))
        
        # Memory utilization progress bar
        ttk.Label(progress_frame, text="Memory:").pack(anchor=tk.W)
        labels['mem_progress'] = ttk.Progressbar(progress_frame, maximum=100)
        labels['mem_progress'].pack(fill=tk.X, pady=(2, 0))
        
        self.gpu_cards[gpu_id] = {
            'frame': card_frame,
            'labels': labels
        }
        
    def update_gpu_card(self, gpu_id, gpu_data):
        """Update GPU card with new data"""
        if gpu_id not in self.gpu_cards:
            self.create_gpu_card(gpu_id, gpu_data)
        
        labels = self.gpu_cards[gpu_id]['labels']
        
        # Update temperature
        temp = gpu_data.get('TEMPERATURE', 0)
        labels['temp'].config(text=f"{temp:.1f} °C")
        
        # Update GPU utilization
        gpu_util = gpu_data.get('UTILIZATION_GPU', 0)
        labels['gpu_util'].config(text=f"{gpu_util:.1f} %")
        labels['gpu_progress']['value'] = gpu_util
        
        # Update memory
        mem_used = gpu_data.get('MEMORY_USED', 0)
        mem_total = gpu_data.get('MEMORY_TOTAL', 1)
        mem_percent = (mem_used / mem_total * 100) if mem_total > 0 else 0
        labels['memory'].config(text=f"{mem_used:.0f} / {mem_total:.0f} MB")
        labels['mem_progress']['value'] = mem_percent
        
        # Update power
        power = gpu_data.get('POWER_USAGE', 0)
        labels['power'].config(text=f"{power:.1f} W")
        
        # Update clocks
        core_clock = gpu_data.get('CLOCK_CORE', 0)
        mem_clock = gpu_data.get('CLOCK_MEMORY', 0)
        labels['core_clock'].config(text=f"{core_clock:.0f} MHz")
        labels['mem_clock'].config(text=f"{mem_clock:.0f} MHz")
        
    def update_graphs(self):
        """Update performance graphs"""
        if not self.gpu_history:
            return
            
        # Clear all axes
        for ax in self.axes.flat:
            ax.clear()
        
        # Redefine axes properties
        self.temp_ax = self.axes[0, 0]
        self.temp_ax.set_title('Temperature (°C)')
        self.temp_ax.set_ylabel('Temperature')
        self.temp_ax.grid(True, alpha=0.3)
        
        self.gpu_util_ax = self.axes[0, 1]
        self.gpu_util_ax.set_title('GPU Utilization (%)')
        self.gpu_util_ax.set_ylabel('Utilization')
        self.gpu_util_ax.set_ylim(0, 100)
        self.gpu_util_ax.grid(True, alpha=0.3)
        
        self.mem_util_ax = self.axes[1, 0]
        self.mem_util_ax.set_title('Memory Usage (MB)')
        self.mem_util_ax.set_ylabel('Memory')
        self.mem_util_ax.grid(True, alpha=0.3)
        
        self.power_ax = self.axes[1, 1]
        self.power_ax.set_title('Power Usage (W)')
        self.power_ax.set_ylabel('Power')
        self.power_ax.grid(True, alpha=0.3)
        
        colors = ['blue', 'red', 'green', 'orange', 'purple', 'brown']
        
        for i, (gpu_id, history) in enumerate(self.gpu_history.items()):
            if not history['timestamps']:
                continue
                
            color = colors[i % len(colors)]
            label = f'GPU {gpu_id}'
            
            # Convert timestamps to relative seconds
            if history['timestamps']:
                base_time = history['timestamps'][0]
                x_data = [(t - base_time).total_seconds() for t in history['timestamps']]
            else:
                x_data = []
            
            # Plot temperature
            if history['temperature']:
                self.temp_ax.plot(x_data, list(history['temperature']), 
                                color=color, label=label, linewidth=2)
            
            # Plot GPU utilization
            if history['utilization_gpu']:
                self.gpu_util_ax.plot(x_data, list(history['utilization_gpu']), 
                                    color=color, label=label, linewidth=2)
            
            # Plot memory usage
            if history['memory_used']:
                self.mem_util_ax.plot(x_data, list(history['memory_used']), 
                                    color=color, label=label, linewidth=2)
            
            # Plot power usage
            if history['power_usage']:
                self.power_ax.plot(x_data, list(history['power_usage']), 
                                 color=color, label=label, linewidth=2)
        
        # Add legends if there's data
        if self.gpu_history:
            for ax in self.axes.flat:
                ax.legend(loc='upper right', fontsize=8)
        
        self.fig.tight_layout(pad=3.0)
        self.canvas.draw()
        
    def update_details_tree(self, data):
        """Update the detailed information tree"""
        # Clear existing items
        for item in self.details_tree.get_children():
            self.details_tree.delete(item)
        
        if not data:
            return
        
        # Add global information
        global_node = self.details_tree.insert('', 'end', text='Global Information', values=('',))
        
        for key, value in data.get('global', {}).items():
            self.details_tree.insert(global_node, 'end', text=key, values=(str(value),))
        
        # Add GPU information
        for gpu_id, gpu_data in data.get('gpus', {}).items():
            gpu_node = self.details_tree.insert('', 'end', text=f'GPU {gpu_id}', values=('',))
            
            for key, value in gpu_data.items():
                if isinstance(value, float):
                    formatted_value = f"{value:.2f}"
                else:
                    formatted_value = str(value)
                self.details_tree.insert(gpu_node, 'end', text=key, values=(formatted_value,))
        
        # Expand all nodes
        for item in self.details_tree.get_children():
            self.details_tree.item(item, open=True)
    
    def update_data(self):
        """Update all data and UI elements"""
        data = self.reader.read_gpu_data()
        
        if data is None:
            self.status_var.set("Error: Cannot read GPU data")
            return
        
        gpu_count = len(data.get('gpus', {}))
        self.status_var.set(f"Monitoring {gpu_count} GPU(s) - Last update: {datetime.now().strftime('%H:%M:%S')}")
        
        # Update GPU cards
        for gpu_id, gpu_data in data.get('gpus', {}).items():
            self.update_gpu_card(gpu_id, gpu_data)
            
            # Store history for graphs
            now = datetime.now()
            history = self.gpu_history[gpu_id]
            history['timestamps'].append(now)
            history['temperature'].append(gpu_data.get('TEMPERATURE', 0))
            history['utilization_gpu'].append(gpu_data.get('UTILIZATION_GPU', 0))
            history['utilization_memory'].append(gpu_data.get('UTILIZATION_MEMORY', 0))
            history['power_usage'].append(gpu_data.get('POWER_USAGE', 0))
            history['memory_used'].append(gpu_data.get('MEMORY_USED', 0))
        
        # Update graphs
        self.update_graphs()
        
        # Update details tree
        self.update_details_tree(data)
    
    def monitoring_loop(self):
        """Background monitoring loop"""
        while self.running:
            try:
                self.root.after(0, self.update_data)
                time.sleep(float(self.refresh_rate_var.get()))
            except Exception as e:
                print(f"Error in monitoring loop: {e}")
                self.root.after(0, lambda: self.status_var.set(f"Error: {e}"))
                time.sleep(1)
    
    def toggle_monitoring(self):
        """Start or stop monitoring"""
        if self.running:
            self.stop_monitoring()
        else:
            self.start_monitoring()
    
    def start_monitoring(self):
        """Start monitoring thread"""
        if not self.running:
            self.running = True
            self.update_thread = threading.Thread(target=self.monitoring_loop, daemon=True)
            self.update_thread.start()
            self.start_button.config(text="Stop Monitoring")
            self.status_var.set("Starting monitoring...")
    
    def stop_monitoring(self):
        """Stop monitoring thread"""
        self.running = False
        if self.update_thread:
            self.update_thread.join(timeout=2)
        self.start_button.config(text="Start Monitoring")
        self.status_var.set("Monitoring stopped")
    
    def on_closing(self):
        """Handle window closing"""
        self.stop_monitoring()
        self.root.destroy()
    
    def run(self):
        """Run the GUI application"""
        self.root.protocol("WM_DELETE_WINDOW", self.on_closing)
        
        # Initial data load
        self.update_data()
        
        self.root.mainloop()

def main():
    """Main entry point"""
    if len(sys.argv) > 1:
        proc_file = sys.argv[1]
    else:
        proc_file = "/proc/gpu_monitor"
    
    try:
        app = GPUMonitorGUI()
        print("Starting Advanced GPU Hardware Monitor...")
        print(f"Reading from: {proc_file}")
        print("Close the window or press Ctrl+C to exit")
        app.run()
    except KeyboardInterrupt:
        print("\nMonitoring stopped by user")
    except Exception as e:
        print(f"Error starting application: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()