/*  $Header$
 *  
 *  Copyright (C) 2002 Intersil Americas Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _ISLMVC_H
#define _ISLMVC_H

/* So we can test parts of this driver on a poldhu based board. */
#include <linux/autoconf.h>

#ifdef CONFIG_POLDHU_TEST_DB_DRIVER
#define TEST_ON_POLDHU 
#else
#undef TEST_ON_POLDHU
#endif

/* INCLUDES */

#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/spinlock.h>
#include <device_config.h>
#include <linux/isil_netlink.h>

/* DEFINES */
#define TRUE  1
#define FALSE 0

/*
 * MACROS to get and set the MVC device nr and MVC nr
 */
/* Add the device type, located in the first 16 bits, to var */
#define ADD_DEV_TYPE(var, type) var = ((var & 0x0000FFFF) + (type << 16))
/* Add the device number, located in the last 16 bits, to var */
#define ADD_DEV_NR(var, dev_nr) var = ((var & 0xFFFF0000) + dev_nr)
/* Return the MVC device number, located in the last 16 bits */
#define GET_DEV_NR(var) (var & 0x0000FFFF)
/* Return the device type, located in the first 16 bits */
#define GET_DEV_TYPE(var) ((var & 0xFFFF0000) >> 16)

/* Debug flag to get debug information for specific sections */
#define DBG_TX	            0x00000001
#define DBG_RX	            0x00000002
#define DBG_IRQ	            0x00000004
#define DBG_IOCTL           0x00000008
#define DBG_OPEN            0x00000010
#define DBG_CLOSE           0x00000020
#define DBG_ERROR           0x00000040
#define DBG_SKB	            0x00000080
#define DBG_WDS	            0x00000100
#define DBG_MGMT            0x00000200
#define DBG_PROBE           0x00000400
#define DBG_PIMFOR          0x00000800
#define DBG_FRAME           0x00001000
#define DBG_NETLINK         0x00002000
#define DBG_TRAP            0x00004000
#define	DBG_FUNC_CALL       0x00008000 
#define DBG_FRAME_SANITY    0x00010000

#define DBG_NONE	    0x00000000
#define DBG_ALL	            0xFFFFFFFF

#ifndef NET_DEBUG
#define NET_DEBUG   (DBG_ERROR) 
#endif
static unsigned int net_debug = NET_DEBUG;

/* macros to simplify debug checking */
#define DEBUGLVL(x) if (net_debug & (x))
// #define DEBUG(x,args...)  do{DEBUGLVL(x) printk(## args);}while(0)
#define DEBUG(x,args...) 
#define _DEBUG(x,args...) 

struct client_information
{
    struct client_information *next_client;
    struct client_information **pprev_client;
    char                      mac_addr[ETH_ALEN];
};

/* Information that need to be kept for each device. */
struct net_local
{
    struct net_device_stats	    stats;	/* tx/rx statistics */
#ifdef BLOB_V2
    struct bis_dev_element          *dev_elem;	        /* MVC element desriptor */
#else    
    struct bis_dev_descr_element    *dev_desc;	/* MVC device desriptor */
#endif /* BLOB_V2 */
    int				    maxhead;	/* maximum head, tail and mtu */
    int				    maxtail;	/* sizes for all MVC devices */
    int				    maxmtu;	/* (so we can bridge w/h copy)*/
    struct trap_info                *traps;	/* registrated traps */
    unsigned char	            *acl;
    int				    acl_nr_items;
    unsigned int		    tx_misaligned;
    unsigned int		    rx_misaligned;
    int				    maxnrframes;
    struct msg_frame                *frame_pool;
    struct msg_frame                *frame_freelist;
    unsigned int		    watchdog_timeout;  /* 0 = Off */
    unsigned int		    watchdog_event;
    struct wds_priv                 *wdsp;
    struct client_information       *assoc_table[MAX_ASSOCIATIONS];
    struct mvc_irq_table            *mvcirq_table;
};

char *hwaddr2string(char *a);

#endif /* !defined(_ISLMVC_H) */
