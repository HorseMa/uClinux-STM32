/*
 * linux/drivers/usbd/usbd-serialnumber.c - USB Device Cable Monitor
 *
 * Copyright (c) 2000, 2001, 2002 Lineo
 * Copyright (c) 2001 Hewlett Packard
 *
 * By: 
 *      Stuart Lynne <sl@lineo.com>, 
 *      Tom Rushworth <tbr@lineo.com>, 
 *      Bruce Balden <balden@lineo.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */


#include <linux/config.h>
#include <linux/module.h>

#include "usbd-export.h"
#include "usbd-build.h"
#include "usbd-module.h"

MODULE_AUTHOR("sl@lineo.com, tbr@lineo.com");
MODULE_DESCRIPTION("USB Device Monitor");
USBD_MODULE_INFO("usbd_monitor 0.2-alpha");

EXPORT_NO_SYMBOLS;

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <asm/uaccess.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <net/arp.h>
#include <linux/rtnetlink.h>
#include <linux/smp_lock.h>
#include <linux/ctype.h>
#include <linux/timer.h>
#include <linux/string.h>
#include <linux/pkt_sched.h>
#include <asm/io.h>
#include <linux/proc_fs.h>
#include <linux/kmod.h>


/*
 * Architecture specific includes
 */

#ifdef CONFIG_ARCH_SA1100

#include <asm/irq.h>
#include <asm/hardware.h>

#include "usbd.h"
#include "usbd-func.h"
#include "usbd-bus.h"
#include "hotplug.h"
#include "sa1100_bi/sa1100.h"
#endif

#if defined(CONFIG_PM)
#include <linux/pm.h>
#endif

#ifdef CONFIG_SA1100_BITSY
#include <linux/h3600_ts.h>
#endif

/* Module Parameters ************************************************************************* */

#ifdef CONFIG_USBD_PROCFS

/* Proc Filesystem *************************************************************************** */

/* *
 * usbd_device_proc_read - implement proc file system read.
 * @file
 * @buf
 * @count
 * @pos
 *
 * Standard proc file system read function.
 *
 * We let upper layers iterate for us, *pos will indicate which device to return
 * statistics for.
 */
static ssize_t
proc_read_serial(struct file *file, char *buf, size_t count, loff_t * pos)
{
    unsigned long   page;
    int len = 0;
    int index;

    // get a page, max 4095 bytes of data...
    if (!(page = get_free_page(GFP_KERNEL))) {
        return -ENOMEM;
    }

    len = 0;
    index = (*pos)++;

    if (index == 0) {
        len += sprintf((char *)page+len, "Serial: ");

#ifdef CONFIG_SA1100_BITSY
        if (machine_is_bitsy()) {
            char serial_number[22];
            int i;
            int j;
            memset(&serial_number, 0, sizeof(serial_number));
            for (i=0,j=0; i<20; i++,j++) {
                char buf[4];
                h3600_eeprom_read(5 + i, buf, 2);
                serial_number[j] = buf[1];
            }
            len += sprintf((char *)page+len, "%s", serial_number);
        }
#endif

#ifdef CONFIG_SA1100_CALYPSO
        if (machine_is_calypso()) {
            __u32 eerom_serial;
            int i;
            if ((i = CalypsoIicGet( IIC_ADDRESS_SERIAL0, (unsigned char*)&eerom_serial, sizeof(eerom_serial)))) {
                bus->serial_number = eerom_serial;
            }
            len += sprintf((char *)page+len, "%x", eerom_serial);
        }
#endif

        len += sprintf((char *)page+len, "\n");
    }


    if (len > count) {
        len = -EINVAL;
    }
    else if (len > 0 && copy_to_user(buf, (char *) page, len)) {
        len = -EFAULT;
    }
    free_page(page);
    return len;
}

static struct file_operations proc_read_serial_ops = {
read: proc_read_serial,
};


#endif


/* Module Init ******************************************************************************* */


/**
 * monitor_modinit - commission bus interface driver
 *
 */
static int __init monitor_modinit(void)
{
    struct proc_dir_entry *p;

#ifndef CONFIG_USBD_PROCFS
    return -EINVAL;
#else
    printk(KERN_INFO "usbdm: %s\n", __usbd_module_info);

    // create proc filesystem entry
    if ((p = create_proc_entry("usb-serial-number", 0, 0)) == NULL)
        return -ENOMEM;

    p->proc_fops = &proc_read_serial_ops;

    printk(KERN_INFO "monitor_modinit: finished\n");

    return 0;
#endif
}


/**
 * monitor_modexit - decommission bus interface driver
 *
 */
static void __exit monitor_modexit(void)
{

    printk(KERN_INFO "\n");

    remove_proc_entry("usb-serial-number", NULL);

    printk(KERN_INFO "monitor_modexit:\n");

    return;
}


module_init(monitor_modinit);
module_exit(monitor_modexit);


