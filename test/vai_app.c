#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "vai_types.h"

int main(void)
{
    char *file_name = "/dev/vai";
    int fd;

    void *ptr;
    uint32_t *bar;
    uint32_t a;

    fd = open(file_name, O_RDWR);
    //ioctl(fd, VAI_GET_AFU_ID, 0);
    ptr = mmap(NULL, 8, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    bar = ptr;

    a = *bar; 

    printf("a: %u\n", a);
    
    close(fd);

    return 0;
}

