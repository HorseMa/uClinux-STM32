/*
 * @(#) routines to makes kernel 2.4 compatible with 2.6 usage.
 *
 * Copyright (C) 2004 Michael Richardson <mcr@sandelman.ottawa.on.ca>
 * Copyright (C) 2005 - 2008 Paul Wouters <paul@xelerance.com>
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 */

#ifndef _IPSEC_KERN24_H


#ifdef NETDEV_23
#if 0
#ifndef NETDEV_25
#define device net_device
#endif
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

#ifndef HAVE_NETDEV_PRINTK
#define netdev_printk(sevlevel, netdev, msglevel, format, arg...) \
	printk(sevlevel "%s: " format , netdev->name , ## arg)
#endif

#ifndef NET_26
#define sk_receive_queue  receive_queue
#define sk_destruct       destruct
#define sk_reuse          reuse
#define sk_zapped         zapped
#define sk_family         family
#define sk_protocol       protocol
#define sk_protinfo       protinfo
#define sk_sleep          sleep
#define sk_state_change   state_change
#define sk_shutdown       shutdown
#define sk_err            err
#define sk_stamp          stamp
#define sk_socket         socket
#define sk_sndbuf         sndbuf
#define sock_flag(sk, flag)  sk->dead
#define sk_for_each(sk, node, plist) for(sk=*plist; sk!=NULL; sk = sk->next)
#endif

/* deal with 2.4 vs 2.6 issues with module counts */

/* in 2.6, all refcounts are maintained *outside* of the
 * module to deal with race conditions.
 */

#ifdef NET_26
#define KLIPS_INC_USE /* nothing */
#define KLIPS_DEC_USE /* nothing */

#else
#define KLIPS_INC_USE MOD_INC_USE_COUNT
#define KLIPS_DEC_USE MOD_DEC_USE_COUNT
#endif

extern int printk_ratelimit(void);


#define _IPSEC_KERN24_H 1

#endif /* _IPSEC_KERN24_H */

