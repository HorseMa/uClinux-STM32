/*
 * $Id: s.ipv6_tunnel.h 1.7 02/08/09 14:09:15-00:00 ville $
 */

#ifndef _NET_IPV6_TUNNEL_H
#define _NET_IPV6_TUNNEL_H

#include <linux/ipv6.h>
#include <linux/netdevice.h>
#include <linux/ipv6_tunnel.h>
#include <linux/skbuff.h>
#include <asm/atomic.h>

#define IPV6_TNL_MAX 128

/* IPv6 tunnel */

struct ipv6_tnl {
	struct ipv6_tnl *next;	/* next tunnel in list */
	struct net_device *dev;	/* virtual device associated with tunnel */
	struct net_device_stats stat;	/* statistics for tunnel device */
	int recursion;		/* depth of hard_start_xmit recursion */
	struct ipv6_tnl_parm parms;	/* tunnel configuration paramters */
	struct flowi fl;	/* flowi template for xmit */
	atomic_t refcnt;	/* nr of identical tunnels used by kernel */
};

#define IPV6_TNL_PRE_ENCAP 0
#define IPV6_TNL_PRE_DECAP 1
#define IPV6_TNL_MAXHOOKS 2

#define IPV6_TNL_DROP 0
#define IPV6_TNL_ACCEPT 1
#define IPV6_TNL_STOLEN 2

typedef int ipv6_tnl_hookfn(struct ipv6_tnl *t, struct sk_buff *skb);

struct ipv6_tnl_hook_ops {
	struct list_head list;
	unsigned int hooknum;
	int priority;
	ipv6_tnl_hookfn *hook;
};

enum ipv6_tnl_hook_priorities {
	IPV6_TNL_PRI_FIRST = INT_MIN,
	IPV6_TNL_PRI_LAST = INT_MAX
};

/* Tunnel encapsulation limit destination sub-option */

struct ipv6_tlv_tnl_enc_lim {
	__u8 type;		/* type-code for option         */
	__u8 length;		/* option length                */
	__u8 encap_limit;	/* tunnel encapsulation limit   */
} __attribute__ ((packed));

#ifdef __KERNEL__
extern struct ipv6_tnl *ipv6_ipv6_tnl_lookup(struct in6_addr *remote,
					     struct in6_addr *local);

extern int ipv6_ipv6_kernel_tnl_add(struct ipv6_tnl_parm *p);

extern int ipv6_ipv6_kernel_tnl_del(struct ipv6_tnl *t);

extern unsigned int ipv6_ipv6_tnl_inc_max_kdev_count(unsigned int n);

extern unsigned int ipv6_ipv6_tnl_dec_max_kdev_count(unsigned int n);

extern unsigned int ipv6_ipv6_tnl_inc_min_kdev_count(unsigned int n);

extern unsigned int ipv6_ipv6_tnl_dec_min_kdev_count(unsigned int n);

extern void ipv6_ipv6_tnl_register_hook(struct ipv6_tnl_hook_ops *reg);

extern void ipv6_ipv6_tnl_unregister_hook(struct ipv6_tnl_hook_ops *reg);
#endif
#endif
