#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <linux/pci.h>
#include <linux/init.h>

#include "vai_internal.h"
#include "vai_types.h"

static int major;
static struct pci_dev *pdev;
static void __iomem *mmio;
static struct class *class;

#define VAI_VENDOR_ID 0xdead
#define VAI_DEVICE_ID 0xbeef

static struct pci_device_id vai_pci_ids[] = {
	{ PCI_DEVICE(VAI_VENDOR_ID, VAI_DEVICE_ID), },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, vai_pci_ids);

static int vai_open(struct inode *in, struct file *f)
{
    printk("vai: open device\n");
    return 0;
}

static int vai_release(struct inode *in, struct file *f)
{
    printk("vai: release device\n");
    return 0;
}

static long vai_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
    printk("vai: ioctl\n");


    return 0;
}

static struct file_operations vai_fops = {
    .owner = THIS_MODULE,
    .open = vai_open,
    .release = vai_release,
    .unlocked_ioctl = vai_ioctl
};

static int vai_pci_probe(struct pci_dev *pcidev, const struct pci_device_id *pcidevid)
{
    struct device *dev;

    dev_info(&(pcidev->dev), "pci_probe\n");
    major = register_chrdev(0, "vai-pci", &vai_fops);
    pdev = pcidev;

    if (pci_enable_device(pcidev) < 0) {
        dev_err(&pcidev->dev, "pci_enable_device\n");
        goto error;
    }

    if (pci_request_region(pcidev, 0, "vai_pci_region_0")) {
        dev_err(&pcidev->dev, "pci_request_region\n");
        goto error;
    }

    mmio = pci_iomap(pcidev, 0, pci_resource_len(pcidev, 0));

    class = class_create(THIS_MODULE, "accel");
    if (IS_ERR(class)) {
        pci_release_region(pcidev, 0);
        unregister_chrdev(major, "vai-pci");
        goto error;
    }

    dev = device_create(class, NULL, major, NULL, "vai");
    if (IS_ERR(dev)) {
        class_destroy(class);
        pci_release_region(pcidev, 0);
        unregister_chrdev(major, "vai-pci");
        goto error;
    }

    return 0;

error:
    return 1;
}

static void vai_pci_remove(struct pci_dev *pcidev)
{
    device_destroy(class, major);
    class_destroy(class);
    pci_release_region(pcidev, 0);
    unregister_chrdev(major, "vai-pci");
}

static struct pci_driver vai_pci_driver = {
    .name = "vai-pci",
    .id_table = vai_pci_ids,
    .probe = vai_pci_probe,
    .remove = vai_pci_remove
};

static int vai_init(void)
{
    if (pci_register_driver(&vai_pci_driver) < 0) {
        return 1;
    }
    return 0;
}

static void vai_exit(void)
{
    pci_unregister_driver(&vai_pci_driver);
}


module_init(vai_init);
module_exit(vai_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jiacheng Ma <jcma@umich.edu>");
MODULE_DESCRIPTION("VAI Guest Deiver");
