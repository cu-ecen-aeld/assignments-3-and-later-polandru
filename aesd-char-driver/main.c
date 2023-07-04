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
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
#include "aesd_ioctl.h"
int aesd_major = 0; // use dynamic major
int aesd_minor = 0;
#define BUFFSIZE 25000
MODULE_AUTHOR("Your Name Here"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;
int cc;
loff_t get_size(void);
loff_t get_offset(loff_t off);
int get_cmd(loff_t off);
long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
loff_t aesd_llseek(struct file *filp, loff_t off, int whence);

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
    // from scull
    //  struct scull_dev *dev; /* device information */
    //  dev = container_of(inode->i_cdev, struct scull_dev, cdev);
    //  filp->private_data = dev; /* for other methods */

    aesd_device = *container_of(inode->i_cdev, struct aesd_dev, cdev);
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
    ssize_t retval = 0;
    PDEBUG("read %zu bytes with offset %lld", count, *f_pos);
    /**
     * TODO: handle read
     */

    if (cc > 9)
        return 0;

    if (!mutex_lock_interruptible(&aesd_device.lock))
    {
        int i;
        int size = 0;
        if (cc > 9)
        {
            return 0;
        }

        for (i = 0; i < 10; i++)
        {
            int len = strlen(aesd_device.buffer[i]);
            if (len == 0)
                break;
            size += len;
        }
        loff_t off = *f_pos;
        int r_count = 1;
        aesd_device.r_pos = 0;
        char *tmp = kmalloc(BUFFSIZE, GFP_KERNEL);
        memset(tmp, 0, BUFFSIZE);
        for (; aesd_device.r_pos < 10; aesd_device.r_pos++)
        {
            loff_t val = off - strlen(aesd_device.buffer[aesd_device.r_pos]);
            if (val < 0)
            {
                break;
            }
            off -= strlen(aesd_device.buffer[aesd_device.r_pos]);
        }

      
        if (aesd_device.r_pos == 10)
        {
            return 0;
        }
        strcat(tmp, aesd_device.buffer[aesd_device.r_pos] + off);
        aesd_device.r_pos = (aesd_device.r_pos + 1) % 10;
        for (; r_count < 10; r_count++)
        {
            strcat(tmp, aesd_device.buffer[aesd_device.r_pos]);
            if(*f_pos+strlen(tmp)>= size ) break;
            printk("OFF %lld  LEN: %d SIZE: %d",*f_pos, strlen(tmp), size);
            aesd_device.r_pos = (aesd_device.r_pos + 1) % 10;
        }
        cc = 10;

        copy_to_user(buf, tmp, strlen(tmp));
        printk("%s %d", tmp, strlen(tmp));
        retval = strlen(tmp);
        kfree(tmp);
    }
    //*f_pos += retval;
    mutex_unlock(&aesd_device.lock);

    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                   loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld", count, *f_pos);
    /**
     * TODO: handle write
     */

    if (!mutex_lock_interruptible(&aesd_device.lock))
    {
        char *tmp = kmalloc(count, GFP_KERNEL);
        tmp[count] = '\0';
        copy_from_user(tmp, buf, count);
        if (count > 0)
        {
            if (aesd_device.r_pos == aesd_device.w_pos && strlen(aesd_device.buffer[aesd_device.r_pos]) > 0)
            {
                aesd_device.r_pos = (aesd_device.r_pos + 1) % 10;
            }

            strcat(aesd_device.temp, tmp);
            retval = count;

            if (tmp[count - 1] == '\n')
            {
                count += strlen(aesd_device.temp);
                strncpy(aesd_device.buffer[aesd_device.w_pos], aesd_device.temp, count);
                memset(aesd_device.temp, 0, BUFFSIZE);

                aesd_device.w_pos = (aesd_device.w_pos + 1) % 10;
                cc = 0;
            }
        }
        kfree(tmp);
        //*f_pos += retval;
    }
    mutex_unlock(&aesd_device.lock);

    return retval;
}
long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg){

    if(cmd == AESDCHAR_IOCSEEKTO){
           int i, offset=0;
           struct aesd_seekto seekto;
           copy_from_user(&seekto, (const void*)arg,sizeof(seekto));
            printk(KERN_ALERT"WC %d  WCO %d",seekto.write_cmd,seekto.write_cmd_offset);
            int t = seekto.write_cmd;
           for(i= t - 1; i >= 0 ;i--){
           
            offset += strlen(aesd_device.buffer[i]);
           }
            offset += seekto.write_cmd_offset;
           aesd_llseek(filp,offset,SEEK_SET);
    }
    
    return 0;
}

struct file_operations aesd_fops = {
    .owner = THIS_MODULE,
    .read = aesd_read,
    .write = aesd_write,
    .open = aesd_open,
    .release = aesd_release,
    .unlocked_ioctl = aesd_ioctl,
    .llseek = aesd_llseek};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add(&dev->cdev, devno, 1);
    if (err)
    {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}

int aesd_init_module(void)
{
    dev_t dev = 0;
    int i;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
                                 "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0)
    {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device, 0, sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */

    aesd_device.r_pos = 0;
    aesd_device.w_pos = 0;

    for (i = 0; i < 10; i++)
    {
        aesd_device.buffer[i] = kmalloc(BUFFSIZE, GFP_KERNEL);
        memset(aesd_device.buffer[i], 0, BUFFSIZE);
    }
    mutex_init(&aesd_device.lock);
    aesd_device.temp = kmalloc(BUFFSIZE, GFP_KERNEL);
    memset(aesd_device.temp, 0, BUFFSIZE);

    cc = 0;
    result = aesd_setup_cdev(&aesd_device);

    if (result)
    {
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

    unregister_chrdev_region(devno, 1);
}

loff_t aesd_llseek(struct file *filp, loff_t off, int whence)
{
    int i;
    int size = 0;
    cc = 0;
    for (i = 0; i < 10; i++)
    {
        int len = strlen(aesd_device.buffer[i]);
        if (len == 0)
            break;
        size += len;
    }
    printk(KERN_ALERT "LSEEK %lld SIZE %d", off, size);
    if (!mutex_lock_interruptible(&aesd_device.lock))
    {
        fixed_size_llseek(filp, off, whence, size);
    }
    mutex_unlock(&aesd_device.lock);
    return 0;
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
