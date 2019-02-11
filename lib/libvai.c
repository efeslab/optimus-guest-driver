#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <inttypes.h>
#include <stdlib.h>

#include "vai/vai.h"
#include "vai/malloc.h"

#define MMIO_SPACE_LENGTH 0x1000
#define VAI_FILE_PATH "/dev/vai"
struct vai_afu_conn *vai_afu_connect()
{
    struct vai_afu_conn *conn = NULL;
    afu_id_t afu_id;
    int ret;

    conn = malloc(sizeof(*conn));
    if (!conn)
        goto err_out;

    conn->fd = open(VAI_FILE_PATH, O_RDWR);
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
    conn->bar = (volatile uint64_t *)mmap(NULL, MMIO_SPACE_LENGTH, PROT_READ | PROT_WRITE, MAP_SHARED, conn->fd, 0);
    if (conn->bar == MAP_FAILED) {
        perror("vai: map mmio bar failed");
        goto err_close_fd;
    }

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

    if (conn->bar)
        if (munmap((void*)conn->bar, MMIO_SPACE_LENGTH) != 0)
            perror("vai: unmap mmio bar failed");

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

volatile void *vai_afu_malloc(struct vai_afu_conn *conn, uint64_t size) {
	if (!conn)
		return NULL;
	else
		return (volatile void *)mspace_malloc(conn->mp, size);
}

void vai_afu_free(struct vai_afu_conn *conn, volatile void *p) {
	if (conn)
		return mspace_free(conn->mp, (void *)p);
}

int vai_afu_mmio_read(struct vai_afu_conn *conn, uint64_t offset, volatile uint64_t *value) {
    if (conn == NULL || conn->bar == NULL || offset > MMIO_SPACE_LENGTH)
        return -1;
    *value = conn->bar[offset/8];
    return 0;
}

int vai_afu_mmio_write(struct vai_afu_conn *conn, uint64_t offset, uint64_t value) {
    if (conn == NULL || conn->bar == NULL || offset > MMIO_SPACE_LENGTH)
        return -1;
    conn->bar[offset/8] = value;
    return 0;
}
