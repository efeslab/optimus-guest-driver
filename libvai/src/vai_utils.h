#ifndef __VAI_UTILS__
#define __VAI_UTILS__
#include <stdio.h>
#include <stdlib.h>
#include "types_int.h"
#include "vai_types.h"
#define NOIMPL {\
    fprintf(stderr, "[%s:%d] not implemented", __func__, __LINE__);\
    exit(-1);\
}

// this length should be consistent with vai_mux implementation
#define MMIO_SPACE_LENGTH 0x1000
/**
 * vai_afu_map_region
 *
 * Map a page-aligned region to the AFU's DMA region.
 */
int vai_afu_map_region(struct _fpga_handle *fpga,
            uint64_t start_addr, uint64_t length);

/**
 * vai_afu_unmap_region
 *
 * Unmap a page-aligned region.
 */
int vai_afu_unmap_region(struct _fpga_handle *fpga,
            uint64_t start_addr, uint64_t length);

/**
 * vai_afu_submit_task
 *
 * Submit a user_defined task to vAFU.
 */
int vai_afu_submit_task(struct _fpga_handle *fpga,
            struct vai_user_task_entry *task);

/**
 * vai_afu_pull_task
 *
 * Pull the task ring.
 * Return the number of finished tasks.
 */
int vai_afu_pull_task(struct _fpga_handle *fpga,
            struct vai_user_task_entry **tasks);

void vai_afu_set_mem_base(struct _fpga_handle *fpga, uint64_t mem_base);
#endif // __VAI_UTILS__
