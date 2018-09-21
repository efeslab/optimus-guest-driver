#ifndef _VAI_TYPES_H_
#define _VAI_TYPES_H_

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <inttypes.h>
#endif

#define CL_SIZE 64
#define CL(x) CL_SIZE*(x)

#define VAI_PAGE_SIZE 4096
#define VAI_PAGE_SHIFT 12
#define VAI_IS_PAGE_ALIGNED(x) (!((x)&((1<<VAI_PAGE_SHIFT)-1)))

#define VAI_USER_TASK_ENTRY_SIZE CL(3)

#define VAI_MAGIC 0xBB
#define VAI_BASE 0x0

typedef struct {
    uint8_t data[16];
} afu_id_t;

struct vai_afu_conn {
    int fd;
    afu_id_t afu_id;
    char *desc;
};

/**
 * VAI_GET_AFU_VERSION
 *
 * Report the version of the driver API by entering the field in the given struct.
 * Return: 0 on success, -errno on failure.
 */
#define VAI_GET_AFU_ID _IO(VAI_MAGIC, VAI_BASE + 0)


struct vai_map_info {
    uint64_t user_addr;
    uint64_t length;
};

/**
 * VAI_DMA_MAP_REGION
 *
 * Map a DMA region based on the provided struct vai_map_info.
 * Return: 0 on success, -errno on failure.
 */
#define VAI_DMA_MAP_REGION _IO(VAI_MAGIC, VAI_BASE + 1)

/**
 * VAI_DMA_UNMAP_REGION
 *
 * Unmap a DMA region based on the provided struct vai_map_info.
 * Return: 0 on success, -errno on failure.
 */
#define VAI_DMA_UNMAP_REGION _IO(VAI_MAGIC, VAI_BASE + 2)


struct vai_user_task_entry {
    uint8_t private[VAI_USER_TASK_ENTRY_SIZE];
};

/**
 * VAI_TASK_SUBMIT
 *
 * Submit a task to the virtualized hardware based on the provided struct vai_task_entry.
 * Return: 0 on success, -errno on failure.
 */
#define VAI_SUBMIT_TASK _IO(VAI_MAGIC, VAI_BASE + 3)

/**
 * VAI_PULL_TASK
 *
 * Pull the ring to get finished tasks.
 * Return: the number of finished tasks.
 */
#define VAI_PULL_TASK _IO(VAI_MAGIC, VAI_BASE + 4)

#endif /* _VAI_TYPES_H_ */
