

#include <hardware/hardware.h>
#include <hardware/ckcatfreg.h>

#include <fcntl.h>
#include <errno.h>

#include <cutils/log.h>
#include <cutils/atomic.h>

#define LOG_TAG "CKCatFregHALStub"

#define DEVICE_NAME "/dev/ckcatfreg"   /* 驱动设备文件路径 */
#define MODULE_NAME "CKCatFreg"		   /* 模块名称 */
#define MODULE_AUTHOR "ckcatck@qq.com" /* 模块作者 */

/* 打开设备 */
static int freg_device_open(const struct hw_module_t *module,
							const char *id, struct hw_device_t **device);

/* 关闭设备 */
static int freg_device_close(struct hw_device_t *device);

/* 写入设备 */
static int freg_set_val(struct freg_device_t *dev, int val);

/* 读取设备 */
static int freg_get_val(struct freg_device_t *dev, int *val);

/* 定义模块操作方法结构体变量 */
static struct hw_module_methods_t freg_module_methods = {
	open : freg_device_open
};

/*定义模块结构体
按照硬件抽象层模块编写规范，每一个硬件抽象层模块必须导出一个名称为 HAL_MODULE_INFO_SYM 的符号，
它指向一个自定义的硬件抽象层模块结构体，而且它的第一个类型为 hw_module_t 的成员变量的 tag 值
必须设置为 HARDWARE_MODULE_TAG 。
*/
struct freg_module_t HAL_MODULE_INFO_SYM = {
	common : {
		tag : HARDWARE_MODULE_TAG,
		version_major : 1,
		version_minor : 0,
		id : FREG_HARDWARE_MODULE_ID,	/* 模块ID */
		name : MODULE_NAME,
		author : MODULE_AUTHOR,
		methods : &freg_module_methods, /* 模块方法 */
	}
};

/* 打开设备 */
static int freg_device_open(const struct hw_module_t *module,
							const char *id, struct hw_device_t **device)
{	
	/* 判断设备ID是否匹配 */
	if (!strcmp(id, FREG_HARDWARE_DEVICE_ID))
	{
		struct freg_device_t *dev;
		/* 设备结构体内存 */
		dev = (struct freg_device_t *)malloc(sizeof(struct freg_device_t));
		if (!dev)
		{
			LOGE("Failed to alloc space for freg_device_t.");
			return -EFAULT;
		}

		memset(dev, 0, sizeof(struct freg_device_t));
		/* 初始化设备结构体 */
		dev->common.tag = HARDWARE_DEVICE_TAG; // 按规范必须为 HARDWARE_DEVICE_TAG
		dev->common.version = 0;
		dev->common.module = (hw_module_t *)module;
		dev->common.close = freg_device_close;
		dev->set_val = freg_set_val;
		dev->get_val = freg_get_val;
		/* 打开设备文件 */
		if ((dev->fd = open(DEVICE_NAME, O_RDWR)) == -1)
		{
			LOGE("Failed to open device file /dev/ckcatfreg -- %s.", strerror(errno));
			free(dev);
			return -EFAULT;
		}
		/* 返回设备结构体 */
		*device = &(dev->common);

		LOGI("Open device file /dev/ckcatfreg successfully.");

		return 0;
	}

	return -EFAULT;
}

/* 关闭设备 */
static int freg_device_close(struct hw_device_t *device)
{
	struct freg_device_t *freg_device = (struct freg_device_t *)device;
	if (freg_device)
	{
		close(freg_device->fd);
		free(freg_device);
	}

	return 0;
}

/* 写入设备 */
static int freg_set_val(struct freg_device_t *dev, int val)
{
	if (!dev)
	{
		LOGE("Null dev pointer.");
		return -EFAULT;
	}

	LOGI("Set value %d to device file /dev/ckcatfreg.", val);
	write(dev->fd, &val, sizeof(val));

	return 0;
}

/* 读取设备 */
static int freg_get_val(struct freg_device_t *dev, int *val)
{
	if (!dev)
	{
		LOGE("Null dev pointer.");
		return -EFAULT;
	}

	if (!val)
	{
		LOGE("Null val pointer.");
		return -EFAULT;
	}

	read(dev->fd, val, sizeof(*val));

	LOGI("Get value %d from device file /dev/ckcatfreg.", *val);

	return 0;
}