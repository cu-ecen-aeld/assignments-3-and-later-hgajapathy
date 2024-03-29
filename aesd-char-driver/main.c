/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>    /* size_t */
#include <linux/cdev.h>
#include <linux/fs.h>       /* everything... */
#include <linux/slab.h>     /* kmalloc() */
#include <linux/uaccess.h>  /* copy_*_user */

#include "aesd_ioctl.h"
#include "aesdchar.h"

int aesd_major =   0;       /* use dynamic major */
int aesd_minor =   0;

MODULE_AUTHOR("Harinarayanan Gajapathy");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
	struct aesd_dev *dev = NULL;

	PDEBUG("open\n");

	dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
	filp->private_data = dev; /* for other methods */

	return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
	PDEBUG("release\n");

	filp->private_data = NULL;

	return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
				loff_t *f_pos)
{
	ssize_t retval = 0;
	size_t entry_offset = 0;
	struct aesd_buffer_entry *entry = NULL;
	struct aesd_dev *dev = NULL;

	PDEBUG("read %zu bytes with offset %lld\n", count, *f_pos);

	if (filp == NULL || buf == NULL) {
		PDEBUG("invalid arguments\n");
		return -EINVAL;
	}

	dev = filp->private_data;

	if (mutex_lock_interruptible(&dev->lock) != 0) {
		PDEBUG("failed to acquire mutex\n");
		return -ERESTARTSYS;
	}

	entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->cb, *f_pos, &entry_offset);
	if (entry != NULL) {
		retval = copy_to_user(buf, (entry->buffptr + entry_offset), (entry->size - entry_offset));
		retval = (entry->size - entry_offset) - retval;
		*f_pos += retval;
	}

	PDEBUG("aesd_read returns %ld\n", retval);

	mutex_unlock(&dev->lock);

	return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
				loff_t *f_pos)
{
	ssize_t retval = -ENOMEM;
	const char *rtnptr = NULL;
	struct aesd_dev *dev = NULL;

	PDEBUG("write %zu bytes with offset %lld\n", count, *f_pos);

	if (filp == NULL || buf == NULL) {
		PDEBUG("invalid arguments\n");
		return -EINVAL;
	}

	dev = filp->private_data;

	if (mutex_lock_interruptible(&dev->lock) != 0) {
		PDEBUG("failed to acquire mutex\n");
		return -ERESTARTSYS;
	}

	if (dev->entry.size == 0) {
		dev->entry.buffptr = (char *) kzalloc(count, GFP_KERNEL);
	} else {
		dev->entry.buffptr = (char *) krealloc(dev->entry.buffptr, \
								dev->entry.size + count, GFP_KERNEL);
	}

	if (dev->entry.buffptr == NULL) {
		PDEBUG("failed to allocate memory\n");
		retval = -ENOMEM;
	} else {
		/* copy_from_user - returns number of bytes that could not be copied.
		* On success, this will be zero. */
		retval = copy_from_user((void *) dev->entry.buffptr + dev->entry.size, buf, count);

		retval = count - retval;
		dev->entry.size += retval;
		PDEBUG("copied %ld bytes from userspace to kernel space, total size %ld\n", \
					retval, dev->entry.size);

		if (dev->entry.buffptr[(dev->entry.size - 1)] == '\n') {
			rtnptr = aesd_circular_buffer_add_entry(&dev->cb, &dev->entry);
			if (rtnptr != NULL)
				kfree(rtnptr);

			dev->entry.buffptr = NULL;
			dev->entry.size = 0;
		}
	}

	mutex_unlock(&dev->lock);

	return retval;
}

static long aesd_adjust_file_offset(struct file *filp, unsigned int write_cmd, unsigned int write_cmd_offset)
{
	long retval = 0;
	int index = 0;
	struct aesd_buffer_entry *entry = NULL;
	struct aesd_dev *dev = NULL;

	PDEBUG("adjust_file_offset\n");

	if (filp == NULL) {
		PDEBUG("invalid arguments\n");
		return -EINVAL;
	}

	dev = filp->private_data;

	if (mutex_lock_interruptible(&dev->lock) != 0) {
		PDEBUG("failed to acquire mutex\n");
		return -ERESTARTSYS;
	}

	AESD_CIRCULAR_BUFFER_FOREACH(entry, &dev->cb, index) {
		/* index will give the total number of commands in circular buffer */
	}

	if (write_cmd > index ||
		write_cmd > AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED ||
		write_cmd_offset >= dev->cb.entry[write_cmd].size) {
		retval = -EINVAL;
	} else {
		for (index = 0; index < write_cmd; index++)
			filp->f_pos += dev->cb.entry[index].size;

		filp->f_pos += write_cmd_offset;
	}

	mutex_unlock(&dev->lock);

	return retval;
}

long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long retval = 0;
	struct aesd_seekto seekto;

	PDEBUG("ioctl\n");

	if (filp == NULL) {
		PDEBUG("invalid arguments\n");
		return -EINVAL;
	}

   	/*
	 * extract the type and number bitfields, and don't decode
	 * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
	 */
	if (_IOC_TYPE(cmd) != AESD_IOC_MAGIC) return -ENOTTY;
	if (_IOC_NR(cmd) > AESDCHAR_IOC_MAXNR) return -ENOTTY;

	switch (cmd) {
	case AESDCHAR_IOCSEEKTO:
		if (copy_from_user(&seekto, (const void __user *)arg, sizeof(seekto)) != 0)
			retval = -EFAULT;
		else
			retval = aesd_adjust_file_offset(filp, seekto.write_cmd, seekto.write_cmd_offset);
		break;

	default:
		retval = -ENOTTY;
		break;
	}

	return retval;
}

loff_t aesd_llseek(struct file *filp, loff_t off, int whence)
{
	loff_t newpos;
	loff_t size = 0;
	int index = 0;
	struct aesd_dev *dev = NULL;
	struct aesd_buffer_entry *entry = NULL;

	PDEBUG("llseek\n");

	if (filp == NULL) {
		PDEBUG("invalid arguments\n");
		return -EINVAL;
	}

	dev = filp->private_data;

	if (mutex_lock_interruptible(&dev->lock) != 0) {
		PDEBUG("failed to acquire mutex\n");
		return -ERESTARTSYS;
	}

	/* calculate size */
	AESD_CIRCULAR_BUFFER_FOREACH(entry, &dev->cb, index)
		size += entry->size;

	newpos = fixed_size_llseek(filp, off, whence, size);

	mutex_unlock(&dev->lock);

	return newpos;
}

struct file_operations aesd_fops = {
	.owner =    THIS_MODULE,
	.read =     aesd_read,
	.write =    aesd_write,
	.open =     aesd_open,
	.release =  aesd_release,
	.llseek =   aesd_llseek,
	.unlocked_ioctl = aesd_ioctl
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
	int err, devno = MKDEV(aesd_major, aesd_minor);

	cdev_init(&dev->cdev, &aesd_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &aesd_fops;
	err = cdev_add(&dev->cdev, devno, 1);
	if (err) {
		printk(KERN_ERR "Error %d adding aesd cdev", err);
	}

	return err;
}

int aesd_init_module(void)
{
	dev_t dev = 0;
	int result;

	PDEBUG("init_module\n");

	result = alloc_chrdev_region(&dev, aesd_minor, 1,
			"aesdchar");
	aesd_major = MAJOR(dev);
	if (result < 0) {
		printk(KERN_WARNING "Can't get major %d\n", aesd_major);
		return result;
	}
	memset(&aesd_device,0,sizeof(struct aesd_dev));

	mutex_init(&aesd_device.lock);
	aesd_circular_buffer_init(&aesd_device.cb);

	result = aesd_setup_cdev(&aesd_device);
	if (result) {
		unregister_chrdev_region(dev, 1);
	}

	return result;
}

void aesd_cleanup_module(void)
{
	dev_t devno = MKDEV(aesd_major, aesd_minor);
	struct aesd_buffer_entry *entry = NULL;
	int index = 0;

	PDEBUG("cleanup_module\n");

	mutex_destroy(&aesd_device.lock);

	AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.cb, index) {
		if (entry->buffptr != NULL) {
			PDEBUG("bufferptr - %s, size %ld\n", entry->buffptr, entry->size);
			kfree(entry->buffptr);
		}
	}

	cdev_del(&aesd_device.cdev);
	unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
