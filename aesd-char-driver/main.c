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
#include <linux/uaccess.h>   // copy_to_user, copy_from_user

#include "aesdchar.h"

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

/* File operations structure */
struct file_operations aesd_fops = {
    .owner = THIS_MODULE,
    .read = aesd_read,
    .write = aesd_write,
    .open = aesd_open,
    .release = aesd_release
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
    ssize_t retval = -ENOMEM;
    struct aesd_dev *dev;
    char *new_buf;

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

    dev->partial_entry.buffptr = new_buf;

    if (copy_from_user(dev->partial_entry.buffptr + dev->partial_entry.size, buf, count)) {
        retval = -EFAULT;
        goto out;
    }

    dev->partial_entry.size += count;
    retval = count;

    /* If newline is found, push to circular buffer */
    if (memchr(dev->partial_entry.buffptr, '\n', dev->partial_entry.size)) {
        /* Free oldest entry if buffer is full */
        if (dev->circular_buffer.full) {
            if (dev->circular_buffer.entry[dev->circular_buffer.out_offs].buffptr) {
                kfree(dev->circular_buffer.entry[dev->circular_buffer.out_offs].buffptr);
                dev->circular_buffer.entry[dev->circular_buffer.out_offs].buffptr = NULL;
                dev->circular_buffer.entry[dev->circular_buffer.out_offs].size = 0;
            }
        }

        /* Add new entry */
        aesd_circular_buffer_add_entry(&dev->circular_buffer, &dev->partial_entry);

        /* Reset partial entry */
        dev->partial_entry.buffptr = NULL;
        dev->partial_entry.size = 0;
    }

out:
    mutex_unlock(&dev->lock);
    return retval;
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
