#ifndef _LIBVAI_H_
#define _LIBVAI_H_

#include "vai_types.h"

/**
 * vai_afu_connect
 *
 * Connect to an vAFU.
 * Return the vai_afu_conn struct if success, NULL if failed.
 */
struct vai_afu_conn *vai_afu_connect(const char *file_path);

/**
 * vai_afu_disconnect
 *
 * Disconnect to an vAFU. It won't free the conn.
 */
void vai_afu_disconnect(struct vai_afu_conn *conn);

/**
 * vai_afu_map_region
 *
 * Map a page-aligned region to the AFU's DMA region.
 */
int vai_afu_map_region(struct vai_afu_conn *conn,
            uint64_t start_addr, uint64_t length);

/**
 * vai_afu_unmap_region
 *
 * Unmap a page-aligned region.
 */
int vai_afu_unmap_region(struct vai_afu_conn *conn,
            uint64_t start_addr, uint64_t length);

/**
 * vai_afu_submit_task
 *
 * Submit a user_defined task to vAFU.
 */
int vai_afu_submit_task(struct vai_afu_conn *conn,
            struct vai_user_task_entry *task);

/**
 * vai_afu_pull_task
 *
 * Pull the task ring.
 * Return the number of finished tasks.
 */
int vai_afu_pull_task(struct vai_afu_conn *conn,
            struct vai_user_task_entry **tasks);

int vai_afu_set_mem_base(struct vai_afu_conn *conn, uint64_t mem_base);
void *vai_afu_malloc(struct vai_afu_conn *conn, uint64_t size);
void vai_afu_free(struct vai_afu_conn *conn, void *p);
int vai_afu_mmio_read(struct vai_afu_conn *conn, uint64_t offset, uint64_t *value);
int vai_afu_mmio_write(struct vai_afu_conn *conn, uint64_t offset, uint64_t value);

#endif /* _LIBVAI_H_ */
