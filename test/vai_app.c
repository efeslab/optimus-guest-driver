#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <assert.h>

#include "vai/vai.h"
#define BUF_SIZE (4096*10)
struct status_cl {
    uint64_t completion;
    uint64_t n_clk;
    uint32_t n_read;
    uint32_t n_write;
};

int main(void)
{
    char *file_name = "/dev/vai";
    int fd;

    void *ptr;
    volatile uint64_t *bar;
    int i;

    volatile char *test_map;

    struct vai_map_info info;
    int ret;

    fd = open(file_name, O_RDWR);
    ret = ioctl(fd, VAI_SET_RESET);

    ptr = mmap(NULL, 0x100, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    bar = ptr;

    test_map = (volatile char*)mmap(NULL, 2*BUF_SIZE+1, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    printf("addr: %llx\n", test_map);
    info.user_addr = (uint64_t)test_map;
    info.length = 2*BUF_SIZE+1;

    uint64_t mem_base = (uint64_t)test_map;
    ret = ioctl(fd, VAI_SET_MEM_BASE, mem_base);

    ret = ioctl(fd, VAI_DMA_MAP_REGION, &info);

    volatile char *src = test_map;
    volatile char *dst = test_map + BUF_SIZE;
    volatile struct status_cl *stat = test_map + 2*BUF_SIZE;

    uint64_t guidl, guidh;
    guidl = bar[1];
    guidh = bar[2];

    printf("guid: %llx%llx\n", guidh, guidl);

    assert(((uint64_t)test_map)%64 == 0);

    for (i=0; i<BUF_SIZE; i++) {
        src[i] = rand()%256;
        dst[i] = 0;
    }

    bar[0x080/8] = 1;
    bar[0x018/8] = (uint64_t)stat/64;
    bar[0x020/8] = (uint64_t)src/64;
    bar[0x028/8] = (uint64_t)dst/64;
    bar[0x030/8] = (uint64_t)1024/64;
    bar[0x078/8] = 6;
    bar[0x070/8] = 1;

    while (stat->completion == 0) {
    }

    for (i=0; i<BUF_SIZE; i++) {
        if (src[i] != dst[i]) {
            printf("fuck it! [%d] is %d, should be %d\n", i, dst[i], src[i]);
            break;
        }
    }

    printf("nclk: %ld\n", stat->n_clk);

    close(fd);

    return 0;
}

