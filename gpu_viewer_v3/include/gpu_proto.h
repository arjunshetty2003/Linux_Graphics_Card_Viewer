#ifndef GPU_PROTO_H
#define GPU_PROTO_H

// Define a value for NETLINK_USER
// This is often 31 for custom protocols, but could be different for your project.
#define NETLINK_USER 31

// Define a maximum payload size for netlink messages
// Adjust this value if your actual GPU information packets are larger.
#define MAX_PAYLOAD 1024 // Example size, can be adjusted

/*
 * This structure defines the format of the GPU information packet
 * that is expected to be sent from the kernel module to the user-space application.
 *
 * IMPORTANT: The members and their types (e.g., vendor_id, device_id)
 * MUST exactly match the structure definition used in your kernel module
 * that is sending this information. Any mismatch can lead to data corruption
 * or crashes.
 */
struct gpu_info_packet {
    unsigned short vendor_id;   // PCI Vendor ID
    unsigned short device_id;   // PCI Device ID
    unsigned char bus;          // PCI Bus number
    unsigned char slot;         // PCI Slot (Device) number
    unsigned char function;     // PCI Function number
    // Add any other fields that your kernel module sends
    // For example:
    // unsigned long memory_total;
    // unsigned long memory_used;
    // unsigned int core_clock_mhz;
    // unsigned int memory_clock_mhz;
    // char gpu_name[64];
};

#endif // GPU_PROTO_H
