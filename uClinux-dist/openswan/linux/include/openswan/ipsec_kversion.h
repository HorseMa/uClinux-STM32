#ifndef _OPENSWAN_KVERSIONS_H
/*
 * header file for FreeS/WAN library functions
 * Copyright (C) 1998, 1999, 2000  Henry Spencer.
 * Copyright (C) 1999, 2000, 2001  Richard Guy Briggs
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Library General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/lgpl.txt>.
 * 
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
 * License for more details.
 *
 * RCSID $Id: ipsec_kversion.h,v 1.15.2.21 2008-02-17 20:35:35 paul Exp $
 */
#define	_OPENSWAN_KVERSIONS_H	/* seen it, no need to see it again */

/*
 * this file contains a series of atomic defines that depend upon
 * kernel version numbers. The kernel versions are arranged
 * in version-order number (which is often not chronological)
 * and each clause enables or disables a feature.
 */

/*
 * First, assorted kernel-version-dependent trickery.
 */
#include <linux/version.h>
#ifndef KERNEL_VERSION
#define KERNEL_VERSION(x,y,z) (((x)<<16)+((y)<<8)+(z))
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,1,0)
#define HEADER_CACHE_BIND_21
#error "KLIPS is no longer supported on Linux 2.0. Sorry"
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,1,0)
#define SPINLOCK
#define PROC_FS_21
#define NETLINK_SOCK
#define NET_21
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,1,19)
#define net_device_stats enet_statistics
#endif                                                                         

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,0)
#define SPINLOCK_23
#define NETDEV_23
#  ifndef CONFIG_IP_ALIAS
#  define CONFIG_IP_ALIAS
#  endif
#include <linux/socket.h>
#include <linux/skbuff.h>
#include <linux/netlink.h>
#  ifdef NETLINK_XFRM
#  define NETDEV_25
#  endif
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,25)
#define PROC_FS_2325
#undef  PROC_FS_21
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,30)
#define PROC_NO_DUMMY
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,35)
#define SKB_COPY_EXPAND
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,37)
#define IP_SELECT_IDENT
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,50)
# if(LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23) && defined(CONFIG_NETFILTER))
#  define SKB_RESET_NFCT
# elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,23)
#  if defined(CONFIG_NF_CONNTRACK) || defined(CONFIG_NF_CONNTRACK_MODULE)
#   define SKB_RESET_NFCT
#  endif
# endif
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,2)
#define IP_SELECT_IDENT_NEW
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,4)
#define IPH_is_SKB_PULLED
#define SKB_COW_NEW
#define PROTO_HANDLER_SINGLE_PARM
#define IP_FRAGMENT_LINEARIZE 1
#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,4) */
#  ifdef REDHAT_BOGOSITY
#  define IP_SELECT_IDENT_NEW
#  define IPH_is_SKB_PULLED
#  define SKB_COW_NEW
#  define PROTO_HANDLER_SINGLE_PARM
#  endif /* REDHAT_BOGOSITY */
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,4) */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,9)
#define MALLOC_SLAB
#define LINUX_KERNEL_HAS_SNPRINTF
#endif                                                                         

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#define HAVE_NETDEV_PRINTK 1
#define NET_26
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,8)
#define NEED_INET_PROTOCOL
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,12)
#define HAVE_SOCK_ZAPPED
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
#define NET_26_24_SKALLOC
#else
#define NET_26_12_SKALLOC
#endif
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,13)
#define HAVE_SOCK_SECURITY
/* skb->nf_debug disappared completely in 2.6.13 */
#define HAVE_SKB_NF_DEBUG
#endif

#define SYSCTL_IPSEC_DEFAULT_TTL sysctl_ip_default_ttl                      
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,14)
/* skb->stamp changed to skb->tstamp in 2.6.14 */
#define HAVE_TSTAMP
#define HAVE_INET_SK_SPORT
#undef  SYSCTL_IPSEC_DEFAULT_TTL
#define SYSCTL_IPSEC_DEFAULT_TTL IPSEC_DEFAULT_TTL
#else
#define HAVE_SKB_LIST
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18) || SLE_VERSION_CODE >= 655616
#define HAVE_NEW_SKB_LINEARIZE
#endif

/* this is the best we can do to detect XEN, which makes
 *  * patches to linux/skbuff.h, making it look like 2.6.18 version 
 *   */
#ifdef CONFIG_XEN
#define HAVE_NEW_SKB_LINEARIZE
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19)
#define VOID_SOCK_UNREGISTER
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
/* skb->nfmark changed to skb->mark in 2.6.20 */
#define nfmark mark
#else
#define HAVE_KMEM_CACHE_T
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
/* need to include ip.h early, no longer pick it up in skbuff.h */
#include <linux/ip.h>
#  define HAVE_KERNEL_TSTAMP
/* type of sock.sk_stamp changed from timeval to ktime  */
#  define grab_socket_timeval(tv, sock)  { (tv) = ktime_to_timeval((sock).sk_stamp); }
#else
#  define grab_socket_timeval(tv, sock)  { (tv) = (sock).sk_stamp; }
/* internals of struct skbuff changed */
#  define        HAVE_DEV_NEXT
#  define ip_hdr(skb)  ((skb)->nh.iph)
#  define skb_tail_pointer(skb)  ((skb)->tail)
#  define skb_end_pointer(skb)  ((skb)->end)
#  define skb_network_header(skb)  ((skb)->nh.raw)
#  define skb_set_network_header(skb,off)  ((skb)->nh.raw = (skb)->data + (off))
#  define tcp_hdr(skb)  ((skb)->h.th)
#  define udp_hdr(skb)  ((skb)->h.uh)
#  define skb_transport_header(skb)  ((skb)->h.raw)
#  define skb_set_transport_header(skb,off)  ((skb)->h.raw = (skb)->data + (off))
#  define skb_mac_header(skb)  ((skb)->mac.raw)
#  define skb_set_mac_header(skb,off)  ((skb)->mac.raw = (skb)->data + (off))
#endif
/* turn a pointer into an offset for above macros */
#define ipsec_skb_offset(skb, ptr) (((unsigned char *)(ptr)) - (skb)->data)

#ifdef NET_21
#  include <linux/in6.h>
#else
     /* old kernel in.h has some IPv6 stuff, but not quite enough */
#  define	s6_addr16	s6_addr
#  define	AF_INET6	10
#  define uint8_t __u8
#  define uint16_t __u16 
#  define uint32_t __u32 
#  define uint64_t __u64 
#endif

#ifdef NET_21
# define ipsec_kfree_skb(a) kfree_skb(a)
#else /* NET_21 */
# define ipsec_kfree_skb(a) kfree_skb(a, FREE_WRITE)
#endif /* NET_21 */

#ifdef NETDEV_23
#if 0
#ifndef NETDEV_25
#define device net_device
#endif
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
# define ipsec_dev_get(n) dev_get_by_name(&init_net, (n))
# define __ipsec_dev_get(n) __dev_get_by_name(&init_net, (n))
#else
# define ipsec_dev_get dev_get_by_name
# define __ipsec_dev_get __dev_get_by_name
#endif
# define ipsec_dev_put(x) dev_put(x)
# define __ipsec_dev_put(x) __dev_put(x)
# define ipsec_dev_hold(x) dev_hold(x)
#else /* NETDEV_23 */
# define ipsec_dev_get dev_get
# define __ipsec_dev_put(x) 
# define ipsec_dev_put(x)
# define ipsec_dev_hold(x) 
#endif /* NETDEV_23 */

#ifndef SPINLOCK
#  include <linux/bios32.h>
     /* simulate spin locks and read/write locks */
     typedef struct {
       volatile char lock;
     } spinlock_t;

     typedef struct {
       volatile unsigned int lock;
     } rwlock_t;                                                                     

#  define spin_lock_init(x) { (x)->lock = 0;}
#  define rw_lock_init(x) { (x)->lock = 0; }

#  define spin_lock(x) { while ((x)->lock) barrier(); (x)->lock=1;}
#  define spin_lock_irq(x) { cli(); spin_lock(x);}
#  define spin_lock_irqsave(x,flags) { save_flags(flags); spin_lock_irq(x);}

#  define spin_unlock(x) { (x)->lock=0;}
#  define spin_unlock_irq(x) { spin_unlock(x); sti();}
#  define spin_unlock_irqrestore(x,flags) { spin_unlock(x); restore_flags(flags);}

#  define read_lock(x) spin_lock(x)
#  define read_lock_irq(x) spin_lock_irq(x)
#  define read_lock_irqsave(x,flags) spin_lock_irqsave(x,flags)

#  define read_unlock(x) spin_unlock(x)
#  define read_unlock_irq(x) spin_unlock_irq(x)
#  define read_unlock_irqrestore(x,flags) spin_unlock_irqrestore(x,flags)

#  define write_lock(x) spin_lock(x)
#  define write_lock_irq(x) spin_lock_irq(x)
#  define write_lock_irqsave(x,flags) spin_lock_irqsave(x,flags)

#  define write_unlock(x) spin_unlock(x)
#  define write_unlock_irq(x) spin_unlock_irq(x)
#  define write_unlock_irqrestore(x,flags) spin_unlock_irqrestore(x,flags)
#endif /* !SPINLOCK */

#ifndef SPINLOCK_23
#  define spin_lock_bh(x)  spin_lock_irq(x)
#  define spin_unlock_bh(x)  spin_unlock_irq(x)

#  define read_lock_bh(x)  read_lock_irq(x)
#  define read_unlock_bh(x)  read_unlock_irq(x)

#  define write_lock_bh(x)  write_lock_irq(x)
#  define write_unlock_bh(x)  write_unlock_irq(x)
#endif /* !SPINLOCK_23 */

#ifndef HAVE_NETDEV_PRINTK
#define netdev_printk(sevlevel, netdev, msglevel, format, arg...) \
	printk(sevlevel "%s: " format , netdev->name , ## arg)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
#define	PROC_NET	init_net.proc_net
#else
#define	PROC_NET	proc_net
#endif

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,0)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0) 
#include "openswan/ipsec_kern24.h"
#else
#error "kernels before 2.4 are not supported at this time"
#endif
#endif


#endif /* _OPENSWAN_KVERSIONS_H */

/*
 * $Log: ipsec_kversion.h,v $
 * Revision 1.15.2.21  2008-02-17 20:35:35  paul
 * enable HAVE_NEW_SKB_LINEARIZE for Suse Linux SLES10 SP1
 *
 * Revision 1.15.2.20  2007-11-16 06:16:10  paul
 * Fix brackets on SKB_RESET_NFCT case
 *
 * Revision 1.15.2.19  2007-11-16 06:01:27  paul
 * On 2.6.23+, sk->nfct is part of skbut only when CONFIG_NF_CONNTRACK or
 * CONFIG_NF_CONNTRACK_MODUE is set, where previously this was handled with
 * CONFIG_NETFILTER.
 *
 * Revision 1.15.2.18  2007-11-07 14:17:56  paul
 * Xen modifies skb structures, so xen kernels < 2.6.18 need to have
 * HAVE_NEW_SKB_LINEARIZE defined.
 *
 * Revision 1.15.2.17  2007-10-31 19:57:40  paul
 * type of sock.sk_stamp changed from timeval to ktime [dhr]
 *
 * Revision 1.15.2.16  2007-10-30 22:17:02  paul
 * Move the define for ktime_to_timeval() from "not 2.6.22" to "< 2.6.16",
 * where it belongs.
 *
 * Revision 1.15.2.15  2007-10-30 21:44:00  paul
 * added a backport definition for define skb_end_pointer [dhr]
 *
 * Revision 1.15.2.14  2007-10-28 00:26:03  paul
 * Start of fix for 2.6.22+ kernels and skb_tail_pointer()
 *
 * Revision 1.15.2.13  2007/09/05 02:28:27  paul
 * Patch by David McCullough for 2.6.22 compatibility (HAVE_KERNEL_TSTAMP,
 * HAVE_DEV_NEXT and other header surgery)
 *
 * Revision 1.15.2.12  2007/08/10 01:40:49  paul
 * Fix for sock_unregister for 2.6.19 by Sergeil
 *
 * Revision 1.15.2.11  2007/02/20 03:53:16  paul
 * Added comment, made layout consistent with other checks.
 *
 * Revision 1.15.2.10  2007/02/16 19:08:12  paul
 * Fix for compiling on 2.6.20 (nfmark is now called mark in sk_buff)
 *
 * Revision 1.15.2.9  2006/07/29 05:00:40  paul
 * Added HAVE_NEW_SKB_LINEARIZE for 2.6.18+ kernels where skb_linearize
 * only takes 1 argument.
 *
 * Revision 1.15.2.8  2006/05/01 14:31:52  mcr
 * FREESWAN->OPENSWAN in #ifdef.
 *
 * Revision 1.15.2.7  2006/01/11 02:02:59  mcr
 * updated patches and DEFAULT_TTL code to work
 *
 * Revision 1.15.2.6  2006/01/03 19:25:02  ken
 * Remove duplicated #ifdef for TTL fix - bad patch
 *
 * Revision 1.15.2.5  2006/01/03 18:06:33  ken
 * Fix for missing sysctl default ttl
 *
 * Revision 1.15.2.4  2005/11/27 21:40:14  paul
 * Pull down TTL fixes from head. this fixes "Unknown symbol sysctl_ip_default_ttl"
 * in for klips as module.
 *
 * Revision 1.15.2.3  2005/11/22 04:11:52  ken
 * Backport fixes for 2.6.14 kernels from HEAD
 *
 * Revision 1.15.2.2  2005/09/01 01:57:19  paul
 * michael's fixes for 2.6.13 from head
 *
 * Revision 1.15.2.1  2005/08/27 23:13:48  paul
 * Fix for:
 * 7 weeks ago:  	[NET]: Remove unused security member in sk_buff
 * changeset 4280: 	328ea53f5fee
 * parent 4279:	beb0afb0e3f8
 * author: 	Thomas Graf <tgraf@suug.ch>
 * date: 	Tue Jul 5 21:12:44 2005
 * files: 	include/linux/skbuff.h include/linux/tc_ematch/tc_em_meta.h net/core/skbuff.c net/ipv4/ip_output.c net/ipv6/ip6_output.c net/sched/em_meta.c
 *
 * This should fix compilation on 2.6.13(rc) kernels
 *
 * Revision 1.15  2005/07/19 20:02:15  mcr
 * 	sk_alloc() interface change.
 *
 * Revision 1.14  2005/07/08 16:20:05  mcr
 * 	fix for 2.6.12 disapperance of sk_zapped field -> sock_flags.
 *
 * Revision 1.13  2005/05/20 03:19:18  mcr
 * 	modifications for use on 2.4.30 kernel, with backported
 * 	printk_ratelimit(). all warnings removed.
 *
 * Revision 1.12  2005/04/13 22:46:21  mcr
 * 	note that KLIPS does not work on Linux 2.0.
 *
 * Revision 1.11  2004/09/13 02:22:26  mcr
 * 	#define inet_protocol if necessary.
 *
 * Revision 1.10  2004/08/03 18:17:15  mcr
 * 	in 2.6, use "net_device" instead of #define device->net_device.
 * 	this probably breaks 2.0 compiles.
 *
 * Revision 1.9  2004/04/05 19:55:05  mcr
 * Moved from linux/include/freeswan/ipsec_kversion.h,v
 *
 * Revision 1.8  2003/12/13 19:10:16  mcr
 * 	refactored rcv and xmit code - same as FS 2.05.
 *
 * Revision 1.7  2003/07/31 22:48:08  mcr
 * 	derive NET25-ness from presence of NETLINK_XFRM macro.
 *
 * Revision 1.6  2003/06/24 20:22:32  mcr
 * 	added new global: ipsecdevices[] so that we can keep track of
 * 	the ipsecX devices. They will be referenced with dev_hold(),
 * 	so 2.2 may need this as well.
 *
 * Revision 1.5  2003/04/03 17:38:09  rgb
 * Centralised ipsec_kfree_skb and ipsec_dev_{get,put}.
 *
 * Revision 1.4  2002/04/24 07:36:46  mcr
 * Moved from ./klips/net/ipsec/ipsec_kversion.h,v
 *
 * Revision 1.3  2002/04/12 03:21:17  mcr
 * 	three parameter version of ip_select_ident appears first
 * 	in 2.4.2 (RH7.1) not 2.4.4.
 *
 * Revision 1.2  2002/03/08 21:35:22  rgb
 * Defined LINUX_KERNEL_HAS_SNPRINTF to shut up compiler warnings after
 * 2.4.9.  (Andreas Piesk).
 *
 * Revision 1.1  2002/01/29 02:11:42  mcr
 * 	removal of kversions.h - sources that needed it now use ipsec_param.h.
 * 	updating of IPv6 structures to match latest in6.h version.
 * 	removed dead code from freeswan.h that also duplicated kversions.h
 * 	code.
 *
 *
 */
