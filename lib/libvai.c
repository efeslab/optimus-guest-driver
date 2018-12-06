#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <inttypes.h>
#include <stdlib.h>

#include "vai_types.h"
#include "malloc.h"

struct vai_afu_conn *vai_afu_connect(const char *file_path)
{
    struct vai_afu_conn *conn = NULL;
    afu_id_t afu_id;
    int ret;

    if (!file_path)
        return NULL;

    conn = malloc(sizeof(*conn));
    if (!conn)
        goto err_out;

    conn->fd = open(file_path, O_RDWR);
    if (conn->fd < 0)
        goto err_free_conn;

    ret = ioctl(conn->fd, VAI_GET_AFU_ID, &afu_id);
    if (ret) {
        printf("vai: ioctl returns %d\n", ret);
        goto err_close_fd;
    }

    conn->afu_id = afu_id;
    conn->desc = NULL;
	conn->mp = create_mspace(0, 1, conn);

    return conn;

err_close_fd:
    close(conn->fd);
err_free_conn:
    free(conn);
err_out:
    return NULL;
}

void vai_afu_disconnect(struct vai_afu_conn *conn)
{
    if (!conn)
        return;

    if (conn->fd >= 0)
        close(conn->fd);

    if (conn->desc)
        free(conn->desc);
}

int vai_afu_map_region(struct vai_afu_conn *conn,
            uint64_t start_addr, uint64_t length)
{
    struct vai_map_info info;
    int ret;

    if (!VAI_IS_PAGE_ALIGNED(start_addr) || !(VAI_IS_PAGE_ALIGNED(length)))
        return -1;

    info.user_addr = start_addr;
    info.length = length;

    ret = ioctl(conn->fd, VAI_DMA_MAP_REGION, &info);
    if (ret) {
        printf("vai: ioctl returns %d\n", ret);
        goto err_out;
    }

    return 0;

err_out:
    return -1;
}

int vai_afu_unmap_region(struct vai_afu_conn *conn,
            uint64_t start_addr, uint64_t length)
{
    struct vai_map_info info;
    int ret;

    if (!VAI_IS_PAGE_ALIGNED(start_addr) || !(VAI_IS_PAGE_ALIGNED(length)))
        return -1;
 
    info.user_addr = start_addr;
    info.length = length;

    ret = ioctl(conn->fd, VAI_DMA_UNMAP_REGION, &info);
    if (ret) {
        printf("vai: ioctl returns %d\n", ret);
        goto err_out;
    }

    return 0;

err_out:
    return -1;
}

void vai_afu_set_mem_base(struct vai_afu_conn *conn, uint64_t mem_base) {
	ioctl(conn->fd, VAI_SET_MEM_BASE, mem_base);
	// ioctl for set mem_base always return 0, because vai_b1w64_mmio return nothing
}

void *vai_afu_malloc(struct vai_afu_conn *conn, uint64_t size) {
	if (!conn)
		return NULL;
	else
		return mspace_malloc(conn->mp, size);
}

void vai_afu_free(struct vai_afu_conn *conn, void *p) {
	if (conn)
		return mspace_free(conn->mp, p);
}
