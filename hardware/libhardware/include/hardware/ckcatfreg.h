#ifndef ANDROID_FREG_INTERFACE_H
#define ANDROID_FREG_INTERFACE_H

#include <hardware/hardware.h>

__BEGIN_DECLS

/* 定义模块ID */
#define FREG_HARDWARE_MODULE_ID "ckcatfreg"

/* 定义设备ID */
#define FREG_HARDWARE_DEVICE_ID "ckcatfreg"

/* 自定义模块结构体 */
struct freg_module_t
{
	struct hw_module_t common;
};

/* 自定义设备结构体 */
struct freg_device_t
{
	struct hw_device_t common;
	int fd;												 /* 文件描述符 */
	int (*set_val)(struct freg_device_t *dev, int val);	 /* 写设备 */
	int (*get_val)(struct freg_device_t *dev, int *val); /* 读设备 */
};

__END_DECLS

#endif
