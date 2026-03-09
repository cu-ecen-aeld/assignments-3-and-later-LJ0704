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
#include <linux/slab.h>   // kmalloc, kfree
#include <linux/string.h> // memset
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Likhita Jonnakuti"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
    struct aesd_dev *dev;
    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev;
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    struct aesd_dev *dev = filp->private_data;
    struct aesd_buffer_entry *entry;
    size_t entry_offset;
    size_t bytes_to_read;
    ssize_t retval = 0;

    if (!dev || !buf || !f_pos)
        return -EFAULT;

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    /* Find entry corresponding to current file position */
    entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->circular_buffer,
                                                           *f_pos, &entry_offset);

    if (!entry) {
        retval = 0; // EOF
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
ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    struct aesd_dev *dev = filp->private_data;
    char *new_buf;
    const char *overwritten = NULL;
    ssize_t retval = -ENOMEM;

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

    /* Only push to circular buffer if a newline is found */
    if (memchr(dev->partial_entry.buffptr, '\n', dev->partial_entry.size)) {
        overwritten = aesd_circular_buffer_add_entry(&dev->circular_buffer,
                                                     &dev->partial_entry);
        if (overwritten)
            kfree(overwritten);

        /* Reset partial entry */
        dev->partial_entry.buffptr = NULL;
        dev->partial_entry.size = 0;
    }

out:
    mutex_unlock(&dev->lock);
    return retval;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int __init aesd_init_module(void)
{
    dev_t dev = 0;
    int result;

    result = alloc_chrdev_region(&dev, aesd_minor, 1, "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0)
        return result;

    memset(&aesd_device, 0, sizeof(struct aesd_dev));
    mutex_init(&aesd_device.lock);
    aesd_circular_buffer_init(&aesd_device.circular_buffer);

    result = aesd_setup_cdev(&aesd_device);
    if (result)
        unregister_chrdev_region(dev, 1);

    return result;
}

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
