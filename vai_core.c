#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <uapi/asm/kvm_para.h>

#include "vai_internal.h"
#include "vai_types.h"

static dev_t vai_dev;
static struct cdev vai_cdev;
static struct class *vai_class;

#define FIRST_MINOR 0
#define MINOR_CNT 1

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

static int __init vai_init(void)
{
    int ret;
    struct device *dev_ret;

    if ((ret = alloc_chrdev_region(&vai_dev, FIRST_MINOR, MINOR_CNT, "vai_ioctl")) < 0) {
        return ret;
    }

    cdev_init(&vai_cdev, &vai_fops);

    if ((ret = cdev_add(&vai_cdev, vai_dev, MINOR_CNT)) < 0) {
        return ret;
    }

    if (IS_ERR(vai_class = class_create(THIS_MODULE, "vai"))) {
        cdev_del(&vai_cdev);
        unregister_chrdev_region(vai_dev, MINOR_CNT);
        return PTR_ERR(vai_class);
    }

    if (IS_ERR(dev_ret = device_create(vai_class, NULL, vai_dev, NULL, "vafu"))) {
        class_destroy(vai_class);
        cdev_del(&vai_cdev);
        unregister_chrdev_region(vai_dev, MINOR_CNT);
        return PTR_ERR(dev_ret);
    }

    return 0;
}

static void __exit vai_exit(void)
{
    device_destroy(vai_class, vai_dev);
    class_destroy(vai_class);
    cdev_del(&vai_cdev);
    unregister_chrdev_region(vai_dev, MINOR_CNT);
}

module_init(vai_init);
module_exit(vai_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jiacheng Ma <jcma@umich.edu>");
MODULE_DESCRIPTION("VAI Guest Deiver");
