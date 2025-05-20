#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <linux/pci.h>

// Include the shared header file.
// Ensure your Makefile for this kernel module has the correct -I flag
// to find this header, e.g., EXTRA_CFLAGS += -I$(PWD)/../include
#include "gpu_proto.h" // This now refers to the gpu_proto_h_multigpu_final version

MODULE_LICENSE("GPL");
MODULE_AUTHOR("LKP Project Contributor");
MODULE_DESCRIPTION("GPU Information Netlink Kernel Module (Multiple GPUs, Real PCI Data)");

static struct sock *nl_sk = NULL; // Netlink socket structure

// Callback function to handle messages received from user-space
static void nl_recv_msg(struct sk_buff *skb) {
    struct nlmsghdr *nlh_recv;
    struct nlmsghdr *nlh_send;
    struct sk_buff *skb_out;
    struct all_gpus_info_packet *all_gpus_payload; // Payload for multiple GPUs
    int pid;
    int res;
    int msg_size_payload;

    struct pci_dev *pdev = NULL; // PCI device iterator
    int gpus_collected = 0;      // Counter for GPUs found
    unsigned char pci_base_class;
    unsigned int full_pci_class; // For debugging

    printk(KERN_INFO "GPU_LKM_MULTI_DEBUG: Netlink message received. Processing...\n");

    // 1. Receive and Validate the Incoming Message from User Space
    nlh_recv = (struct nlmsghdr *)skb->data;
    if (nlh_recv->nlmsg_len < NLMSG_HDRLEN || skb->len < nlh_recv->nlmsg_len) {
        printk(KERN_ERR "GPU_LKM_MULTI_DEBUG: Corrupted netlink message received.\n");
        return;
    }
    pid = nlh_recv->nlmsg_pid; // PID of the sending user-space process
    printk(KERN_INFO "GPU_LKM_MULTI_DEBUG: Message received from user-space PID %d.\n", pid);

    // 2. Prepare the Payload Structure for Sending Back
    msg_size_payload = sizeof(struct all_gpus_info_packet);

    skb_out = nlmsg_new(msg_size_payload, GFP_KERNEL); // Allocate skb
    if (!skb_out) {
        printk(KERN_ERR "GPU_LKM_MULTI_DEBUG: Failed to allocate new skb for outgoing message.\n");
        return;
    }

    nlh_send = nlmsg_put(skb_out, 0, nlh_recv->nlmsg_seq, NLMSG_DONE, msg_size_payload, 0);
    if (!nlh_send) {
        printk(KERN_ERR "GPU_LKM_MULTI_DEBUG: nlmsg_put failed for outgoing message.\n");
        kfree_skb(skb_out); // Free allocated skb
        return;
    }
    NETLINK_CB(skb_out).dst_group = 0; // Unicast
    all_gpus_payload = (struct all_gpus_info_packet *)NLMSG_DATA(nlh_send);
    memset(all_gpus_payload, 0, sizeof(struct all_gpus_info_packet)); // Important: Zero out payload

    // 3. Find GPU Information by Iterating Through PCI Devices
    printk(KERN_INFO "GPU_LKM_MULTI_DEBUG: Searching for Display Controller PCI devices...\n");

    for_each_pci_dev(pdev) {
        full_pci_class = pdev->class; // Get the full 24-bit class code
        pci_base_class = full_pci_class >> 16; // Get the base class (top byte of the 3-byte class code)
                                               // Note: pdev->class is u32, class code is 3 bytes.
                                               // PCI_CLASS_REVISION is the lowest byte.
                                               // So, (pdev->class >> 8) gives base class + subclass.
                                               // (pdev->class >> 16) gives just the base class.
                                               // Let's use (pdev->class >> 16) for base class as per common convention.
                                               // Or, more simply, the defined PCI_CLASS_DISPLAY_VGA etc. are already shifted.
                                               // The original pdev->class >> 8 was likely trying to get base+subclass.
                                               // Let's stick to the original pdev->class >> 8 for matching against PCI_CLASS_DISPLAY_*,
                                               // as those constants are defined as (BaseClass << 8 | SubClass).
                                               // For debugging, let's print the full class.

        pci_base_class = pdev->class >> 8; // Original logic for matching

        // --- ADDED DEBUG PRINT ---
        printk(KERN_INFO "GPU_LKM_MULTI_DEBUG: Found PCI Device: V=0x%04x D=0x%04x Class=0x%06x (BaseClass for check=0x%02X) Name: %s\n",
               pdev->vendor, pdev->device, full_pci_class, pci_base_class, pci_name(pdev));
        // --- END ADDED DEBUG PRINT ---


        if (gpus_collected >= MAX_GPUS_SUPPORTED) { // MAX_GPUS_SUPPORTED from gpu_proto.h
            printk(KERN_INFO "GPU_LKM_MULTI_DEBUG: Reached MAX_GPUS_SUPPORTED limit (%d).\n", MAX_GPUS_SUPPORTED);
            break;
        }

        // Check for various display controller types
        // The PCI_CLASS_DISPLAY_* constants are typically (BaseClass << 8 | SubClass)
        // So pdev->class >> 8 gives (BaseClass | SubClass >> 8) if class is 3 bytes.
        // Let's assume pdev->class holds the full 24-bit class code (class_device in lspci)
        // PCI_CLASS_DISPLAY_VGA is 0x0300.
        // If pdev->class is 0x030000, then pdev->class >> 8 is 0x0300. This is correct.

        if ((pdev->class >> 8) == PCI_CLASS_DISPLAY_VGA ||      // e.g., 0x0300
            (pdev->class >> 8) == PCI_CLASS_DISPLAY_XGA ||      // e.g., 0x0301
            (pdev->class >> 8) == PCI_CLASS_DISPLAY_3D  ||      // e.g., 0x0302
            (pdev->class >> 8) == PCI_CLASS_DISPLAY_OTHER) {    // e.g., 0x0380
            
            printk(KERN_INFO "GPU_LKM_MULTI_DEBUG: Matched as Display Controller (Class for check: 0x%02X): %s\n", (pdev->class >> 8), pci_name(pdev));
            
            all_gpus_payload->gpus[gpus_collected].vendor_id = pdev->vendor;
            all_gpus_payload->gpus[gpus_collected].device_id = pdev->device;
            all_gpus_payload->gpus[gpus_collected].bus = pdev->bus->number;
            all_gpus_payload->gpus[gpus_collected].slot = PCI_SLOT(pdev->devfn);
            all_gpus_payload->gpus[gpus_collected].function = PCI_FUNC(pdev->devfn);
            all_gpus_payload->gpus[gpus_collected].is_valid = 1; // Mark this GPU entry as valid
            
            gpus_collected++;
        }
    }
    all_gpus_payload->num_gpus_found = gpus_collected;

    if (gpus_collected == 0) {
        printk(KERN_WARNING "GPU_LKM_MULTI_DEBUG: No Display Controller PCI device matched the class checks.\n");
    }

    printk(KERN_INFO "GPU_LKM_MULTI_DEBUG: Sending info for %d GPU(s) to PID %d\n", gpus_collected, pid);

    // 4. Send the Message Back to User Space
    res = nlmsg_unicast(nl_sk, skb_out, pid);
    if (res < 0) {
        printk(KERN_INFO "GPU_LKM_MULTI_DEBUG: Error sending unicast message to PID %d: error code %d\n", pid, res);
    } else {
        printk(KERN_INFO "GPU_LKM_MULTI_DEBUG: Successfully sent GPU info to user-space PID %d\n", pid);
    }
}

// Module Initialization Function
static int __init gpu_lkm_init(void) {
    struct netlink_kernel_cfg cfg = {
        .input = nl_recv_msg,
    };
    printk(KERN_INFO "GPU_LKM_MULTI_DEBUG: Initializing Netlink GPU Module (Multi-GPU, Protocol ID: %d)\n", NETLINK_USER);
    nl_sk = netlink_kernel_create(&init_net, NETLINK_USER, &cfg); // NETLINK_USER from gpu_proto.h
    if (!nl_sk) {
        printk(KERN_ALERT "GPU_LKM_MULTI_DEBUG: Error creating netlink socket. Protocol %d might be in use.\n", NETLINK_USER);
        return -ENOMEM;
    }
    printk(KERN_INFO "GPU_LKM_MULTI_DEBUG: Netlink socket created successfully.\n");
    return 0;
}

// Module Exit Function
static void __exit gpu_lkm_exit(void) {
    printk(KERN_INFO "GPU_LKM_MULTI_DEBUG: Exiting Netlink GPU Module (Multi-GPU)\n");
    if (nl_sk != NULL) {
        netlink_kernel_release(nl_sk);
        nl_sk = NULL;
    }
    printk(KERN_INFO "GPU_LKM_MULTI_DEBUG: Netlink socket released.\n");
}

module_init(gpu_lkm_init);
module_exit(gpu_lkm_exit);
