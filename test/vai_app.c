#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <linux/ioctl.h>

int main(void)
{
    char *file_name = "/dev/vafu";
    int fd;

    fd = open(file_name, O_RDWR);
    ioctl(fd, 0, 0);
    close(fd);

    return 0;
}

