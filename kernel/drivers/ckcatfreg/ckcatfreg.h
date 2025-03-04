#ifndef _FAKE_REG_H_
#define _FAKE_REG_H_

#include <linux/cdev.h>
#include <linux/semaphore.h>

#define FREG_DEVICE_NODE_NAME  "ckcatfreg"  /* 设备名称 */
#define FREG_DEVICE_FILE_NAME  "ckcatfreg"  /* /dev 目录下名称 */
#define FREG_DEVICE_PROC_NAME  "ckcatfreg"  /* /proc 目录下名称 */
#define FREG_DEVICE_CLASS_NAME "ckcatfreg"  /* /sys/class/ 目录下名称 */

/* 虚拟硬件设备freg */
struct fake_reg_dev {
	int val;               /* 虚拟寄存器 */
	struct semaphore sem;  /* 同步信号量 */
	struct cdev dev;       /* 设备对象 */
};

#endif

