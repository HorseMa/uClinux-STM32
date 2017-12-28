/**
*******************************************************************************
*
* \brief	spi_char.c - SPI character device driver
*
* (C) Copyright 2004, Josef Baumgartner (josef.baumgartner@telex.de)
*
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the Free
* Software Foundation; either version 2 of the License, or (at your option)
* any later version.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 675 Mass Ave, Cambridge, MA 02139, USA.
*
******************************************************************************/

/*****************************************************************************/
/* includes */
#include <asm/semaphore.h>
#include <asm/system.h>                 /* cli() and friends */
#include <asm/uaccess.h>
#include <linux/config.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/wait.h>

#include "spi_core.h"

#if ((LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)) || \
     (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,99)))
#error Driver only supports kernel 2.4.x
#endif

/* Include versioning info, if needed */
#if (defined(MODULE) && defined(CONFIG_MODVERSIONS) && !defined(MODVERSIONS))
#define MODVERSIONS
#endif

#if defined(MODVERSIONS)
#include <linux/modversions.h>
#endif

#define SPI_CHAR_VERSION	"1.0"

/**
*******************************************************************************
* \brief        ioctl function for spi driver interface
*
******************************************************************************/
static int spi_ioctl(struct inode *inode, struct file *filp,
                     unsigned int cmd, unsigned long arg)
{
        int ret = 0;
        struct qspi_dev *dev = filp->private_data;

        ret = spi_dev_ioctl (dev, cmd, arg);
        return(ret);
}

/**
*******************************************************************************
* \brief        open function for the spi driver interface
*
******************************************************************************/
static int spi_open(struct inode *inode, struct file *filep)
{
        int num;
        void *ret;

        num = MINOR(filep->f_dentry->d_inode->i_rdev);

        if ((ret = spi_dev_open(num)) == NULL)
            return (-ENOMEM);

        //printk ("filep->private_data: %08x ret: ret%08x\n", (uint)filep->private_data, (uint)ret);
        filep->private_data = ret;

        return(0);
}

/**
*******************************************************************************
* \brief        close function for the spi driver interface
*
******************************************************************************/
static int spi_close(struct inode *inode, struct file *filep)
{
        void * dev;

        dev = filep->private_data;

        spi_dev_release (dev);
        return 0;
}

/**
*******************************************************************************
* \brief        read function for the spi driver interface
*
******************************************************************************/
static ssize_t spi_read(struct file *filep, char *buffer, size_t length,
                         loff_t *off)
{
        void *dev;
        int total = 0;

        dev = filep->private_data;

        total = spi_dev_read (dev, buffer, length, MEM_TYPE_USER);

        return(total);
}

/**
*******************************************************************************
* \brief        write function for the spi driver interface
*
******************************************************************************/
static ssize_t spi_write(struct file *filep, const char *buffer, size_t length,
                         loff_t *off)
{
        void *dev;
        int total = 0;

        dev = filep->private_data;

        total = spi_dev_write (dev, buffer, length, MEM_TYPE_USER);

        return(total);
}


static struct file_operations Fops = {
        owner:          THIS_MODULE,
        read:           spi_read,
        write:          spi_write,
        ioctl:          spi_ioctl,
        open:           spi_open,
        release:        spi_close
};


/**
*******************************************************************************
* \brief        init function for the spi character driver interface
*
******************************************************************************/
int __init spi_char_init(void)
{
        int ret;

        if ((ret = devfs_register_chrdev(SPI_MAJOR, DEVICE_NAME, &Fops) < 0)) {
                printk ("%s device failed with %d\n",
                        "Sorry, registering the character", ret);
                return(ret);
        }

        printk ("SPI character device driver %s\n", SPI_CHAR_VERSION);
        return (ret);
}

/**
*******************************************************************************
* \brief        exit function for the spi character driver interface
*
******************************************************************************/
void __exit spi_char_exit(void)      /* the __exit added by ron  */
{
        int ret;

        /* Unregister the device */
        if ((ret = devfs_unregister_chrdev(SPI_MAJOR, DEVICE_NAME)) < 0)
                printk("Error in unregister_chrdev: %d\n", ret);

        return;
}

module_init(spi_char_init);
module_exit(spi_char_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Josef Baumgartner <josef.baumgartner@telex.de>");
MODULE_DESCRIPTION("SPI-Bus char driver module");

