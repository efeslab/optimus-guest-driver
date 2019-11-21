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
#include <asm/pgtable_types.h>
#include <asm/msr.h>
#include <linux/hugetlb.h>

//#include "vai/vai_internal.h"
#include "vai/vai_types.h"

static dev_t dev;
static struct cdev cdev;
static struct pci_dev *pdev;
static struct device *device;
static void __iomem *mmio_bar0;
static void __iomem *mmio_bar2;
static struct class *class;
resource_size_t bar0_start, bar0_end;
resource_size_t bar2_start, bar2_end;

#define PGSHIFT_4K 12
#define PGSHIFT_2M 21
#define PGSHIFT_1G 30

#define PGSIZE_4K (1UL << PGSHIFT_4K)
#define PGSIZE_2M (1UL << PGSHIFT_2M)
#define PGSIZE_1G (1UL << PGSHIFT_1G)

#define PGSIZE_FLAG_4K (1 << 0)
#define PGSIZE_FLAG_2M (1 << 1)
#define PGSIZE_FLAG_1G (1 << 2)

#define MAP_BEHAVISOR_MAP 0x0
#define MAP_BEHAVISOR_UNMAP 0x1

static int open_cnt;
DEFINE_MUTEX(open_cnt_lock);
DEFINE_MUTEX(io_lock);

struct hlist_head pinned_pages[12];

#define VAI_VENDOR_ID 0xbeef
#define VAI_DEVICE_ID 0xdead

struct vai_paging_notifier *paging_notifier;

static struct pci_device_id vai_pci_ids[] = {
    { PCI_DEVICE(VAI_VENDOR_ID, VAI_DEVICE_ID), },
    { 0, }
};
MODULE_DEVICE_TABLE(pci, vai_pci_ids);

#if 0
static uint32_t vai_read32_mmio(int offset)
{
    u32 ret;

    mutex_lock(&io_lock);
    ret = readl((uint8_t *)mmio + offset);
    mutex_unlock(&io_lock);

    return ret;
}
#endif

static uint64_t vai_read64_mmio(int offset)
{
    long ret;

    mutex_lock(&io_lock);
    ret = readq((uint8_t *)mmio_bar0 + offset);
    mutex_unlock(&io_lock);

    return ret;
}

static void vai_b1w32_mmio(int offset, uint32_t val)
{
    mutex_lock(&io_lock);
    writel(val, (uint8_t *)mmio_bar2 + offset);
    mutex_unlock(&io_lock);
}

static void vai_b1w64_mmio(int offset, uint64_t val)
{
    mutex_lock(&io_lock);
    writeq(val, (uint8_t *)mmio_bar2 + offset);
    mutex_unlock(&io_lock);
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

static long vai_ioctl_set_reset(void __user *arg);
static void vai_dma_unpin_all_pages(void);
static int vai_release(struct inode *in, struct file *f)
{

    vai_dma_unpin_all_pages();
    vai_ioctl_set_reset(NULL);

    printk("vai: release device\n");
    mutex_lock(&open_cnt_lock);
    open_cnt--;
    mutex_unlock(&open_cnt_lock);

    return 0;
}

static int vai_mmap(struct file *file, struct vm_area_struct *vma)
{
    uint64_t size = vma->vm_end - vma->vm_start;
    int ret;

    printk("vai: mmap vma start %lx size %llx\n", vma->vm_start, size);

    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
    ret = remap_pfn_range(vma, vma->vm_start, bar0_start >> PAGE_SHIFT,
            size, vma->vm_page_prot);

    printk("vai: mmap ret %d\n", ret);

    return ret;
}

static long vai_ioctl_get_id(void __user *arg)
{
    afu_id_t afu_id;
    uint64_t *ptr = (uint64_t*)&afu_id;
    int i;

    for (i=0 ; i<2; i++) {
        uint64_t x = vai_read64_mmio(VAI_ACCELERATOR_L + 8*i);
        *(ptr+i) = x;
        printk("vai: mmio read %x: %llx\n", 8*i, x);
    }

    if (copy_to_user(arg, &afu_id, sizeof(afu_id)))
        return -EFAULT;

    return 0;
}

struct pinned_page {
    uint64_t vfn;
    uint16_t pgsize_flag;
    struct page *page;
    struct hlist_node node;
};

struct fast_pin_notifier {
    uint32_t num_pages;
    uint16_t behavior;
    uint16_t pgsize_flag;
    uint64_t gva_start_addr;
    uint64_t gpas[0];
};

static void vai_dma_notify_page_map(uint64_t vfn, uint64_t pfn)
{
    paging_notifier->va = vfn << PAGE_SHIFT;
    paging_notifier->pa = pfn << PAGE_SHIFT;

    vai_b1w64_mmio(VAI_PAGING_NOTIFY_MAP, VAI_NOTIFY_DO_MAP);
}

static void vai_dma_notify_page_unmap(uint64_t vfn)
{
    paging_notifier->va = vfn << PAGE_SHIFT;

    vai_b1w64_mmio(VAI_PAGING_NOTIFY_MAP, VAI_NOTIFY_DO_UNMAP);
}

static uint64_t vai_check_page_size(struct vai_map_info *info)
{
    struct vm_area_struct *vma;
    uint64_t pgsize;
    uint64_t off;

    vma = find_vma(current->mm, info->user_addr);
    pgsize = vma_kernel_pagesize(vma);

    if (pgsize == PGSIZE_4K)
        return PGSIZE_4K;

    if (pgsize > info->length)
        return PGSIZE_4K;

    for (off = pgsize; off < info->length; off += pgsize) {
        struct vm_area_struct *vma_iter;

        vma_iter = find_vma(current->mm, info->user_addr + off);
        if (vma_iter == vma)
            continue;

        if (vma_kernel_pagesize(vma) == pgsize)
            vma = vma_iter;
        else
            return PGSIZE_4K;
    }

    return pgsize;
}

static long vai_dma_pin_pages_batch(struct vai_map_info *info, uint64_t pgsize)
{
    long npages, n_4k_pages;
    long i, off, iter;
    long pinned;
    long full_size;
    struct fast_pin_notifier *notifier = NULL;
    uint64_t notifier_pa;
    struct page **pages;
    int ret;
    int stride, shift, flag;

    if (!info)
        return -EFAULT;

    //printk("vai: map_info: start_addr=%#llx, len=%#llx\n", info->user_addr, info->length);

    if (pgsize == 0)
        pgsize = vai_check_page_size(info);

    //printk("vai: %s: checked pgsize == %#llx\n", __func__, pgsize);

    if (pgsize == PGSIZE_4K) {
        stride = 1;
        shift = PGSHIFT_4K;
        flag = PGSIZE_FLAG_4K;
    }
    else if (pgsize == PGSIZE_2M) {
        stride = 512;
        shift = PGSHIFT_2M;
        flag = PGSIZE_FLAG_2M;
    }
    else {
        stride = 512*512;
        shift = PGSHIFT_1G;
        flag = PGSIZE_FLAG_1G;
    }

    npages = info->length >> shift;
    n_4k_pages = info->length >> PAGE_SHIFT;

    full_size = sizeof(*notifier) + sizeof(uint64_t) * npages;
    notifier = kzalloc(full_size, GFP_KERNEL);
    if (!notifier) {
        printk("vai: %s: failed to alloc memory for notifier: size %ld\n", __func__, full_size);
        if (full_size <= PAGE_SIZE)
            return -ENOMEM;

#define DEFAULT_BATCH_SIZE 508
        for (off = 0; off < info->length; off += (pgsize * DEFAULT_BATCH_SIZE)) {
            struct vai_map_info new_info;

            new_info.user_addr = info->user_addr + off;
            new_info.length = (off + pgsize * DEFAULT_BATCH_SIZE <= info->length) ?
                        DEFAULT_BATCH_SIZE * pgsize : (info->length - off);
            ret = vai_dma_pin_pages_batch(&new_info, pgsize);
            if (ret)
                return ret;
        }

        return 0;
    }

    /* set the header of fast paging notifier */
    notifier->num_pages = npages;
    notifier->behavior = MAP_BEHAVISOR_MAP; /* 0 for map */
    notifier->pgsize_flag = (pgsize == PGSIZE_4K ? PGSIZE_FLAG_4K :
                                (pgsize == PGSIZE_2M ? PGSIZE_FLAG_2M : PGSIZE_FLAG_1G));
    notifier->gva_start_addr = info->user_addr;

    /*
    printk("vai: notifier: num_pages=%d, behavior=%x, pgsize_flag=%x, gva_start_addr=%#x\n",
                    notifier->num_pages,
                    notifier->behavior,
                    notifier->pgsize_flag,
                    notifier->gva_start_addr);
    */


    pages = vzalloc(sizeof(struct page *) * n_4k_pages);
    if (!pages) {
        printk("vai: %s: failed to alloc memory for page list: size %ld\n", __func__, 8*npages);
        return -ENOMEM;
    }

    /* pin the pages and get page struct */
    pinned = get_user_pages_fast(info->user_addr, n_4k_pages, 1, pages);
    if (pinned != n_4k_pages) {
        printk("vai: %s: cannot pin pages, expected %ld, pinned %ld\n",
                    __func__, npages, pinned);

        for (i = 0; i < pinned; i++) {
            put_page(pages[i]);
        }

        kfree(notifier);
        vfree(pages);

        return -EFAULT;
    }

    /* set gpa in notifier and add pinned pages to hash table */
    iter = 0;
    for (i = 0; i < n_4k_pages; i += stride) {
        struct pinned_page *pg = kzalloc(sizeof(*pg), GFP_KERNEL);
        uint64_t vfn = (info->user_addr >> PAGE_SHIFT) + i;

        /* set gpa */
        notifier->gpas[iter] = page_to_pfn(pages[i]) << PAGE_SHIFT;
        //printk("vai: va %#llx pa %#llx\n", vfn << PAGE_SHIFT, notifier->gpas[iter]);

        pg->vfn = vfn;
        pg->page = pages[i];
        pg->pgsize_flag = flag;
        hash_add(pinned_pages, &pg->node, vfn);

        iter++;
    }

    //printk("vai: added %d pages to notifier\n", iter);

    /* do the notification */
    notifier_pa = virt_to_phys(notifier);
    vai_b1w64_mmio(VAI_FAST_PAGING_MAP, notifier_pa);
    
    kfree(notifier);
    vfree(pages);

    return 0;
}

static void vai_dma_unpin_all_pages(void)
{
    struct hlist_node *tmp;
    struct pinned_page *p;
    int i;

    hash_for_each_safe(pinned_pages, i, tmp, p, node) {
        if (p->pgsize_flag == PGSIZE_FLAG_4K) {
            put_page(p->page);
        }
        else if (p->pgsize_flag == PGSIZE_FLAG_2M) {
            long offset;
            for (offset = 0; offset < 512; offset++) {
                put_page(p->page+offset);
            }
        }
        else {
            long offset;
            for (offset = 0; offset < 512*512; offset++) {
                put_page(p->page+offset);
            }
        }

        hash_del(&p->node);
        kfree(p);
    }
}

static long vai_dma_unpin_pages_batch(struct vai_map_info *info)
{
    long npages = info->length >> PAGE_SHIFT;
    struct pinned_page *tmp, *res = NULL;
    long i;
    struct fast_pin_notifier *notifier;
    uint64_t notifier_pa;
    uint64_t pgsize;

    pgsize = vai_check_page_size(info);

    for (i = 0; i < npages; i++) {
        uint64_t vfn = (info->user_addr >> PAGE_SHIFT) + i;

        hash_for_each_possible(pinned_pages, tmp, node, vfn) {
            if (vfn == tmp->vfn) {
                res = tmp;
                break;
            }
        }

        if (res) {
            hash_del(&res->node);

            if (res->pgsize_flag == PGSIZE_FLAG_4K) {
                put_page(res->page);
            }
            else if (res->pgsize_flag == PGSIZE_FLAG_2M) {
                long offset;
                for (offset = 0; offset < 512; offset++) {
                    put_page(res->page+offset);
                }
            }
            else {
                long offset;
                for (offset = 0; offset < 512*512; offset++) {
                    put_page(res->page+offset);
                }
            }

            kfree(res);
        }
    }

    notifier = kzalloc(sizeof(*notifier), GFP_KERNEL);
    if (!notifier) {
        printk("vai: %s: cannot alloc memory\n", __func__);
        return -ENOMEM;
    }
    notifier->num_pages = npages;
    notifier->behavior = MAP_BEHAVISOR_UNMAP;
    notifier->pgsize_flag = (pgsize == PGSIZE_4K ? PGSIZE_FLAG_4K :
                        (pgsize == PGSIZE_2M ? PGSIZE_FLAG_2M : PGSIZE_FLAG_1G));
    notifier->gva_start_addr = info->user_addr;
    notifier_pa = virt_to_phys(notifier);

    vai_b1w64_mmio(VAI_FAST_PAGING_MAP, notifier_pa);

    kfree(notifier);

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

    ret = vai_dma_pin_pages_batch(&info, 0);
    if (ret)
        return -EINVAL;

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

    ret = vai_dma_unpin_pages_batch(&info);
    if (ret)
        return -EINVAL;

    return 0;
}

static long vai_ioctl_set_mem_base(uint64_t mem_base)
{
    vai_b1w64_mmio(VAI_MEM_BASE, mem_base);
    printk("vai: set membase to %#llx\n", mem_base);
    return 0;
}

static long vai_ioctl_set_reset(void __user *arg)
{
    vai_b1w64_mmio(VAI_RESET, VAI_RESET_ENABLE);
    printk("vai: reset\n");
    return 0;
}

static long vai_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
    printk("vai: ioctl\n");

    switch (cmd) {
    case VAI_GET_AFU_ID:
        return vai_ioctl_get_id((void __user *)arg);
    case VAI_DMA_MAP_REGION:
        return vai_ioctl_dma_map_region((void __user *)arg);
    case VAI_DMA_UNMAP_REGION:
        return vai_ioctl_dma_unmap_region((void __user *)arg);
    case VAI_SET_MEM_BASE:
        return vai_ioctl_set_mem_base(arg);
    case VAI_SET_RESET:
        return vai_ioctl_set_reset((void __user*)arg);
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

static void vai_initialize_paging_notifier(void)
{
    void *va = kzalloc(sizeof(*paging_notifier), GFP_KERNEL);
    u64 pa = virt_to_phys(va);

    paging_notifier = va;

    printk("vai: pg notifier va %llx pa %llx\n", (u64)va, pa);

    vai_b1w64_mmio(VAI_PAGING_NOTIFY_MAP_ADDR, (uint64_t)pa);
}

static int vai_pci_probe(struct pci_dev *pcidev, const struct pci_device_id *pcidevid)
{
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

    mmio_bar0 = pci_iomap(pcidev, 0, pci_resource_len(pcidev, 0));
    mmio_bar2 = pci_iomap(pcidev, 2, pci_resource_len(pcidev, 2));

    bar0_start = pci_resource_start(pcidev, 0);
    bar0_end = pci_resource_end(pcidev, 0);
    bar2_start = pci_resource_start(pcidev, 2);
    bar2_end = pci_resource_end(pcidev, 2);
    pr_info("bar 0 virtual start %llx\n", (uint64_t)mmio_bar0);
    pr_info("bar0 start %llx, bar0 end %llx\n", bar0_start, bar0_end);
    pr_info("length %llx\n", (unsigned long long)(bar0_end + 1 - bar0_start));
    pr_info("bar2 virtual start %llx\n", (uint64_t)mmio_bar2);
    pr_info("bar2 start %llx, bar0 end %llx\n", bar2_start, bar2_end);
    pr_info("length %llx\n", (unsigned long long)(bar2_end + 1 - bar2_start));

    device = device_create(class, NULL, dev, NULL, "vai");
    if (IS_ERR(device)) {
        pr_err("vai: error in creating device\n");
        goto err_release_region;
    }

    vai_initialize_paging_notifier();

    return 0;

err_release_region:
    pci_release_region(pcidev, 0);
err_cdev_del:
    cdev_del(&cdev);
err_unregister_chrdev:
    unregister_chrdev_region(dev, 1);
err:
    pci_disable_device(pcidev);
    return 1;
}

static void vai_pci_remove(struct pci_dev *pcidev)
{
    printk("vai: pci remove\n");

    device_unregister(device);
    pci_release_region(pcidev, 0);
    cdev_del(&cdev);
    unregister_chrdev_region(dev, 1);
    pci_disable_device(pcidev);
}

static struct pci_driver vai_pci_driver = {
    .name = "vai-pci",
    .id_table = vai_pci_ids,
    .probe = vai_pci_probe,
    .remove = vai_pci_remove
};

static int vai_init(void)
{
    class = class_create(THIS_MODULE, "accel");
    if (IS_ERR(class)) {
        pr_err("vai: failed to create class\n");
        return 1;
    }

    if (pci_register_driver(&vai_pci_driver) < 0) {
        return 1;
    }

    return 0;
}

static void vai_exit(void)
{
    pci_unregister_driver(&vai_pci_driver);
    class_destroy(class);
}


module_init(vai_init);
module_exit(vai_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jiacheng Ma <jcma@umich.edu>");
MODULE_DESCRIPTION("Guest Accelerator Driver for Optimus FPGA Hypervisor");
