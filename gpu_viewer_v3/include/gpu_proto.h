#ifndef GPU_PROTO_H
#define GPU_PROTO_H

// Define a value for NETLINK_USER
// This is often 31 for custom protocols.
#define NETLINK_USER 31

// Maximum number of GPUs the module will report in a single message
#define MAX_GPUS_SUPPORTED 4 // You can adjust this if needed

// Structure for a single GPU's PCI identification information
struct gpu_info_packet {
    unsigned short vendor_id;   // PCI Vendor ID
    unsigned short device_id;   // PCI Device ID
    unsigned char bus;          // PCI Bus number
    unsigned char slot;         // PCI Slot (Device) number
    unsigned char function;     // PCI Function number
    unsigned char is_valid;     // 1 if this entry contains valid GPU data, 0 otherwise
};

// Structure to hold information for multiple GPUs
struct all_gpus_info_packet {
    int num_gpus_found; // Actual number of GPUs found and populated in the 'gpus' array
    struct gpu_info_packet gpus[MAX_GPUS_SUPPORTED];
};

// Define a maximum payload size for netlink messages
// This needs to be large enough for struct all_gpus_info_packet plus Netlink headers.
// NLMSG_HDRLEN is ~16 bytes. Add some buffer.
#define MAX_PAYLOAD (sizeof(struct all_gpus_info_packet) + NLMSG_HDRLEN + 128)

#endif // GPU_PROTO_H
