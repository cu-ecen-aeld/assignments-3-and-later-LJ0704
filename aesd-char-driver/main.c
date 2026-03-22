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
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>   
#include <linux/compat.h>

#include "aesdchar.h"
#include "aesd_ioctl.h"

/* Dynamic major number allocation */
int aesd_major = 0;
int aesd_minor = 0;

/* Device structure */
struct aesd_dev aesd_device;

MODULE_AUTHOR("Likhita Jonnakuti");
MODULE_LICENSE("Dual BSD/GPL");

/* Prototypes for file operations */
int aesd_open(struct inode *inode, struct file *filp);
int aesd_release(struct inode *inode, struct file *filp);
ssize_t aesd_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);
loff_t  aesd_llseek(struct file *filp, loff_t offset, int whence);
long    aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);


/* File operations structure */
struct file_operations aesd_fops = {
    .owner = THIS_MODULE,
    .read = aesd_read,
    .write = aesd_write,
    .open = aesd_open,
    .release = aesd_release,
    .llseek = aesd_llseek, //Adding LLseek to file operations
    .unlocked_ioctl = aesd_ioctl,
    .compat_ioctl   = compat_ptr_ioctl,
};

/* Open function */
int aesd_open(struct inode *inode, struct file *filp)
{
    struct aesd_dev *dev;
    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev;
    return 0;
}

/* Release function */
int aesd_release(struct inode *inode, struct file *filp)
{
    return 0;
}

/* Read function */
ssize_t aesd_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    ssize_t retval = 0;
    struct aesd_dev *dev;
    struct aesd_buffer_entry *entry;
    size_t entry_offset;
    size_t bytes_to_read;

    dev = filp->private_data;
    if (!dev || !buf || !f_pos)
        return -EFAULT;

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->circular_buffer,
                                                           *f_pos, &entry_offset);
    if (!entry) {
        retval = 0; /* EOF */
        goto out;
    }

    bytes_to_read = entry->size - entry_offset;
    if (bytes_to_read > count)
        bytes_to_read = count;

    if (copy_to_user(buf, entry->buffptr + entry_offset, bytes_to_read)) {
        retval = -EFAULT;
        goto out;
    }

    *f_pos += bytes_to_read;
    retval = bytes_to_read;

out:
    mutex_unlock(&dev->lock);
    return retval;
}

/* Write function */
ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    ssize_t retval;
    struct aesd_dev *dev;
    char *new_buf;
    char *newline_ptr;
    char *line_start;
    size_t leftover;
    size_t line_len;
    struct aesd_buffer_entry entry;
    const char *old;
    retval = -ENOMEM;
    dev = filp->private_data;   
    if (!dev || !buf)
        return -EFAULT;

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    /* Append new data to partial_entry */
    new_buf = krealloc(dev->partial_entry.buffptr,
                       dev->partial_entry.size + count, GFP_KERNEL);
    if (!new_buf)
        goto out;

    dev->partial_entry.buffptr =  (const char *)new_buf;

    if (copy_from_user((char *)dev->partial_entry.buffptr + dev->partial_entry.size,
                       buf, count)) {
        retval = -EFAULT;
        goto out;
    }

    dev->partial_entry.size += count;
    retval = count;

    /* Process complete lines */
    line_start = (char *)dev->partial_entry.buffptr;
    
    while ((newline_ptr = memchr(line_start, '\n',
                                 (const char *)dev->partial_entry.buffptr
                                 + dev->partial_entry.size
                                 - line_start)) != NULL) {

        line_len      = (size_t)(newline_ptr - line_start) + 1;/* include \n */

        /* Create a temporary entry */
        entry.buffptr = kmalloc(line_len, GFP_KERNEL);
        entry.size = line_len;
        if (!entry.buffptr) {
            retval = -ENOMEM;
            goto out;
        }

        memcpy((void *)entry.buffptr, line_start, line_len);

        /* If the buffer is full, free the entry that will be overwritten */
        if (dev->circular_buffer.full) {
            old = dev->circular_buffer.entry[dev->circular_buffer.out_offs].buffptr;
            if (old)
                kfree(old);
        }
        aesd_circular_buffer_add_entry(&dev->circular_buffer, &entry);

        /* Free overwritten oldest entry if needed handled inside add_entry */

        line_start = newline_ptr + 1;
    }

    /* Any leftover bytes after the last newline stay in partial_entry */
    leftover = dev->partial_entry.buffptr + dev->partial_entry.size - line_start;
    if (leftover > 0) {
        memmove((char *)dev->partial_entry.buffptr, line_start, leftover);
    }
    dev->partial_entry.size = leftover;

out:
    mutex_unlock(&dev->lock);
    return retval;
}


/* 
Function name : aesd_llseek 
Description :used reposition the file offset
	     Supports SEEK_SET, SEEK_CUR, and SEEK_END and updates the file position accordingly
*/

loff_t aesd_llseek(struct file *filp, loff_t offset, int whence)
{
	//struct aesd_dev *dev = filp->private_data;
	//loff_t new_pos;
	//loff_t total_size;
	struct aesd_dev *dev;
        loff_t total_size;
        dev = filp->private_data;
	
	if(!dev)
	{
		return -EFAULT;
	}
	
	if(mutex_lock_interruptible(&dev->lock))
	{
		return -ERESTARTSYS;
	}
	
	total_size = (loff_t)aesd_circular_buffer_total_size(&dev->circular_buffer);
	
	
 /*   switch (whence) {
    case SEEK_SET:
        new_pos = offset;
        break;
    case SEEK_CUR:
        new_pos = filp->f_pos + offset;
        break;
    case SEEK_END:
        new_pos = total_size + offset;
        break;
    default:
        mutex_unlock(&dev->lock);
        return -EINVAL;
    }

    if (new_pos < 0 || new_pos > total_size) {
        mutex_unlock(&dev->lock);
        return -EINVAL;
    }

    filp->f_pos = new_pos;
    mutex_unlock(&dev->lock);
    return new_pos;*/
        mutex_unlock(&dev->lock);

    /*
     * fixed_size_llseek() handles SEEK_SET / SEEK_CUR / SEEK_END,
     * bounds-checks the result (0 … size), and updates filp->f_pos
     * under the inode lock — preventing races between concurrent
     * llseek and read calls on the same file descriptor.
     */
    return fixed_size_llseek(filp, offset, whence, total_size);
}


/* 
Function name : aesd_ioctl
Description:  
*/
long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct aesd_dev *dev = filp->private_data;
    struct aesd_seekto seekto;
    ssize_t new_pos;

    /* Validate magic number and command number */
    if (_IOC_TYPE(cmd) != AESD_IOC_MAGIC)
    {
        return -ENOTTY;
    }
    if (_IOC_NR(cmd) > AESDCHAR_IOC_MAXNR)
    {
        return -ENOTTY;
    }

    switch (cmd) {
    case AESDCHAR_IOCSEEKTO:
        /* Copy the aesd_seekto struct from user space */
        if (copy_from_user(&seekto, (const void __user *)arg, sizeof(seekto)))
        {
            return -EFAULT;
	}
        if (mutex_lock_interruptible(&dev->lock))
        { 
            return -ERESTARTSYS;
	}
        /*
         * Translate (write_cmd, write_cmd_offset) → absolute fpos.
         * Returns -1 when either value is out of range.
         */
        new_pos = aesd_circular_buffer_fpos_from_cmd(&dev->circular_buffer, seekto.write_cmd, seekto.write_cmd_offset);
        
        mutex_unlock(&dev->lock);

        if (new_pos < 0)
        {
            return -EINVAL;
	}
        filp->f_pos = (loff_t)new_pos;
        return 0;

    default:
        return -ENOTTY;
    }
}

/* Setup character device */ 
static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err;
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;

    err = cdev_add(&dev->cdev, devno, 1);
    if (err)
        printk(KERN_ERR "Error %d adding aesd cdev\n", err);

    return err;
}

/* Module init */
int __init aesd_init_module(void)
{
    dev_t dev = 0;
    int result;

    result = alloc_chrdev_region(&dev, aesd_minor, 1, "aesdchar");
    if (result < 0)
        return result;

    aesd_major = MAJOR(dev);
    memset(&aesd_device, 0, sizeof(struct aesd_dev));
    mutex_init(&aesd_device.lock);
    aesd_circular_buffer_init(&aesd_device.circular_buffer);

    result = aesd_setup_cdev(&aesd_device);
    if (result)
        unregister_chrdev_region(dev, 1);

    return result;
}

/* Module exit */
void __exit aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);
    struct aesd_buffer_entry *entry;
    uint8_t index;

    cdev_del(&aesd_device.cdev);

    if (aesd_device.partial_entry.buffptr)
        kfree(aesd_device.partial_entry.buffptr);

    AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.circular_buffer, index) {
        if (entry->buffptr)
            kfree(entry->buffptr);
    }

    mutex_destroy(&aesd_device.lock);
    unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
