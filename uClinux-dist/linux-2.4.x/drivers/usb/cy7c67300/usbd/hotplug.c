/*
 * linux/drivers/usbd/usbd-hotplug.c 
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



#if defined(CONFIG_PM)
#include <linux/pm.h>
#endif

#ifdef CONFIG_HOTPLUG

int hotplug(char *agent, char *interface, char *action)
{
    char *argv[3], *envp[5], ifname[12 + IFNAMSIZ], action_str[32];

    if (!hotplug_path [0]) {
        printk (KERN_ERR "hotplug: hotplug_path empty\n");
        return -EINVAL;
    }

    /* printk(KERN_DEBUG "hotplug: %s\n", hotplug_path); */

    if (in_interrupt ()) {
        printk(KERN_ERR "hotplug: in_interrupt\n");
        return -EINVAL;
    }
    sprintf(ifname, "INTERFACE=%s", interface);
    sprintf(action_str, "ACTION=%s", action);

    argv[0] = hotplug_path;
    argv[1] = agent;
    argv[2] = 0;

    envp [0] = "HOME=/";
    envp [1] = "PATH=/sbin:/bin:/usr/sbin:/usr/bin";
    envp [2] = ifname;
    envp [3] = action_str;
    envp [4] = 0;

    return call_usermodehelper(argv[0], argv, envp);
}

#endif /* CONFIG_HOTPLUG */

