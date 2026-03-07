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

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    struct aesd_dev *dev = filp->private_data;
    ssize_t retval = 0;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */
        if(mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    // find entry using helper
    struct aesd_buffer_entry *entry;
    size_t entry_offset;

    entry = aesd_circular_buffer_find_entry_offset_for_fpos(
                &dev->buffer, *f_pos, &entry_offset);

    if(!entry) {
        retval = 0;
        goto out;
    }

    size_t bytes_to_copy;

    bytes_to_copy = min(count, entry->size - entry_offset);

    if(copy_to_user(buf, entry->buffptr + entry_offset, bytes_to_copy)) {
        retval = -EFAULT;
        goto out;
    }

    *f_pos += bytes_to_copy;
    retval = bytes_to_copy;

out:
    mutex_unlock(&dev->lock);
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf,size_t count, loff_t *f_pos)
{
    struct aesd_dev *dev = filp->private_data;
    ssize_t retval = -ENOMEM;
    char *kbuf = NULL;
    char *newbuf = NULL;
    struct aesd_buffer_entry entry;

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    kbuf = kmalloc(count, GFP_KERNEL);
    if (!kbuf)
        goto out;

    if (copy_from_user(kbuf, buf, count))
    {
        retval = -EFAULT;
        goto cleanup_kbuf;
    }

    /* Append incoming write data to pending buffer */
    newbuf = krealloc(dev->pending_write,
                      dev->pending_size + count,
                      GFP_KERNEL);

    if (!newbuf)
        goto cleanup_kbuf;

    memcpy(newbuf + dev->pending_size, kbuf, count);

    dev->pending_write = newbuf;
    dev->pending_size += count;

    kfree(kbuf);
    kbuf = NULL;

    /* Check for newline termination */
    if (memchr(dev->pending_write, '\n', dev->pending_size))
    {
        struct aesd_buffer_entry *overwrite_entry = NULL;

        entry.buffptr = dev->pending_write;
        entry.size = dev->pending_size;

        if (dev->buffer.full)
        {
            overwrite_entry =
                &dev->buffer.entry[dev->buffer.in_offs];
        }

        aesd_circular_buffer_add_entry(&dev->buffer, &entry);

        if (overwrite_entry && overwrite_entry->buffptr)
            kfree(overwrite_entry->buffptr);

        dev->pending_write = NULL;
        dev->pending_size = 0;
    }

    retval = count;

    goto out;

cleanup_kbuf:
    if (kbuf)
        kfree(kbuf);

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



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));
    aesd_device.pending_write = NULL;
    aesd_device.pending_size = 0;
    /**
     * TODO: initialize the AESD specific portion of the device
     */
    mutex_init(&aesd_device.lock);
    aesd_circular_buffer_init(&aesd_device.buffer);
    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */
     if(aesd_device.pending_write)
    kfree(aesd_device.pending_write);
    int i;
    for(i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++)
    { 
        if(aesd_device.buffer.entry[i].buffptr)
        kfree(aesd_device.buffer.entry[i].buffptr);
    }
    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
