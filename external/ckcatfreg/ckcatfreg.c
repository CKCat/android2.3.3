#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

// 虚拟寄存器设备的路径
#define FREG_DEVICE_NAME "/dev/ckcatfreg"

int main(int argc, char const *argv[])
{
    int fd = -1;
    int val = 0;
    // 以读写方式打开设备文件
    fd = open(FREG_DEVICE_NAME, O_RDWR);
    if (fd == -1)
    {
        printf("Failed to open device %s.\n", FREG_DEVICE_NAME);
        return -1;
    }
    printf("Read original value:\n");
    // 读取设备的原始值
    read(fd, &val, sizeof(val));
    printf("%d.\n\n", val);

    val = 5;
    printf("Write value %d to %s.\n\n", val, FREG_DEVICE_NAME);
    // 写入新的值
    write(fd, &val, sizeof(val));

    printf("Read the value again:\n");
    val = 0;
    read(fd, &val, sizeof(val));
    printf("%d.\n\n", val);

    return 0;
}