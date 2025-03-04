/*
 * 这是一个简单的字符设备驱动示例
 * 它实现了一个虚拟的寄存器设备,可以通过读写操作来访问一个整型值
 *
 * 主要功能:
 * 1. 创建一个字符设备节点 /dev/freg
 * 2. 在 /sys/class/freg 下创建设备文件
 * 3. 在 /proc/freg 下创建访问接口
 * 4. 支持对虚拟寄存器的读写操作
 */

// 包含需要的头文件
#include <linux/init.h>	   // 包含初始化和退出宏
#include <linux/module.h>  // 包含核心模块功能
#include <linux/types.h>   // 包含类型定义
#include <linux/fs.h>	   // 包含文件系统相关定义
#include <linux/proc_fs.h> // 包含proc文件系统定义
#include <linux/device.h>  // 包含设备相关定义
#include <asm/uaccess.h>   // 包含用户空间访问函数

#include "ckcatfreg.h" // 包含自定义的头文件

static int freg_major = 0; /* 主设备号 */
static int freg_minor = 0; /* 从设备号 */

/* 设备类别和设备变量 */
static struct class *freg_class = NULL;
static struct fake_reg_dev *freg_dev = NULL;

/* 传统文件操作回调函数 */
static int freg_open(struct inode *inode, struct file *filp);
static int freg_release(struct inode *inode, struct file *filp);
static ssize_t freg_read(struct file *filp, char __user *buf, size_t count,
						 loff_t *f_pos);
static ssize_t freg_write(struct file *filp, const char __user *buf,
						  size_t count, loff_t *f_pos);

/* 传统文件操作回调函数表
 * 当应用程序对设备文件进行操作时会调用这些函数
 * .owner: 模块的所有者
 * .open: 打开设备时的回调函数
 * .release: 关闭设备时的回调函数
 * .read: 读取设备时的回调函数
 * .write: 写入设备时的回调函数
 */
static struct file_operations freg_fops = {
	.owner = THIS_MODULE,
	.open = freg_open,
	.release = freg_release,
	.read = freg_read,
	.write = freg_write,
};

/* devfs 文件系统的设备属性操作方法 */
static ssize_t freg_val_show(struct device *dev,
							 struct device_attribute *attr, char *buf);
static ssize_t freg_val_store(struct device *dev,
							  struct device_attribute *attr, const char *buf, size_t count);

/* devfs 文件系统的设备属性 */
static DEVICE_ATTR(val, S_IRUGO | S_IWUSR, freg_val_show, freg_val_store);

/* 打开设备的回调函数
 * 当应用程序打开设备文件时被调用
 * inode: 包含设备号等信息
 * filp: 文件指针
 */
static int freg_open(struct inode *inode, struct file *filp)
{
	struct fake_reg_dev *dev;

	/* 将自定义的设备结构体保存在文件指针的私有数据域中，以便访问设备时可以直接使用
	 * container_of(ptr, type, member) 宏用于从结构体的某个成员指针获取整个结构体的指针。
	 */
	dev = container_of(inode->i_cdev, struct fake_reg_dev, dev);
	filp->private_data = dev;

	return 0;
}

/* 关闭设备的回调 */
static int freg_release(struct inode *inode, struct file *filp)
{
	return 0;
}

/* 读取设备的回调函数
 * 当应用程序读取设备时被调用
 * filp: 文件指针
 * buf: 用户空间的缓冲区
 * count: 要读取的字节数
 * f_pos: 文件位置指针
 */
static ssize_t freg_read(struct file *filp, char __user *buf,
						 size_t count, loff_t *f_pos)
{
	ssize_t err = 0;
	struct fake_reg_dev *dev = filp->private_data;

	/* 同步访问 */
	if (down_interruptible(&(dev->sem)))
	{
		return -ERESTARTSYS;
	}

	if (count < sizeof(dev->val))
	{
		goto out;
	}

	/* 将 val 的值拷贝到用户提供的缓冲区中 */
	if (copy_to_user(buf, &(dev->val), sizeof(dev->val)))
	{
		err = -EFAULT;
		goto out;
	}

	err = sizeof(dev->val);

out:
	up(&(dev->sem));
	return err;
}

/* 写入设备的回调 */
static ssize_t freg_write(struct file *filp, const char __user *buf,
						  size_t count, loff_t *f_pos)
{
	struct fake_reg_dev *dev = filp->private_data;
	ssize_t err = 0;

	/* 同步访问 */
	if (down_interruptible(&(dev->sem)))
	{
		return -ERESTARTSYS;
	}

	if (count != sizeof(dev->val))
	{
		goto out;
	}

	/* 将用户提供的缓冲区中的值写到 val */
	if (copy_from_user(&(dev->val), buf, count))
	{
		err = -EFAULT;
		goto out;
	}

	err = sizeof(dev->val);

out:
	up(&(dev->sem));
	return err;
}

/* 将寄存器的值读到缓冲区 buf 中，内部使用 */
static ssize_t __freg_get_val(struct fake_reg_dev *dev, char *buf)
{
	int val = 0;
	printk(KERN_ALERT "__freg_get_val\n");

	if (down_interruptible(&(dev->sem)))
	{
		return -ERESTARTSYS;
	}

	val = dev->val;
	up(&(dev->sem));

	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}

/* 将缓冲区 buf 中的值写到 val 中，内部使用 */
static ssize_t __freg_set_val(struct fake_reg_dev *dev, const char *buf,
							  size_t count)
{
	int val = 0;
	printk(KERN_ALERT "__freg_set_val\n");

	val = simple_strtol(buf, NULL, 10);

	if (down_interruptible(&(dev->sem)))
	{
		return -ERESTARTSYS;
	}

	dev->val = val;
	up(&(dev->sem));

	return count;
}

/* devfs 文件系统读取回调 */
static ssize_t freg_val_show(struct device *dev,
							 struct device_attribute *attr, char *buf)
{
	/* 获取设备的私有数据 */
	struct fake_reg_dev *hdev = (struct fake_reg_dev *)dev_get_drvdata(dev);

	return __freg_get_val(hdev, buf);
}

/* devfs 文件系统写入回调 */
static ssize_t freg_val_store(struct device *dev, struct device_attribute *attr,
							  const char *buf, size_t count)
{
	struct fake_reg_dev *hdev = (struct fake_reg_dev *)dev_get_drvdata(dev);

	return __freg_set_val(hdev, buf, count);
}

/*proc 文件读回调*/
static ssize_t freg_proc_read(char *page, char **start, off_t off,
							  int count, int *eof, void *data)
{
	if (off > 0)
	{
		*eof = 1;
		return 0;
	}

	return __freg_get_val(freg_dev, page);
}

/* proc 文件写回调 */
static ssize_t freg_proc_write(struct file *filp,
							   const char __user *buff, unsigned long len, void *data)
{
	int err = 0;
	char *page = NULL;

	if (len > PAGE_SIZE)
	{
		printk(KERN_ALERT "The buff is too large: %lu.\n", len);
		return -EFAULT;
	}

	page = (char *)__get_free_page(GFP_KERNEL);
	if (!page)
	{
		printk(KERN_ALERT "Failed to alloc page.\n");
		return -ENOMEM;
	}

	if (copy_from_user(page, buff, len))
	{
		printk(KERN_ALERT "Failed to copy buff from user.\n");
		err = -EFAULT;
		goto out;
	}

	err = __freg_set_val(freg_dev, page, len);

out:
	free_page((unsigned long)page);
	return err;
}

/* 创建 /proc/ckcatfreg 文件 */
static void freg_create_proc(void)
{
	struct proc_dir_entry *entry;

	entry = create_proc_entry(FREG_DEVICE_PROC_NAME, 0, NULL);
	if (entry)
	{
		entry->owner = THIS_MODULE;
		entry->read_proc = freg_proc_read;	 /* 设置文件读回调 */
		entry->write_proc = freg_proc_write; /* 设置文件写回调 */
	}
}

/* 删除 /proc/ckcatfreg 文件 */
static void freg_remove_proc(void)
{
	remove_proc_entry(FREG_DEVICE_PROC_NAME, NULL);
}

/* 初始化设备 */
static int __freg_setup_dev(struct fake_reg_dev *dev)
{
	int err;
	dev_t devno = MKDEV(freg_major, freg_minor);

	memset(dev, 0, sizeof(struct fake_reg_dev));

	/* 初始化字符设备 */
	cdev_init(&(dev->dev), &freg_fops);
	dev->dev.owner = THIS_MODULE;
	dev->dev.ops = &freg_fops;

	/* 注册字符串设备 */
	err = cdev_add(&(dev->dev), devno, 1);
	if (err)
	{
		return err;
	}

	/* 初始化信号量和虚拟寄存器值 */
	init_MUTEX(&(dev->sem));
	dev->val = 0;

	return 0;
}

/* 驱动程序的入口函数
 * 在加载模块时被调用
 * 完成以下工作:
 * 1. 申请设备号
 * 2. 创建设备类
 * 3. 创建设备文件
 * 4. 注册字符设备
 * 5. 初始化设备
 */
static int __init freg_init(void)
{
	int err = -1;
	dev_t dev = 0;
	struct device *temp = NULL;

	printk(KERN_ALERT "Initializing freg device.\n");

	/* 动态分配主设备号和从设备号 */
	err = alloc_chrdev_region(&dev, 0, 1, FREG_DEVICE_NODE_NAME);
	if (err < 0)
	{
		printk(KERN_ALERT "Failed to alloc char dev region.\n");
		goto fail;
	}

	freg_major = MAJOR(dev); /* 获取主设备号 */
	freg_minor = MINOR(dev); /* 获取从设备号 */

	/* 分配自定义虚拟设备空间 */
	freg_dev = kmalloc(sizeof(struct fake_reg_dev), GFP_KERNEL);
	if (!freg_dev)
	{
		err = -ENOMEM;
		printk(KERN_ALERT "Failed to alloc ckcatfreg device.\n");
		goto unregister;
	}

	/* 初始化设备 */
	err = __freg_setup_dev(freg_dev);
	if (err)
	{
		printk(KERN_ALERT "Failed to setup ckcatfreg device: %d.\n", err);
		goto cleanup;
	}

	/* 在 /sys/class/ 目录下创建设备类别目录 ckcatfreg */
	freg_class = class_create(THIS_MODULE, FREG_DEVICE_CLASS_NAME);
	if (IS_ERR(freg_class))
	{
		err = PTR_ERR(freg_class);
		printk(KERN_ALERT "Failed to create ckcatfreg device class.\n");
		goto destroy_cdev;
	}
	/* 在 /dev/ 目录和 /sys/class/ckcatfreg 目录下分别创建文件 ckcatfreg */
	temp = device_create(freg_class, NULL, dev, "%s", FREG_DEVICE_FILE_NAME);
	if (IS_ERR(temp))
	{
		err = PTR_ERR(temp);
		printk(KERN_ALERT "Failed to create ckcatfreg device.\n");
		goto destroy_class;
	}

	/* 在 /sys/class/ckcatfreg/ckcatfreg 目录下创建属性文件 val */
	err = device_create_file(temp, &dev_attr_val);
	if (err < 0)
	{
		printk(KERN_ALERT "Failed to create attribute val of freg device.\n");
		goto destroy_device;
	}

	dev_set_drvdata(temp, freg_dev);

	/* 创建 /proc/ckcatfreg 文件 */
	freg_create_proc();

	printk(KERN_ALERT "Succedded to initialize freg device.\n");

	return 0;

destroy_device:
	device_destroy(freg_class, dev); /* 删除 ckcatfreg 文件 */
destroy_class:
	class_destroy(freg_class); /* 删除 /sys/class/ 文件 */
destroy_cdev:
	cdev_del(&(freg_dev->dev)); /* 删除设备 */
cleanup:
	kfree(freg_dev); /* 释放虚拟设备空间 */
unregister:
	unregister_chrdev_region(MKDEV(freg_major, freg_minor), 1); /* 释放设备号 */
fail:
	return err;
}

/* 驱动程序的退出函数
 * 在卸载模块时被调用
 * 完成资源释放工作:
 * 1. 删除 proc 文件
 * 2. 删除设备文件
 * 3. 注销设备类
 * 4. 释放设备号
 */
static void __exit freg_exit(void)
{
	dev_t devno = MKDEV(freg_major, freg_minor);

	printk(KERN_ALERT "Destroy ckcatfreg device.\n");

	freg_remove_proc();

	if (freg_class)
	{ /* 删除 ckcatfreg 文件 */
		device_destroy(freg_class, MKDEV(freg_major, freg_minor));
		/* 删除 /sys/class/ 文件 */
		class_destroy(freg_class);
	}

	if (freg_dev)
	{
		cdev_del(&(freg_dev->dev)); /* 删除设备 */
		kfree(freg_dev);			/* 释放虚拟设备空间 */
	}

	unregister_chrdev_region(devno, 1); /* 释放设备号 */
}

// 声明模块的许可证
MODULE_LICENSE("GPL");
// 模块描述信息
MODULE_DESCRIPTION("CKCat Fake Register Driver");

// 注册模块的入口和出口函数
module_init(freg_init); // 模块加载函数
module_exit(freg_exit); // 模块卸载函数
