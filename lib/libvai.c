#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <linux/ioctl.h>

#include "vai_types.h"

struct vai_afu_conn *vai_afu_connect(const char *file_path)
{
    struct vai_afu_conn *conn = NULL;
    struct vai_afu_version version;
    int ret;

    if (!file_path)
        return NULL;

    conn = malloc(sizeof(*conn));
    if (!conn)
        goto err_out;

    conn->fd = open(file_path, O_RDWR);
    if (conn->fd < 0)
        goto err_free_conn;

    ret = ioctl(conn->fd, VAI_GET_AFU_VERSION, &version);
    if (ret) {
        printf("vai: ioctl returns %d\n", ret);
        goto err_close_fd;
    }

    conn->afu_id = version.afu_id;
    conn->vai_afu_version = version.vai_afu_version;
    conn->desc = NULL;

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

int vai_afu_submit_task(struct vai_afu_conn *conn,
            struct vai_user_task_entry *task)
{
    int ret;

    ret = ioctl(conn->fd, VAI_TASK_SUBMIT, task);
    if (ret) {
        printf("vai: ioctl returns %d\n", ret);
        goto err_out;
    }

    return 0;

err_out:
    return -1;
}

int vai_afu_pull_task(struct vai_afu_conn *conn,
            struct vai_user_task_entry **tasks)
{
    return ioctl(conn->fd, VAI_PULL_TASK, tasks);
}
