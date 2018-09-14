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
#include <linux/mutex.h>
#include <linux/hashtable.h>
#include <linux/uaccess.h>
#include <linux/mm.h>

#include "vai_internal.h"
#include "vai_types.h"

static dev_t dev;
static struct cdev cdev;
static struct pci_dev *pdev;
static void __iomem *mmio;
static struct class *class;
resource_size_t bar0_start;
resource_size_t bar0_end;

static int open_cnt;
DEFINE_MUTEX(open_cnt_lock);
DEFINE_MUTEX(io_lock);

struct hlist_head pinned_pages[12];

#define VAI_VENDOR_ID 0xdead
#define VAI_DEVICE_ID 0xbeef

static struct pci_device_id vai_pci_ids[] = {
	{ PCI_DEVICE(VAI_VENDOR_ID, VAI_DEVICE_ID), },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, vai_pci_ids);

static long vai_read_mmio(int offset)
{
    long ret;

    mutex_lock(&io_lock);
    ret = readq((uint8_t *)mmio + offset);
    mutex_unlock(&io_lock);

    return ret;
}

static int vai_open(struct inode *in, struct file *f)
{
    printk("vai: open device\n");

    mutex_lock(&open_cnt_lock);
    if (open_cnt > 0) {
        mutex_unlock(&open_cnt_lock);
        goto err_occupied;
    }
    open_cnt++;
    mutex_unlock(&open_cnt_lock);

    return 0;

err_occupied:
    printk("vai: open failed, already occupied\n");
    return -EFAULT;
}

static int vai_release(struct inode *in, struct file *f)
{
    printk("vai: release device\n");

    mutex_lock(&open_cnt_lock);
    open_cnt--;
    mutex_unlock(&open_cnt_lock);

    return 0;
}

static int vai_mmap(struct file *file, struct vm_area_struct *vma)
{
    uint64_t size = vma->vm_end - vma->vm_start;

    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
    return remap_pfn_range(vma, vma->vm_start, bar0_start >> PAGE_SHIFT,
            size, vma->vm_page_prot);

    return 0;
}

static long vai_ioctl_get_version(void __user *arg)
{
    struct vai_afu_version version;
    uint64_t *ptr = (uint64_t*)&version;
    int i;

    for (i=0 ; i<4; i++) {
        uint64_t x = vai_read_mmio(8*i);
        *(ptr+i) = x;
        printk("vai: mmio read %x: %llx\n", 8*i, x);
    }

    if (copy_to_user(arg, &version, sizeof(version)))
        return -EFAULT;

    return 0;
}

struct pinned_page {
    uint64_t gfn;
    struct page *page;
    struct hlist_node node;
};

static long vai_dma_pin_pages(struct vai_map_info *info)
{
    long npages, i;
    long pinned, all_pinned=0;
    struct pinned_page *tmp;

    if (!info)
        return -EFAULT;

    npages = info->length >> PAGE_SHIFT;

    for (i=0; i<npages; i++) {
        struct pinned_page *pg = kzalloc(sizeof(*pg), GFP_KERNEL);
        uint64_t gfn = (info->user_addr >> PAGE_SHIFT) + i;

        pinned = get_user_pages_fast(info->user_addr+i*PAGE_SIZE, 1, 1, &pg->page);
        if (pinned != 1)
            goto err;

        pg->gfn = gfn;
        all_pinned += pinned;
        hash_add(pinned_pages, &pg->node, gfn);
    }

    return all_pinned;

err:
    for (i=0; i<all_pinned; i++) {
        uint64_t gfn = (info->user_addr >> PAGE_SHIFT) + i;

        hash_for_each_possible(pinned_pages, tmp, node, gfn) {
            if (gfn == tmp->gfn) {
                put_page(tmp->page);
            }
        }
    }

    return -EFAULT;
}

static long vai_dma_unpin_pages(struct vai_map_info *info)
{
    long npages = info->length >> PAGE_SHIFT;
    struct pinned_page *tmp;
    long i;

    for (i=0; i<npages; i++) {
        uint64_t gfn = (info->user_addr >> PAGE_SHIFT) + i;

        hash_for_each_possible(pinned_pages, tmp, node, gfn) {
            if (gfn == tmp->gfn) {
                put_page(tmp->page);
            }
        }
    }

    return 0;
}

static long vai_page_table_map_pages(struct vai_map_info *info)
{
    /* currently do not support page table */
    return 0;
}

static long vai_page_table_unmap_pages(struct vai_map_info *info)
{
    return 0;
}
       
static long vai_ioctl_dma_map_region(void __user *arg)
{
    struct vai_map_info info;
    long ret;
    uint64_t user_addr, length;

    if (copy_from_user(&info, arg, sizeof(info)))
        return -EFAULT;

    user_addr = info.user_addr;
    length = info.length;

    if (!PAGE_ALIGNED(user_addr) || !PAGE_ALIGNED(length) || !length)
        return -EINVAL;

    if (user_addr + length < user_addr)
        return -EINVAL;

    ret = vai_dma_pin_pages(&info);
    if (ret)
        return -EINVAL;

    vai_page_table_map_pages(&info);

    return 0;
}

static long vai_ioctl_dma_unmap_region(void __user *arg)
{
    struct vai_map_info info;
    long ret;
    uint64_t user_addr, length;

    if (copy_from_user(&info, arg, sizeof(info)))
        return -EFAULT;

    user_addr = info.user_addr;
    length = info.length;

    if (!PAGE_ALIGNED(user_addr) || !PAGE_ALIGNED(length) || !length)
        return -EINVAL;

    if (user_addr + length < user_addr)
        return -EINVAL;

    ret = vai_dma_unpin_pages(&info);
    if (ret)
        return -EINVAL;

    vai_page_table_unmap_pages(&info);

    return 0;
}

static long vai_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
    printk("vai: ioctl\n");

    switch (cmd) {
    case VAI_GET_AFU_VERSION:
        return vai_ioctl_get_version((void __user *)arg);
    case VAI_DMA_MAP_REGION:
        return vai_ioctl_dma_map_region((void __user *)arg);
    case VAI_DMA_UNMAP_REGION:
        return vai_ioctl_dma_unmap_region((void __user *)arg);
    }

    return -EINVAL;
}

static struct file_operations vai_fops = {
    .owner = THIS_MODULE,
    .open = vai_open,
    .release = vai_release,
    .unlocked_ioctl = vai_ioctl,
    .mmap = vai_mmap
};

static int vai_pci_probe(struct pci_dev *pcidev, const struct pci_device_id *pcidevid)
{
    struct device *device;

    hash_init(pinned_pages);

    dev_info(&(pcidev->dev), "pci_probe\n");

    if (alloc_chrdev_region(&dev, 0, 1, "vai-pci")) {
        dev_err(&pcidev->dev, "pci_enable_device\n");
        goto err;
    }

    cdev_init(&cdev, &vai_fops);
    if (cdev_add(&cdev, dev, 1)) {
        goto err_cdev_del;
    }    

    pdev = pcidev;

    if (pci_enable_device(pcidev) < 0) {
        dev_err(&pcidev->dev, "pci_enable_device\n");
        goto err;
    }

    if (pci_request_region(pcidev, 0, "vai_pci_region_0")) {
        dev_err(&pcidev->dev, "pci_request_region\n");
        goto err_unregister_chrdev;
    }

    mmio = pci_iomap(pcidev, 0, pci_resource_len(pcidev, 0));

    bar0_start = pci_resource_start(pcidev, 0);
    bar0_end = pci_resource_end(pcidev, 0);
    pr_info("length %llx\n", (unsigned long long)(bar0_end + 1 - bar0_start));

    class = class_create(THIS_MODULE, "accel");
    if (IS_ERR(class))
        goto err_release_region;

    device = device_create(class, NULL, dev, NULL, "vai");
    if (IS_ERR(device))
        goto err_destroy_class;

    return 0;

err_destroy_class:
    class_destroy(class);
err_release_region:
    pci_release_region(pcidev, 0);
err_cdev_del:
    cdev_del(&cdev);
err_unregister_chrdev:
    unregister_chrdev_region(dev, 1);
err:
    return 1;
}

static void vai_pci_remove(struct pci_dev *pcidev)
{
    class_destroy(class);
    pci_release_region(pcidev, 0);
    cdev_del(&cdev);
    unregister_chrdev_region(dev, 1);
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