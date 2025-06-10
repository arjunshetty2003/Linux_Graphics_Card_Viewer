#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/of.h>
#include <linux/kobject.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ChatGPT");
MODULE_DESCRIPTION("Extended GPU Viewer Module");
MODULE_VERSION("0.4");

static struct proc_dir_entry *proc_entry;

static u64 read_bar_size(struct pci_dev *dev, int bar)
{
    u32 old = pci_read_config_dword(dev, PCI_BASE_ADDRESS_0 + 4 * bar);
    pci_write_config_dword(dev, PCI_BASE_ADDRESS_0 + 4 * bar, 0xFFFFFFFF);
    u32 size = pci_read_config_dword(dev, PCI_BASE_ADDRESS_0 + 4 * bar);
    pci_write_config_dword(dev, PCI_BASE_ADDRESS_0 + 4 * bar, old);
    if (!size)
        return 0;
    return ~(size & PCI_BASE_ADDRESS_MEM_MASK) + 1;
}

// Helper to show GPU info
static int gpu_viewer_proc_show(struct seq_file *m, void *v)
{
    struct pci_dev *pdev = NULL;
    seq_printf(m, "=== GPU Viewer ===\n");
    for_each_pci_dev(pdev) {
        u16 vendor, device;
        u32 class;
        pci_read_config_word(pdev, PCI_VENDOR_ID, &vendor);
        pci_read_config_word(pdev, PCI_DEVICE_ID, &device);
        class = pdev->class;
        if (((class >> 16) & 0xff) != PCI_BASE_CLASS_DISPLAY)
            continue;

        seq_printf(m, "\nBus: %02x:%02x.%x\n",
                   pdev->bus->number, PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));
        seq_printf(m, "Vendor ID: 0x%04x Device ID: 0x%04x\n", vendor, device);
        seq_printf(m, "Class: 0x%06x\n", class >> 8);
        seq_printf(m, "Driver: %s\n", pdev->driver ? pdev->driver->name : "unknown");

        u64 bar5 = read_bar_size(pdev, 5);
        seq_printf(m, "VRAM: %llu MB\n", bar5 >> 20);

        u16 linkstat = 0;
        pci_read_config_word(pdev, 0x72, &linkstat);
        int gen = linkstat & 0xF;
        int width = (linkstat >> 4) & 0x3F;
        seq_printf(m, "PCIe Link: Gen%d x%d\n", gen, width);

        // Attempt sysfs temperature/power
        char path[128];
        snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s/hwmon", pci_name(pdev));
        seq_printf(m, "Temp: N/A (sysfs)\nPower: N/A (sysfs)\n");
    }
    return 0;
}

static int gpu_viewer_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, gpu_viewer_proc_show, NULL);
}

static const struct proc_ops gpu_viewer_proc_ops = {
    .proc_open = gpu_viewer_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static int __init gpu_viewer_init(void)
{
    proc_entry = proc_create("gpu_viewer", 0, NULL, &gpu_viewer_proc_ops);
    if (!proc_entry)
        return -ENOMEM;
    pr_info("gpu_viewer module loaded\n");
    return 0;
}

static void __exit gpu_viewer_exit(void)
{
    proc_remove(proc_entry);
    pr_info("gpu_viewer module unloaded\n");
}

module_init(gpu_viewer_init);
module_exit(gpu_viewer_exit);
