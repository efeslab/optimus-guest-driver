#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <inttypes.h>
#include <stdlib.h>

#include "vai_utils.h"
#include "vai_types.h"

int vai_afu_map_region(struct _fpga_handle *fpga,
            uint64_t start_addr, uint64_t length)
{
    struct vai_map_info info;
    int ret;

    if (!VAI_IS_PAGE_ALIGNED(start_addr) || !(VAI_IS_PAGE_ALIGNED(length)))
        return -1;

    info.user_addr = start_addr;
    info.length = length;

    ret = ioctl(fpga->fddev, VAI_DMA_MAP_REGION, &info);
    if (ret) {
        printf("vai: ioctl returns %d\n", ret);
        goto err_out;
    }

    return 0;

err_out:
    return -1;
}

int vai_afu_unmap_region(struct _fpga_handle *fpga,
            uint64_t start_addr, uint64_t length)
{
    struct vai_map_info info;
    int ret;

    if (!VAI_IS_PAGE_ALIGNED(start_addr) || !(VAI_IS_PAGE_ALIGNED(length)))
        return -1;
 
    info.user_addr = start_addr;
    info.length = length;

    ret = ioctl(fpga->fddev, VAI_DMA_UNMAP_REGION, &info);
    if (ret) {
        printf("vai: ioctl returns %d\n", ret);
        goto err_out;
    }

    return 0;

err_out:
    return -1;
}

void vai_afu_set_mem_base(struct _fpga_handle *fpga, uint64_t mem_base) {
	ioctl(fpga->fddev, VAI_SET_MEM_BASE, mem_base);
	// ioctl for set mem_base always return 0, because vai_b1w64_mmio return nothing
}
