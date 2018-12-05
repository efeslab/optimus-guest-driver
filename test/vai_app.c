#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <assert.h>

#include "vai_types.h"

int main(void)
{
    char *file_name = "/dev/vai";
    int fd;

    void *ptr;
    uint64_t *bar;
    uint64_t a = -1;

    void *test_map;

    struct vai_map_info info;

    fd = open(file_name, O_RDWR);
    afu_id_t afu_id;
    int ret = ioctl(fd, VAI_GET_AFU_ID, &afu_id);
    assert (ret == 0);
    printf("afu_id is %#llx\n", *((unsigned long long*)(&afu_id)));
    ptr = mmap(NULL, 8, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    bar = ptr;

    *bar = a;

    test_map = mmap((void*)0x10000, 4096, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    printf("addr: %llx\n", test_map);
    info.user_addr = (uint64_t)test_map;
    info.length = 4096;

    ret = ioctl(fd, VAI_DMA_MAP_REGION, &info);
    uint64_t mem_base=0x12345123;
    ret= ioctl(fd, VAI_SET_MEM_BASE, &mem_base);
    ret = ioctl(fd, VAI_SET_RESET);

    printf("test_msg: %s\n", (char *)test_map);

    close(fd);

    return 0;
}

