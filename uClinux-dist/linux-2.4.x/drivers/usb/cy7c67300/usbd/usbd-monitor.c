/*
 * linux/drivers/usbd/usbd-monitor.c - USB Device Cable Monitor
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
USBD_MODULE_INFO("usbd_monitor 0.3");

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


/* Module Parameters ************************************************************************* */

typedef enum monitor_status {
    MONITOR_UNKNOWN,
    MONITOR_LOADING,            // loading due to cable connection interrupt
    MONITOR_LOADED,             // loaded 
    MONITOR_UNLOADING,          // unloading due to cable disconnection interrupt
    MONITOR_UNLOADED,           // unloaded
#ifdef CONFIG_PM
    MONITOR_SUSPENDING,         // suspending due to power management event
    MONITOR_SUSPENDED,          // suspended due to power management event
    MONITOR_RESTORING,          // restoring 
#endif
} monitor_status_t;

/* struct monitor_bi_data -
 *
 * private data structure for this bus interface driver
 */
struct monitor_data {
    monitor_status_t            status;
    struct tq_struct            monitor_bh;
    struct tq_struct            hotplug_bh;

    int                         have_irq;
#ifdef CONFIG_PM
    struct pm_dev              *pm_info;
#endif

};

struct monitor_data monitor;
void monitor_int_hndlr(int irq, void *dev_id, struct pt_regs *regs);

/**
 * monitor_request_irq
 */
int monitor_request_irq(void)
{
#if defined(CONFIG_SA1100_USBCABLE_GPIO)
    int rc;

    printk(KERN_DEBUG"monitor_request_irq: %d %d\n", 
            CONFIG_SA1100_USBCABLE_GPIO, SA1100_GPIO_TO_IRQ(CONFIG_SA1100_USBCABLE_GPIO));

    if ((rc = request_irq(SA1100_GPIO_TO_IRQ(CONFIG_SA1100_USBCABLE_GPIO), 
                    monitor_int_hndlr, SA_SHIRQ, "USBD Monitor", &monitor))) 
    {
        printk(KERN_DEBUG"monitor_request_irq: failed: %d\n", rc);
        return -EINVAL;
    }
    GPDR &= ~GPIO_GPIO(CONFIG_SA1100_USBCABLE_GPIO);
    set_GPIO_IRQ_edge(GPIO_GPIO(CONFIG_SA1100_USBCABLE_GPIO), GPIO_BOTH_EDGES);
#endif
    return 0;
}

/**
 * monitor_free_irq
 */
void monitor_free_irq(void)
{
#if defined(CONFIG_SA1100_USBCABLE_GPIO)
    free_irq(SA1100_GPIO_TO_IRQ(CONFIG_SA1100_USBCABLE_GPIO), NULL); 
#endif
}


/**
 * monitor_connected - connected to cable
 *
 * Return non-zero if via USB cable to USB Hub or Host
 */
int monitor_connected(void) 
{
    int rc = 1;

    /*
     * Architecture specific - determine connect status
     */

    /*
     * SA-1100
     *
     * Only pay attention to the status if we also can control the pullup.
     */
#if defined(CONFIG_SA1100_USBCABLE_GPIO) && defined(CONFIG_SA1100_CONNECT_GPIO)

#ifdef CONFIG_SA1100_CONNECT_ACTIVE_HIGH
    rc = (GPLR & GPIO_GPIO(23)) != 0;
    printk(KERN_DEBUG"udc_connected: ACTIVE_HIGH: %d", rc);
#else
    rc = (GPLR & GPIO_GPIO(23)) == 0;
    printk(KERN_DEBUG"udc_connected: ACTIVE_LOW: %d", rc);
#endif

#endif /* CONFIG_ARCH_SA1100 && CONFIG_SA1100_USBCABLE_GPIO */

    printk(KERN_DEBUG"monitor_connected: %d\n", rc);
    return rc;
}

static char *hotplug_actions[] = {
    "load",
    "suspend",
    "restore-loaded",
    "restore-unloaded",
    "unload"
};
/* indices for the hotplug_actions array, these must match the array */
#define MHA_LOAD               0
#define MHA_SUSPEND            1
#define MHA_RESTORE_LOADED     2
#define MHA_RESTORE_UNLOADED   3
#define MHA_UNLOAD             4

#if defined(CONFIG_USBD_PROCFS) && defined(CONFIG_PM)

#define HOTPLUG_SYNC_TIMEOUT  (10*HZ)

static DECLARE_MUTEX_LOCKED(hotplug_done);
static struct timer_list hotplug_timeout;

static void hotplug_sync_over(unsigned long data)
{
    printk(KERN_ERR "usbdmonitor: warning - hotplug script timed out\n");
    up(&hotplug_done);
}
#endif

static int monitor_exiting = 0;

int monitor_hotplug(int action_ndx)
{
    /* This should probably be serialized - if PM runs in a separate
       context, it would seem possible for someone to "rmmmod usbdmonitor"
       (e.g. via "at") at the same time time PM decides to suspend.
       Unfortunately, there is no airtight way to accomplish that inside
       this module - once PM has called the registered fn, the "race"
       is on :(. */
    int rc;
    if (action_ndx < 0 || action_ndx > MHA_UNLOAD) {
        return(-EINVAL);
    }
    if (monitor_exiting) {
        if (MHA_UNLOAD != action_ndx) {
            return(-EINVAL);
	}
	if (MONITOR_UNLOADED == monitor.status ||
            MONITOR_UNLOADING == monitor.status) {
            /* No need to do it again... */
            return(0);
        }
    }
    printk(KERN_DEBUG "monitor_hotplug: agent: usbd interface: monitor action: %s\n", hotplug_actions[action_ndx]);
#if defined(CONFIG_USBD_PROCFS) && defined(CONFIG_PM)
    /* Sync - fire up the script and wait for it to echo something to
              /proc/usb-monitor (or else PM SUSPEND may not work) */
    init_MUTEX_LOCKED(&hotplug_done);
    /* fire up the script */
    rc = hotplug("usbd", "monitor", hotplug_actions[action_ndx]);
    if (0 == rc) {
        /* wait for the nudge from a write to /proc/usb-monitor */
        init_timer(&hotplug_timeout);
        hotplug_timeout.data = 0;
        hotplug_timeout.function = hotplug_sync_over;
        hotplug_timeout.expires = jiffies + HOTPLUG_SYNC_TIMEOUT;
        add_timer(&hotplug_timeout);
        down_interruptible(&hotplug_done);
        del_timer(&hotplug_timeout);
    }
#else
    /* Async - fire up the script and return */
    rc = hotplug("usbd", "monitor", hotplug_actions[action_ndx]);
#endif
    return(rc);
}

#ifdef CONFIG_PM
/**
 * monitor_suspend
 *
 * Check status, unload if required, set monitor status.
 */
int monitor_suspend(void)
{
    unsigned long flags;
    local_irq_save(flags);
    switch(monitor.status) {
    case MONITOR_UNKNOWN:
    case MONITOR_UNLOADED:
        local_irq_restore(flags);
        return 0;

    case MONITOR_LOADING:
    case MONITOR_UNLOADING:
    case MONITOR_SUSPENDING:
    case MONITOR_RESTORING:
    case MONITOR_SUSPENDED:
        // XXX we should wait for this
        local_irq_restore(flags);
        return -EINVAL; 

    case MONITOR_LOADED:
        monitor.status = MONITOR_SUSPENDING;
        local_irq_restore(flags);
        monitor_hotplug(MHA_SUSPEND);
        monitor.status = MONITOR_SUSPENDED;
        return 0;
    default:
        return -EINVAL;
    }
}


/**
 * hotplug_bh - 
 * @ignored:
 *
 * Check connected status and load/unload as appropriate.
 */
void hotplug_bh(void *ignored)
{
    printk(KERN_DEBUG "hotplug_bh:\n");

    if (monitor_connected()) {
        printk(KERN_DEBUG"monitor_restore: RESTORE_LOADED\n");
        monitor_hotplug(MHA_RESTORE_LOADED);
        monitor.status = MONITOR_LOADED;
    }
    else {
        printk(KERN_DEBUG"monitor_restore: RESTORE_UNLOADED\n");
        monitor_hotplug(MHA_RESTORE_UNLOADED);
        monitor.status = MONITOR_UNLOADED;
    }

    MOD_DEC_USE_COUNT;
}

/**
 * hotplug_schedule_bh -
 */
void hotplug_schedule_bh(void) 
{
    printk(KERN_DEBUG "hotplug_schedule_bh: schedule bh\n");

    if (monitor.hotplug_bh.sync) {
        return;
    }

    MOD_INC_USE_COUNT;
    if (!schedule_task(&monitor.hotplug_bh)) {
        printk(KERN_DEBUG "monitor_schedule_bh: failed\n");
        MOD_DEC_USE_COUNT;
    }
}



/**
 * monitor_restore
 *
 * Check status, load if required, set monitor status.
 */
int monitor_restore(void)
{
    unsigned long flags;
    local_irq_save(flags);
    switch(monitor.status) {
    case MONITOR_SUSPENDED:
        monitor.status = MONITOR_RESTORING;
        local_irq_restore(flags);

        hotplug_schedule_bh();

        return 0;

    case MONITOR_UNKNOWN:
    case MONITOR_UNLOADED:
    case MONITOR_LOADING:
    case MONITOR_UNLOADING:
    case MONITOR_SUSPENDING:
    case MONITOR_RESTORING:
        // XXX we should wait for this
        local_irq_restore(flags);
        return -EINVAL; 

    case MONITOR_LOADED:
        local_irq_restore(flags);
        return 0;
    default:
        return -EINVAL;
    }
}


/**
 * monitor_pm_event
 *
 * Handle power management event
 */
static int monitor_pm_event(struct pm_dev *dev, pm_request_t rqst, void *unused)
{
    int rc;
    /* See comment regarding race condition at start of monitor_hotplug() */
    MOD_INC_USE_COUNT;
    rc = 0;
    switch (rqst) {
    case PM_SUSPEND:
        // Force unloading
        rc = monitor_suspend();
        printk(KERN_ERR "%s: suspend finished (rc=%d)\n",__FUNCTION__,rc);
        break;
    case PM_RESUME:
        // load if required.
        monitor_restore();
    }
    MOD_DEC_USE_COUNT;
    return rc;
}
#endif /* CONFIG_PM */

/**
 * monitor_load
 *
 * Check status, load if required, set monitor status.
 */
int monitor_load(void)
{
    unsigned long flags;
    printk(KERN_DEBUG"monitor_load: \n");
    local_irq_save(flags);
    switch(monitor.status) {
    case MONITOR_UNKNOWN:
    case MONITOR_UNLOADED:
        monitor.status = MONITOR_LOADING;
        local_irq_restore(flags);
        monitor_hotplug(MHA_LOAD);
        monitor.status = MONITOR_LOADED;
        return 0;

    case MONITOR_LOADING:
    case MONITOR_UNLOADING:
#ifdef CONFIG_PM
    case MONITOR_SUSPENDING:
    case MONITOR_SUSPENDED:
    case MONITOR_RESTORING:
#endif
        // XXX we should wait for this
        local_irq_restore(flags);
        return -EINVAL; 

    case MONITOR_LOADED:
        local_irq_restore(flags);
        return 0;
    default:
        return -EINVAL;
    }
}


/**
 * monitor_unload
 *
 * Check status, unload if required, set monitor status.
 */
int monitor_unload(void)
{
    unsigned long flags;
    printk(KERN_DEBUG"monitor_unload: \n");
    local_irq_save(flags);
    switch(monitor.status) {
    case MONITOR_UNKNOWN:
    case MONITOR_UNLOADED:
        local_irq_restore(flags);
        return 0;

    case MONITOR_LOADING:
    case MONITOR_UNLOADING:
#ifdef CONFIG_PM
    case MONITOR_SUSPENDING:
    case MONITOR_SUSPENDED:
    case MONITOR_RESTORING:
#endif
        // XXX we should wait for this
        local_irq_restore(flags);
        return -EINVAL; 

    case MONITOR_LOADED:
        monitor.status = MONITOR_UNLOADING;
        local_irq_restore(flags);
        monitor_hotplug(MHA_UNLOAD);
        monitor.status = MONITOR_UNLOADED;
        return 0;
    default:
        return -EINVAL;
    }
}



/**
* monitor_event - called from monitor interrupt handler
 */
void monitor_event(void)
{
    if (monitor_connected()) {
        monitor_load();
    }
    else {
        monitor_unload();
#if !defined(CONFIG_SA1100_CONNECT_GPIO) 
        monitor_load();
#endif
    }
}

/**
 * monitor_bh - 
 * @ignored:
 *
 * Check connected status and load/unload as appropriate.
 */
void monitor_bh(void *ignored)
{
    printk(KERN_DEBUG "monitor_bh:\n");

    monitor_event();

    MOD_DEC_USE_COUNT;
}


/**
 * monitor_schedule_bh -
 */
void monitor_schedule_bh(void) 
{
    //printk(KERN_DEBUG "monitor_schedule_bh: schedule bh to %s\n", msg);

    if (monitor.monitor_bh.sync) {
        return;
    }

    MOD_INC_USE_COUNT;
    if (!schedule_task(&monitor.monitor_bh)) {
        printk(KERN_DEBUG "monitor_schedule_bh: failed\n");
        MOD_DEC_USE_COUNT;
    }
}

/**
 * monitor_int_hndlr -
 * @irq:
 * @dev_id:
 * @regs
 */
void monitor_int_hndlr(int irq, void *dev_id, struct pt_regs *regs)
{
    printk(KERN_DEBUG "\n");
    printk(KERN_DEBUG "monitor_int_hndlr:\n");
    monitor_schedule_bh();
}

#ifdef CONFIG_USBD_PROCFS
/* Proc Filesystem *************************************************************************** */

/* *
 * usbd_monitor_proc_read - implement proc file system read.
 * @file
 * @buf
 * @count
 * @pos
 *
 * Standard proc file system read function.
 */
static ssize_t
usbd_monitor_proc_read(struct file *file, char *buf, size_t count, loff_t * pos)
{
    unsigned long   page;
    int len = 0;
    int index;

    MOD_INC_USE_COUNT;
    // get a page, max 4095 bytes of data...
    if (!(page = get_free_page(GFP_KERNEL))) {
        MOD_DEC_USE_COUNT;
        return -ENOMEM;
    }

    len = 0;
    index = (*pos)++;

    if (index == 0) {
        len += sprintf((char *)page+len, "usb-monitor status\n");
    }

    if (index==1) {
        switch(monitor.status) {
        case MONITOR_UNKNOWN:
            len += sprintf((char *)page+len, "unknown\n");
            break;
        case MONITOR_UNLOADED:
            len += sprintf((char *)page+len, "unloaded\n");
            break;
        case MONITOR_LOADING:
            len += sprintf((char *)page+len, "loading\n");
            break;
        case MONITOR_UNLOADING:
            len += sprintf((char *)page+len, "unloading\n");
            break;
#ifdef CONFIG_PM
        case MONITOR_SUSPENDING:
            len += sprintf((char *)page+len, "suspending\n");
            break;
        case MONITOR_SUSPENDED:
            len += sprintf((char *)page+len, "suspended\n");
            break;
        case MONITOR_RESTORING:
            len += sprintf((char *)page+len, "restoring\n");
            break;
#endif
        case MONITOR_LOADED:
            len += sprintf((char *)page+len, "loaded\n");
            break;
        default:
            len += sprintf((char *)page+len, "?\n");
        }
    }

    if (len > count) {
        len = -EINVAL;
    }
    else if (len > 0 && copy_to_user(buf, (char *) page, len)) {
        len = -EFAULT;
    }
    free_page(page);
    MOD_DEC_USE_COUNT;
    return len;
}

/* *
 * usbd_monitor_proc_write - implement proc file system write.
 * @file
 * @buf
 * @count
 * @pos
 *
 * Proc file system write function, used to signal monitor actions complete.
 * (Hotplug script (or whatever) writes to the file to signal the completion
 * of the script.)  An ugly hack.
 */
static ssize_t
usbd_monitor_proc_write(
        struct file *file,
        const char *buf,
        size_t count,
        loff_t * pos)
{
    size_t n = count;
    char c;
    MOD_INC_USE_COUNT;
    //printk(KERN_DEBUG "%s: count=%u\n",__FUNCTION__,count);
    while (n > 0) {
        // Not too efficient, but it shouldn't matter
        if (copy_from_user(&c,buf+(count-n),1)) {
            count = -EFAULT;
            break;
        }
        n -= 1;
        //printk(KERN_DEBUG "%s: %u/%u %02x\n",__FUNCTION__,count-n,count,c);
    }
    up(&hotplug_done);
    MOD_DEC_USE_COUNT;
    return(count);
}

static struct file_operations usbd_monitor_proc_operations_functions = {
    read: usbd_monitor_proc_read,
    write: usbd_monitor_proc_write,
};

#endif


/* Module Init ******************************************************************************* */


/**
 * monitor_modinit - commission bus interface driver
 *
 */
static int __init monitor_modinit(void)
{

    printk(KERN_INFO "usbdm: %s\n", __usbd_module_info);

    // Initialize data structures _before_ installing interrupt handlers
    monitor.status = MONITOR_UNKNOWN;

    monitor.monitor_bh.routine = monitor_bh;
    monitor.monitor_bh.data = NULL;
    // XXX monitor.monitor_bh.sync = 0;

    monitor.hotplug_bh.routine = hotplug_bh;
    monitor.hotplug_bh.data = NULL;
    // XXX monitor.hotplug_bh.sync = 0;


    /*
     * Architecture specific - request IRQ
     */

    if (!monitor_request_irq()) {
        monitor.have_irq++;
    }
    else {
        printk(KERN_DEBUG"usbdm: request irq failed\n");
    }

#ifdef CONFIG_PM
    /*
     * Architecture specific - register with power management
     */

    monitor.pm_info = NULL;

    if (!(monitor.pm_info = pm_register(PM_USB_DEV, PM_SYS_UNKNOWN, monitor_pm_event))) {
        printk(KERN_ERR "%s: couldn't register for power management\n", __FUNCTION__);
        if (monitor.have_irq) {
            free_irq(IRQ_GPIO12, &monitor); 
        }
        return 1;
    }
    monitor.pm_info->state = 0;
#endif

#ifdef CONFIG_USBD_PROCFS
    {
        struct proc_dir_entry *p;

        // create proc filesystem entries
        if ((p = create_proc_entry("usb-monitor", 0, 0)) == NULL) {
            if (monitor.have_irq) {
    		monitor_free_irq();
                monitor.have_irq = 0;
            }
#ifdef CONFIG_PM
            if (monitor.pm_info) {
                pm_unregister(monitor.pm_info);
                monitor.pm_info = NULL;
            }
#endif
            return -ENOMEM;
	}
        p->proc_fops = &usbd_monitor_proc_operations_functions;
    }
#endif

    // Find out if the cable is connected 
    // and load USBD modules if we are.
    
    monitor_event();

    printk(KERN_INFO "monitor_modinit: finished\n");

    return 0;
}


/**
 * monitor_modexit - decommission bus interface driver
 *
 */
static void __exit monitor_modexit(void)
{

    printk(KERN_INFO "\n");
    printk(KERN_INFO "monitor_modexit:\n");

    /* Stop any hotplug actions except unload */
    monitor_exiting = 1;

    /*
     * Architecture specific - free appropriate IRQ
     */

    monitor_free_irq();

#ifdef CONFIG_PM
    if (monitor.pm_info) {
        pm_unregister(monitor.pm_info);
        monitor.pm_info = NULL;
    }
#endif

    /*
     * Force unloading
     */
    monitor_hotplug(MHA_UNLOAD);

#ifdef CONFIG_USBD_PROCFS
    // remove proc filesystem entry *AFTER* the last hotplug,
    // in case it is being used for sync hotplug.
    remove_proc_entry("usb-monitor", NULL);
#endif

    return;
}


module_init(monitor_modinit);
module_exit(monitor_modexit);


