#include <linux/module.h>    // Core header for loading LKMs into the kernel
#include <linux/kernel.h>    // Contains types, macros, functions for the kernel
#include <linux/init.h>      // Macros to mark up functions e.g. __init __exit
#include <linux/netlink.h>   // For netlink sockets
#include <linux/skbuff.h>    // For socket buffer (sk_buff) structures
#include <net/sock.h>        // For struct sock, netlink_kernel_create, sock_release
#include <linux/pci.h>       // For PCI device iteration and access

// Include the shared header file.
// The Makefile should specify the correct include path (e.g., -I../include).
// This file defines NETLINK_USER, MAX_PAYLOAD, and struct gpu_info_packet.
#include "gpu_proto.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name / LKP Student");
MODULE_DESCRIPTION("GPU Information Netlink Kernel Module (Real Data Attempt)");

// Static variable to hold the netlink socket structure
static struct sock *nl_sk = NULL;

// Function to handle messages received from user-space via netlink
static void nl_recv_msg(struct sk_buff *skb) {
    struct nlmsghdr *nlh_recv;         // Netlink message header for received message
    struct nlmsghdr *nlh_send;         // Netlink message header for sending message
    struct sk_buff *skb_out;           // Socket buffer for outgoing message
    struct gpu_info_packet *gpu_data_payload; // Payload structure
    int pid;                           // Process ID of the sending user-space application
    int res;                           // Result of netlink send operation
    int msg_size;                      // Size of the payload

    // PCI related variables
    struct pci_dev *pdev = NULL;
    int found_gpu = 0;
    unsigned short vendor_id = 0, device_id = 0;
    unsigned char bus_num = 0, dev_num = 0, func_num = 0;


    printk(KERN_INFO "GPU_LKM_REAL: Entering %s\n", __FUNCTION__);

    // 1. Receive and Process the Incoming Message
    nlh_recv = (struct nlmsghdr *)skb->data;
    if (nlh_recv->nlmsg_len < NLMSG_HDRLEN || skb->len < nlh_recv->nlmsg_len) {
        printk(KERN_ERR "GPU_LKM_REAL: Corrupted netlink message received (length: %u, skb_len: %u).\n", nlh_recv->nlmsg_len, skb->len);
        return;
    }
    pid = nlh_recv->nlmsg_pid; /* PID of sending process */
    printk(KERN_INFO "GPU_LKM_REAL: Message received from PID %d.\n", pid);


    // 2. Find GPU Information using PCI iteration
    printk(KERN_INFO "GPU_LKM_REAL: Searching for VGA compatible PCI devices...\n");
    // Iterate over all PCI devices
    // pci_get_device(VENDOR_ANY, DEVICE_ANY, from)
    // For VGA compatible: pci_get_class(PCI_CLASS_DISPLAY_VGA << 8, from)
    for_each_pci_dev(pdev) {
        // Check if the device is a VGA compatible controller or 3D controller
        if (pdev->class == (PCI_CLASS_DISPLAY_VGA << 8) || pdev->class == (PCI_CLASS_DISPLAY_3D << 8)) {
            printk(KERN_INFO "GPU_LKM_REAL: Found a VGA/3D compatible device: %s\n", pci_name(pdev));
            vendor_id = pdev->vendor;
            device_id = pdev->device;
            bus_num = pdev->bus->number;
            dev_num = PCI_SLOT(pdev->devfn);   // Device (Slot) number
            func_num = PCI_FUNC(pdev->devfn);  // Function number

            found_gpu = 1;
            // pci_dev_put(pdev); // Decrement reference count if we got it with pci_get_device or similar
            // for_each_pci_dev does not increment the refcount, so no put needed here for 'pdev' from the loop itself.
            break; // Found one, use this for the demo
        }
    }

    if (!found_gpu) {
        printk(KERN_WARNING "GPU_LKM_REAL: No VGA compatible PCI device found.\n");
        // Send a response indicating no GPU found or send dummy data
        // For this example, we'll send 0s if no GPU is found
        vendor_id = 0x0000;
        device_id = 0x0000;
        bus_num = 0;
        dev_num = 0;
        func_num = 0;
    }

    // 3. Prepare the GPU Information Packet to Send Back
    msg_size = sizeof(struct gpu_info_packet);

    skb_out = nlmsg_new(msg_size, GFP_KERNEL);
    if (!skb_out) {
        printk(KERN_ERR "GPU_LKM_REAL: Failed to allocate new skb for outgoing message\n");
        return;
    }

    nlh_send = nlmsg_put(skb_out, 0, nlh_recv->nlmsg_seq, NLMSG_DONE, msg_size, 0);
    if (!nlh_send) {
        printk(KERN_ERR "GPU_LKM_REAL: nlmsg_put failed for outgoing message\n");
        kfree_skb(skb_out);
        return;
    }
    NETLINK_CB(skb_out).dst_group = 0;

    gpu_data_payload = (struct gpu_info_packet *)NLMSG_DATA(nlh_send);

    // Populate with found (or default) data
    gpu_data_payload->vendor_id = vendor_id;
    gpu_data_payload->device_id = device_id;
    gpu_data_payload->bus = bus_num;
    gpu_data_payload->slot = dev_num;
    gpu_data_payload->function = func_num;

    printk(KERN_INFO "GPU_LKM_REAL: Sending GPU info: V=0x%04x D=0x%04x B=%02u S=%02u F=%02u to PID %d\n",
           gpu_data_payload->vendor_id, gpu_data_payload->device_id, gpu_data_payload->bus,
           gpu_data_payload->slot, gpu_data_payload->function, pid);

    // 4. Send the Message Back to User-Space
    res = nlmsg_unicast(nl_sk, skb_out, pid);
    if (res < 0) {
        printk(KERN_INFO "GPU_LKM_REAL: Error while sending unicast message to PID %d: error %d\n", pid, res);
    } else {
        printk(KERN_INFO "GPU_LKM_REAL: Successfully sent GPU info to PID %d\n", pid);
    }
}

// Module Initialization Function
static int __init gpu_lkm_init(void) {
    struct netlink_kernel_cfg cfg = {
        .input = nl_recv_msg,
    };

    printk(KERN_INFO "GPU_LKM_REAL: Initializing Netlink GPU Module (Protocol: %d) for Real Data\n", NETLINK_USER);

    nl_sk = netlink_kernel_create(&init_net, NETLINK_USER, &cfg);
    if (!nl_sk) {
        printk(KERN_ALERT "GPU_LKM_REAL: Error creating netlink socket. Protocol %d might be in use or invalid.\n", NETLINK_USER);
        return -ENOMEM;
    }
    printk(KERN_INFO "GPU_LKM_REAL: Netlink socket created successfully.\n");

    return 0;
}

// Module Exit Function
static void __exit gpu_lkm_exit(void) {
    printk(KERN_INFO "GPU_LKM_REAL: Exiting Netlink GPU Module (Real Data)\n");
    if (nl_sk != NULL) {
        netlink_kernel_release(nl_sk);
        nl_sk = NULL;
    }
    printk(KERN_INFO "GPU_LKM_REAL: Netlink socket released.\n");
}

module_init(gpu_lkm_init);
module_exit(gpu_lkm_exit);
