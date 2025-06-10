#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("OpenAI ChatGPT");
MODULE_DESCRIPTION("GPU Viewer Kernel Module");
MODULE_VERSION("0.1");

static struct proc_dir_entry *proc_entry;

// Helper: Read PCI BAR size
static u64 read_bar_size(struct pci_dev *dev, int bar)
{
u32 orig, size;
int offset = PCI_BASE_ADDRESS_0 + 4 * bar;
if (pci_read_config_dword(dev, offset, &orig) != PCIBIOS_SUCCESSFUL)
    return 0;

pci_write_config_dword(dev, offset, 0xFFFFFFFF);
if (pci_read_config_dword(dev, offset, &size) != PCIBIOS_SUCCESSFUL) {
    pci_write_config_dword(dev, offset, orig);
    return 0;
}

pci_write_config_dword(dev, offset, orig);

if (!size || size == 0xFFFFFFFF)
    return 0;

return ~(size & PCI_BASE_ADDRESS_MEM_MASK) + 1;
}

static int gpu_viewer_proc_show(struct seq_file *m, void *v)
{
struct pci_dev *pdev = NULL;
seq_puts(m, "=== GPU Viewer ===\n");

for_each_pci_dev(pdev) {
    u16 vendor, device;
    u32 class_code;

    vendor = pdev->vendor;
    device = pdev->device;
    class_code = pdev->class;

    // Filter for Display class (0x03xxxx)
    if ((class_code >> 16) != PCI_BASE_CLASS_DISPLAY)
        continue;

    // GPU Identification
    seq_printf(m, "\nBus: %02x:%02x.%x\n",
               pdev->bus->number,
               PCI_SLOT(pdev->devfn),
               PCI_FUNC(pdev->devfn));

    seq_printf(m, "Vendor ID: 0x%04x\n", vendor);
    seq_printf(m, "Device ID: 0x%04x\n", device);
    seq_printf(m, "Class: 0x%06x\n", class_code >> 8);

    if (pdev->driver && pdev->driver->name)
        seq_printf(m, "Driver: %s\n", pdev->driver->name);
    else
        seq_printf(m, "Driver: unknown\n");

    // Estimate VRAM from BAR5
    u64 vram_bytes = read_bar_size(pdev, 5);
    if (vram_bytes)
        seq_printf(m, "VRAM: %llu MB\n", vram_bytes >> 20);
    else
        seq_printf(m, "VRAM: unknown\n");

    // PCIe Link Width/Speed (basic check)
    u16 link_status;
    int pos = pci_find_capability(pdev, PCI_CAP_ID_EXP);
    if (pos) {
        pci_read_config_word(pdev, pos + 0x12, &link_status);
        int speed = link_status & 0xF;
        int width = (link_status >> 4) & 0x3F;
        seq_printf(m, "PCIe Link: Gen%d x%d\n", speed, width);
    } else {
        seq_puts(m, "PCIe Link: unavailable\n");
    }

    seq_puts(m, "Temp: N/A (sysfs)\n");
    seq_puts(m, "Power: N/A (sysfs)\n");
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
if (!proc_entry) {
pr_err("gpu_viewer: Failed to create /proc entry\n");
return -ENOMEM;
}

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


