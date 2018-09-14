#ifndef _VAI_INTERNAL_
#define _VAI_INTERNAL_

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>

struct vai_dma_region {
    u64 user_addr;
    u64 length;
    struct page **pages;
    struct list_head next;
};

extern struct list_head vai_dma_region_list;

#endif /* _VAI_INTERNAL_ */
