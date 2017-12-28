/* $USAGI: ndisc.c,v 1.132 2004/02/01 10:05:55 yoshfuji Exp $ */

/*
 *	Neighbour Discovery for IPv6
 *	Linux INET6 implementation 
 *
 *	Authors:
 *	Pedro Roque		<pedro_m@yahoo.com>	
 *	Mike Shaver		<shaver@ingenia.com>
 *
 *	This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

/*
 *	Changes:
 *
 *	Lars Fenneberg			:	fixed MTU setting on receipt
 *						of an RA.
 *
 *	Janos Farkas			:	kmalloc failure checks
 *	Alexey Kuznetsov		:	state machine reworked
 *						and moved to net/core.
 *	Pekka Savola			:	RFC2461 validation
 *	YOSHIFUJI Hideaki @USAGI	:	Verify ND options properly
 *	Ville Nuorvala			:	proxy offers protection
 *						against DAD
 */

#define __NO_VERSION__
#include <linux/config.h>

/* Set to 3 to get tracing... */
#ifdef CONFIG_IPV6_NDISC_DEBUG
#define ND_DEBUG 3
#else
#define ND_DEBUG 1
#endif

#define ND_PRINTK(x...) printk(x)
#define ND_NOPRINTK(x...) do { ; } while(0)
#define ND_PRINTK0 ND_PRINTK
#define ND_PRINTK1 ND_NOPRINTK
#define ND_PRINTK2 ND_NOPRINTK
#define ND_PRINTK3 ND_NOPRINTK
#if ND_DEBUG >= 1
#undef ND_PRINTK1
#define ND_PRINTK1 ND_PRINTK
#endif
#if ND_DEBUG >= 2
#undef ND_PRINTK2
#define ND_PRINTK2 ND_PRINTK
#endif
#if ND_DEBUG >= 3
#undef ND_PRINTK3
#define ND_PRINTK3 ND_PRINTK
#endif

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/sched.h>
#include <linux/net.h>
#include <linux/in6.h>
#include <linux/route.h>
#include <linux/init.h>
#ifdef CONFIG_SYSCTL
#include <linux/sysctl.h>
#endif

#include <linux/if_arp.h>
#include <linux/ipv6.h>
#include <linux/icmpv6.h>
#include <linux/inet.h>

#include <linux/if_tr.h>
#include <linux/jhash.h>

#include <net/sock.h>
#include <net/snmp.h>

#include <net/ipv6.h>
#include <net/protocol.h>
#include <net/ndisc.h>
#include <net/ip6_route.h>
#include <net/addrconf.h>
#include <net/icmp.h>

#include <net/checksum.h>
#include <linux/proc_fs.h>

static struct socket *ndisc_socket;

static u32 ndisc_hash(const void *pkey, const struct net_device *dev);
static int ndisc_constructor(struct neighbour *neigh);
static void ndisc_solicit(struct neighbour *neigh, struct sk_buff *skb);
static void ndisc_error_report(struct neighbour *neigh, struct sk_buff *skb);
static int pndisc_constructor(struct pneigh_entry *n);
static void pndisc_destructor(struct pneigh_entry *n);
static void pndisc_redo(struct sk_buff *skb);

static struct neigh_ops ndisc_generic_ops =
{
	AF_INET6,
	NULL,
	ndisc_solicit,
	ndisc_error_report,
	neigh_resolve_output,
	neigh_connected_output,
	dev_queue_xmit,
	dev_queue_xmit
};

static struct neigh_ops ndisc_hh_ops =
{
	AF_INET6,
	NULL,
	ndisc_solicit,
	ndisc_error_report,
	neigh_resolve_output,
	neigh_resolve_output,
	dev_queue_xmit,
	dev_queue_xmit
};


static struct neigh_ops ndisc_direct_ops =
{
	AF_INET6,
	NULL,
	NULL,
	NULL,
	dev_queue_xmit,
	dev_queue_xmit,
	dev_queue_xmit,
	dev_queue_xmit
};

struct neigh_table nd_tbl =
{
	NULL,
	AF_INET6,
	sizeof(struct neighbour) + sizeof(struct in6_addr),
	sizeof(struct in6_addr),
	ndisc_hash,
	ndisc_constructor,
	pndisc_constructor,
	pndisc_destructor,
	pndisc_redo,
	"ndisc_cache",
        { NULL, NULL, &nd_tbl, 0, NULL, NULL,
		  30*HZ, 1*HZ, 86400*HZ, 30*HZ, 5*HZ, 3, 3, 0, 3, 1*HZ, (8*HZ)/10, 64, 0 },
	30*HZ, 128, 512, 1024,
};

#define NDISC_OPT_SPACE(len) (((len)+2+7)&~7)

static u8 *ndisc_fill_option(u8 *opt, int type, void *data, int data_len)
{
	int space = NDISC_OPT_SPACE(data_len);

	opt[0] = type;
	opt[1] = space>>3;
	memcpy(opt+2, data, data_len);
	data_len += 2;
	opt += data_len;
	if ((space -= data_len) > 0)
		memset(opt, 0, space);
	return opt + space;
}

struct nd_opt_hdr *ndisc_next_option(struct nd_opt_hdr *cur,
				     struct nd_opt_hdr *end)
{
	int type;
	if (!cur || !end || cur >= end)
		return NULL;
	type = cur->nd_opt_type;
	do {
		cur = ((void *)cur) + (cur->nd_opt_len << 3);
	} while(cur < end && cur->nd_opt_type != type);
	return (cur <= end && cur->nd_opt_type == type ? cur : NULL);
}

struct ndisc_options *ndisc_parse_options(u8 *opt, int opt_len,
					  struct ndisc_options *ndopts)
{
	struct nd_opt_hdr *nd_opt = (struct nd_opt_hdr *)opt;

	ND_PRINTK3(KERN_DEBUG 
		   "ndisc_parse_options(opt=%p, opt_len=%d, ndopts=%p)\n",
		   opt, opt_len, ndopts);

	if (!nd_opt || opt_len < 0 || !ndopts)
		return NULL;
	memset(ndopts, 0, sizeof(*ndopts));
	while (opt_len) {
		int l;
		if (opt_len < sizeof(struct nd_opt_hdr)) {
			ND_PRINTK3(KERN_WARNING 
				   "ndisc_parse_options(opt=%p, opt_len=%d, ndopts=%p): truncated options found\n",
				   opt, opt_len, ndopts);
			return NULL;
		}
		l = nd_opt->nd_opt_len << 3;
		if (opt_len < l || l == 0) {
			ND_PRINTK3(KERN_WARNING
				   "ndisc_parse_options(): %s found; type=%d, len=%d (remain: %d)\n",
				   l ? "truncated option" : "zero-length option",
				   nd_opt->nd_opt_type, l, opt_len);
			return NULL;
		}
		switch (nd_opt->nd_opt_type) {
		case ND_OPT_SOURCE_LL_ADDR:
		case ND_OPT_TARGET_LL_ADDR:
		case ND_OPT_MTU:
		case ND_OPT_REDIRECT_HDR:
		case ND_OPT_RTR_ADV_INTERVAL:
			if (ndopts->nd_opt_array[nd_opt->nd_opt_type]) {
				ND_PRINTK3(KERN_WARNING 
					   "ndisc_parse_options(): duplicated ND6 option found: type=%d\n",
					   nd_opt->nd_opt_type);
			} else {
				ndopts->nd_opt_array[nd_opt->nd_opt_type] = nd_opt;
			}
			break;
		case ND_OPT_PREFIX_INFO:
			ndopts->nd_opts_pi_end = nd_opt;
			if (ndopts->nd_opt_array[nd_opt->nd_opt_type] == 0)
				ndopts->nd_opt_array[nd_opt->nd_opt_type] = nd_opt;
			break;
		case ND_OPT_ROUTE_INFO:
#if defined(CONFIG_IPV6_ROUTE_INFO)
			ndopts->nd_opts_ri_end = nd_opt;
			if (ndopts->nd_opt_array[nd_opt->nd_opt_type] == 0)
				ndopts->nd_opt_array[nd_opt->nd_opt_type] = nd_opt;
#endif
			break;
		case ND_OPT_HOME_AGENT_INFO:
			/* These two are handled by the MIPv6 code */
			break;
		default:
			/*
			 * Unknown options must be silently ignored,
			 * to accommodate future extension to the protocol.
			 */
			ND_PRINTK2(KERN_WARNING
				   "ndisc_parse_options(): ignored unsupported option; type=%d, len=%d\n",
				   nd_opt->nd_opt_type, nd_opt->nd_opt_len);
		}
		opt_len -= l;
		nd_opt = ((void *)nd_opt) + l;
	}
	return ndopts;
}

int ndisc_mc_map(struct in6_addr *addr, char *buf, struct net_device *dev, int dir)
{
	switch (dev->type) {
	case ARPHRD_ETHER:
	case ARPHRD_IEEE802:	/* Not sure. Check it later. --ANK */
	case ARPHRD_FDDI:
		ipv6_eth_mc_map(addr, buf);
		return 0;
	case ARPHRD_IEEE802_TR:
		ipv6_tr_mc_map(addr,buf);
		return 0;
	case ARPHRD_ARCNET:
		ipv6_arcnet_mc_map(addr, buf);
		return 0;
	default:
		if (dir) {
			memcpy(buf, dev->broadcast, dev->addr_len);
			return 0;
		}
	}
	return -EINVAL;
}

static u32 ndisc_hash(const void *pkey, const struct net_device *dev)
{
	const u32 *p32 = pkey;
	u32 addr_hash, i;

	addr_hash = 0;
	for (i = 0; i < (sizeof(struct in6_addr) / sizeof(u32)); i++)
		addr_hash ^= *p32++;

	return jhash_2words(addr_hash, dev->ifindex, nd_tbl.hash_rnd);
}

static int ndisc_constructor(struct neighbour *neigh)
{
	struct in6_addr *addr = (struct in6_addr*)&neigh->primary_key;
	struct net_device *dev = neigh->dev;
	struct inet6_dev *in6_dev = in6_dev_get(dev);
	int addr_type;

	if (in6_dev == NULL)
		return -EINVAL;

	addr_type = ipv6_addr_type(addr);
	if (in6_dev->nd_parms)
		neigh->parms = in6_dev->nd_parms;

	if (addr_type&IPV6_ADDR_MULTICAST)
		neigh->type = RTN_MULTICAST;
	else
		neigh->type = RTN_UNICAST;
	if (dev->hard_header == NULL) {
		neigh->nud_state = NUD_NOARP;
		neigh->ops = &ndisc_direct_ops;
		neigh->output = neigh->ops->queue_xmit;
	} else {
		if (addr_type&IPV6_ADDR_MULTICAST) {
			neigh->nud_state = NUD_NOARP;
			ndisc_mc_map(addr, neigh->ha, dev, 1);
		} else if (dev->flags&(IFF_NOARP|IFF_LOOPBACK)) {
			neigh->nud_state = NUD_NOARP;
			memcpy(neigh->ha, dev->dev_addr, dev->addr_len);
			if (dev->flags&IFF_LOOPBACK)
				neigh->type = RTN_LOCAL;
		} else if (dev->flags&IFF_POINTOPOINT) {
			neigh->nud_state = NUD_NOARP;
			memcpy(neigh->ha, dev->broadcast, dev->addr_len);
		}
		if (dev->hard_header_cache)
			neigh->ops = &ndisc_hh_ops;
		else
			neigh->ops = &ndisc_generic_ops;
		if (neigh->nud_state&NUD_VALID)
			neigh->output = neigh->ops->connected_output;
		else
			neigh->output = neigh->ops->output;
	}
	in6_dev_put(in6_dev);
	return 0;
}

static int pndisc_constructor(struct pneigh_entry *n)
{
	struct in6_addr *addr = (struct in6_addr*)&n->key;
	struct in6_addr maddr;
	struct net_device *dev = n->dev;

	if (dev == NULL || __in6_dev_get(dev) == NULL)
		return -EINVAL;
	addrconf_addr_solict_mult(addr, &maddr);
	ipv6_dev_mc_inc(dev, &maddr);
	return 0;
}

static void pndisc_destructor(struct pneigh_entry *n)
{
	struct in6_addr *addr = (struct in6_addr*)&n->key;
	struct in6_addr maddr;
	struct net_device *dev = n->dev;

	if (dev == NULL || __in6_dev_get(dev) == NULL)
		return;
	addrconf_addr_solict_mult(addr, &maddr);
	ipv6_dev_mc_dec(dev, &maddr);
}



static int
ndisc_build_ll_hdr(struct sk_buff *skb, struct net_device *dev,
		   struct in6_addr *daddr, struct neighbour *neigh, int len)
{
	unsigned char ha[MAX_ADDR_LEN];
	unsigned char *h_dest = NULL;

	skb_reserve(skb, (dev->hard_header_len + 15) & ~15);

	if (dev->hard_header) {
		if (ipv6_addr_type(daddr) & IPV6_ADDR_MULTICAST) {
			ndisc_mc_map(daddr, ha, dev, 1);
			h_dest = ha;
		} else if (neigh) {
			read_lock_bh(&neigh->lock);
			if (neigh->nud_state&NUD_VALID) {
				memcpy(ha, neigh->ha, dev->addr_len);
				h_dest = ha;
			}
			read_unlock_bh(&neigh->lock);
		} else {
			neigh = neigh_lookup(&nd_tbl, daddr, dev);
			if (neigh) {
				read_lock_bh(&neigh->lock);
				if (neigh->nud_state&NUD_VALID) {
					memcpy(ha, neigh->ha, dev->addr_len);
					h_dest = ha;
				}
				read_unlock_bh(&neigh->lock);
				neigh_release(neigh);
			}
		}

		if (dev->hard_header(skb, dev, ETH_P_IPV6, h_dest, NULL, len) < 0)
			return 0;
	}

	return 1;
}

static void inline ndisc_update(struct neighbour *neigh, 
				u8 *lladdr, u32 flags)
{
	int notify;
	write_lock_bh(&neigh->lock);
	notify = __neigh_update(neigh, lladdr, NUD_STALE, flags);
#ifdef CONFIG_ARPD
	if (notify > 0 && neigh->parms->app_probes) {
		write_unlock_bh(&neigh->lock);
		neigh_app_notify(neigh);
	} else
#endif
	write_unlock_bh(&neigh->lock);
}

/*
 *	Send a Neighbour Advertisement
 */

void ndisc_send_na(struct net_device *dev, struct neighbour *neigh,
		   struct in6_addr *daddr, struct in6_addr *solicited_addr,
		   int router, int solicited, int override, int inc_opt) 
{
	struct in6_addr tmpaddr;
	struct inet6_ifaddr *ifp;
	struct sock *sk = ndisc_socket->sk;
	struct in6_addr *src_addr;
	struct nd_msg *msg;
	int len;
	struct sk_buff *skb;
	struct inet6_dev *idev;
	int err;

	/* for proxy or anycast, solicited_addr != src_addr */
	ifp = ipv6_get_ifaddr(solicited_addr, dev);
	if (ifp) {
		src_addr = solicited_addr;
		in6_ifa_put(ifp);
	} else {
		if (ipv6_dev_get_saddr(dev, daddr, &tmpaddr, sk->net_pinfo.af_inet6.use_tempaddr)) {
#ifdef CONFIG_IPV6_NDISC_DEBUG
			char abuf[64];
			in6_ntop(daddr, abuf);
			ND_PRINTK2(KERN_WARNING 
				   "ndisc_send_na(): could not get source address for %s on device %p(%s)\n", 
				   abuf, dev, dev ? dev->name : "<null>");
#endif
			return;
		}
		src_addr = &tmpaddr;
	}

	len = sizeof(struct nd_msg);

	if (inc_opt) {
		if (dev->addr_len)
			len += NDISC_OPT_SPACE(dev->addr_len);
		else
			inc_opt = 0;
	}

	skb = sock_alloc_send_skb(sk, MAX_HEADER + len + dev->hard_header_len + 15,
				  1, &err);

	if (skb == NULL) {
		ND_PRINTK1(KERN_WARNING 
			   "send_na: alloc skb failed\n");
		return;
	}

	if (ndisc_build_ll_hdr(skb, dev, daddr, neigh, len) == 0) {
		kfree_skb(skb);
		return;
	}

	ip6_nd_hdr(sk, skb, dev, src_addr, daddr, IPPROTO_ICMPV6, len);

	msg = (struct nd_msg *) skb_put(skb, len);
        msg->icmph.icmp6_type = NDISC_NEIGHBOUR_ADVERTISEMENT;
        msg->icmph.icmp6_code = 0;
        msg->icmph.icmp6_cksum = 0;

        msg->icmph.icmp6_unused = 0;
        msg->icmph.icmp6_router    = router;
        msg->icmph.icmp6_solicited = solicited;
        msg->icmph.icmp6_override  = !!override;

        /* Set the target address. */
	ipv6_addr_copy(&msg->target, solicited_addr);

	if (inc_opt)
		ndisc_fill_option(msg->opt, ND_OPT_TARGET_LL_ADDR, dev->dev_addr, dev->addr_len);

	/* checksum */
	msg->icmph.icmp6_cksum = csum_ipv6_magic(src_addr, daddr, len, 
						 IPPROTO_ICMPV6,
						 csum_partial((__u8 *) msg, 
							      len, 0));

	dev_queue_xmit(skb);

	idev = in6_dev_get(dev);
	ICMP6_INC_STATS(idev,Icmp6OutNeighborAdvertisements);
	ICMP6_INC_STATS(idev,Icmp6OutMsgs);
	if (idev)
		in6_dev_put(idev);
}        

void ndisc_send_ns(struct net_device *dev, struct neighbour *neigh,
		   struct in6_addr *solicit,
		   struct in6_addr *daddr, struct in6_addr *saddr,
		   int dad) 
{
        struct sock *sk = ndisc_socket->sk;
        struct sk_buff *skb;
	struct inet6_dev *idev;
        struct nd_msg *msg;
	struct in6_addr addr_buf;
        int len;
	int err;
	int send_llinfo;

#if CONFIG_IPV6_NDISC_DEBUG
	char abuf1[128], abuf2[128], abuf3[128];

	if (saddr)
		in6_ntop(saddr, abuf1);
	if (daddr)
		in6_ntop(daddr, abuf2);
	if (solicit)
		in6_ntop(solicit, abuf3);
	ND_PRINTK3(KERN_DEBUG 
		   "ndisc_send_ns(): saddr=%s, daddr=%s, solicit=%s, dad=%d\n",
		   saddr ? abuf1 : "<null>",
		   daddr ? abuf2 : "<null>",
		   solicit ? abuf3 : "<null>",
		   dad);
#endif

	if (dad) {
		memset(&addr_buf, 0, sizeof(addr_buf));
		saddr = &addr_buf;
		send_llinfo = 0;
	} else {
		if (saddr == NULL) {
			if (ipv6_get_lladdr(dev, &addr_buf))
				return;
			saddr = &addr_buf;
		}
		send_llinfo = !ipv6_addr_any(saddr) && dev->addr_len;
	}

	len = sizeof(struct nd_msg);
	if (send_llinfo)
		len += NDISC_OPT_SPACE(dev->addr_len);

	skb = sock_alloc_send_skb(sk, MAX_HEADER + len + dev->hard_header_len + 15,
				  1, &err);

	if (skb == NULL) {
		ND_PRINTK1(KERN_WARNING
			   "send_ns: alloc skb failed\n");
		return;
	}

	if (ndisc_build_ll_hdr(skb, dev, daddr, neigh, len) == 0) {
		kfree_skb(skb);
		return;
	}

	ip6_nd_hdr(sk, skb, dev, saddr, daddr, IPPROTO_ICMPV6, len);

	msg = (struct nd_msg *)skb_put(skb, len);
	msg->icmph.icmp6_type = NDISC_NEIGHBOUR_SOLICITATION;
	msg->icmph.icmp6_code = 0;
	msg->icmph.icmp6_cksum = 0;
	msg->icmph.icmp6_unused = 0;

	/* Set the target address. */
	ipv6_addr_copy(&msg->target, solicit);

	if (send_llinfo)
		ndisc_fill_option(msg->opt, ND_OPT_SOURCE_LL_ADDR, dev->dev_addr, dev->addr_len);

	/* checksum */
	msg->icmph.icmp6_cksum = csum_ipv6_magic(&skb->nh.ipv6h->saddr,
						 daddr, len, 
						 IPPROTO_ICMPV6,
						 csum_partial((__u8 *) msg, 
							      len, 0));
#if CONFIG_IPV6_NDISC_DEBUG
	if (saddr)
		in6_ntop(saddr, abuf1);
	if (daddr)
		in6_ntop(daddr, abuf2);
	if (solicit)
		in6_ntop(solicit, abuf3);
	ND_PRINTK3(KERN_DEBUG 
		   "ndisc_send_ns(): -> saddr=%s, daddr=%s, solicit=%s, option_ok=%d\n",
		  saddr ? abuf1 : "<null>",
		  daddr ? abuf2 : "<null>",
		  solicit ? abuf3 : "<null>",
		  send_llinfo);
#endif

	/* send it! */
	dev_queue_xmit(skb);

	idev = in6_dev_get(dev);
	ICMP6_INC_STATS(idev,Icmp6OutNeighborSolicits);
	ICMP6_INC_STATS(idev,Icmp6OutMsgs);
	if (idev)
		in6_dev_put(idev);
}

void ndisc_send_rs(struct net_device *dev, struct in6_addr *saddr,
		   struct in6_addr *daddr)
{
	struct sock *sk = ndisc_socket->sk;
        struct sk_buff *skb;
	struct inet6_dev *idev;
	struct rs_msg *hdr;
        int len;
	int err;

	len = sizeof(struct icmp6hdr);
	if (dev->addr_len)
		len += NDISC_OPT_SPACE(dev->addr_len);

	skb = sock_alloc_send_skb(sk, MAX_HEADER + len + dev->hard_header_len + 15,
				  1, &err);

	if (skb == NULL) {
		ND_PRINTK1(KERN_WARNING
			   "send_rs: alloc skb failed\n");
		return;
	}

	if (ndisc_build_ll_hdr(skb, dev, daddr, NULL, len) == 0) {
		kfree_skb(skb);
		return;
	}

	ip6_nd_hdr(sk, skb, dev, saddr, daddr, IPPROTO_ICMPV6, len);

	hdr = (struct rs_msg *)skb_put(skb, len);
        hdr->icmph.icmp6_type = NDISC_ROUTER_SOLICITATION;
        hdr->icmph.icmp6_code = 0;
        hdr->icmph.icmp6_cksum = 0;
        hdr->icmph.icmp6_unused = 0;

	if (dev->addr_len)
		ndisc_fill_option(hdr->opt, ND_OPT_SOURCE_LL_ADDR, dev->dev_addr, dev->addr_len);

	/* checksum */
	hdr->icmph.icmp6_cksum = csum_ipv6_magic(&skb->nh.ipv6h->saddr, daddr, len,
						 IPPROTO_ICMPV6,
						 csum_partial((__u8 *) hdr, len, 0));

	/* send it! */
	ND_PRINTK3(KERN_DEBUG 
		   "ndisc_send_rs: send RS message\n");
	dev_queue_xmit(skb);

	idev = in6_dev_get(dev);
	ICMP6_INC_STATS(idev,Icmp6OutRouterSolicits);
	ICMP6_INC_STATS(idev,Icmp6OutMsgs);
	if (idev)
		in6_dev_put(idev);
}

static void ndisc_error_report(struct neighbour *neigh, struct sk_buff *skb)
{
	/*
	 *	"The sender MUST return an ICMP
	 *	 destination unreachable"
	 */
	dst_link_failure(skb);
	kfree_skb(skb);
}

/* Called with locked neigh: either read or both */

static void ndisc_solicit(struct neighbour *neigh, struct sk_buff *skb)
{
	struct in6_addr *saddr = NULL;
	struct in6_addr mcaddr;
	struct net_device *dev = neigh->dev;
	struct in6_addr *target = (struct in6_addr *)&neigh->primary_key;
	int probes = atomic_read(&neigh->probes);

	if (skb && ipv6_chk_addr(&skb->nh.ipv6h->saddr, dev))
		saddr = &skb->nh.ipv6h->saddr;

	read_lock_bh(&neigh->lock);
	if ((probes -= neigh->parms->ucast_probes) < 0) {
		if (!(neigh->nud_state&NUD_VALID))
			ND_PRINTK1(KERN_WARNING 
				   "ndisc_solicit(): trying to ucast probe in NUD_INVALID\n");
		read_unlock_bh(&neigh->lock);
		ndisc_send_ns(dev, neigh, target, target, saddr, 0);
	} else if ((probes -= neigh->parms->app_probes) < 0) {
		read_unlock_bh(&neigh->lock);
#ifdef CONFIG_ARPD
		neigh_app_ns(neigh);
#endif
	} else {
		read_unlock_bh(&neigh->lock);
		addrconf_addr_solict_mult(target, &mcaddr);
		ndisc_send_ns(dev, NULL, target, &mcaddr, saddr, 0);
	}
}

#define IPV6READYLOGO
#ifdef IPV6READYLOGO
static int ndisc_getfrag( const void *data, struct in6_addr *addr, char *buff,
		unsigned int offset, unsigned int len)
{
	ND_PRINTK1(KERN_DEBUG "ndisc_getfrag len=%d \n", len);
	memcpy( buff, data+offset, len);
	return 0;
}	

static void no_neigh_send_na( struct net_device *dev, struct sk_buff *oldskb)
	// copy&modify from icmp.c : static void icmpv6_echo_reply(struct sk_buff *skb)
	// Need more work if define CONFIG_IPV6_IPSEC
{
	struct sock *sk = ndisc_socket->sk;
	struct nd_msg *msg;
	int len;
	struct sk_buff *skb;
	struct inet6_dev *idev;
	int err;

	int ipsec_hdrlen = 0;
	int ipsec_authhdrlen = 0;

	struct in6_addr *daddr ,*saddr;
	struct flowi fl;

	daddr = &oldskb->nh.ipv6h->saddr;
	saddr = &oldskb->nh.ipv6h->daddr;

	if ((ipv6_addr_type(saddr) & (IPV6_ADDR_MULTICAST|IPV6_ADDR_ANYCAST))
#ifdef CONFIG_IPV6_ANYCAST
			|| ipv6_chk_acast_addr(0, saddr)
#endif
	   )
		saddr = NULL;
	len = sizeof(struct nd_msg);
//no opt         len += NDISC_OPT_SPACE(dev->addr_len);
	skb = sock_alloc_send_skb(sk, MAX_HEADER + len + dev->hard_header_len + 15 + ipsec_hdrlen,0, &err);

	if (skb == NULL) {
		ND_PRINTK1(KERN_DEBUG "no_neigh_send_ns: alloc skb failed\n");
		return;
	}
#if 0  // Can not make ll here, because ns is w/o ll
	if (ndisc_build_ll_hdr(skb, dev, daddr, NULL, len + ipsec_hdrlen) == 0
	   ) {
		ND_PRINTK1(KERN_DEBUG "no_neigh_send_ns: build_ll_hdr failed\n");
		kfree_skb(skb);
		return;
	}
#endif
	ip6_nd_hdr(sk, skb, dev, saddr, daddr, IPPROTO_ICMPV6, len + ipsec_authhdrlen);
	msg = (struct nd_msg *) skb_put(skb, len);

	msg->icmph.icmp6_type = NDISC_NEIGHBOUR_ADVERTISEMENT;
	msg->icmph.icmp6_code = 0;
	msg->icmph.icmp6_cksum = 0;

	msg->icmph.icmp6_unused = 0;
	msg->icmph.icmp6_router    = 1; //router;
	msg->icmph.icmp6_solicited = 1; //solicited;
	msg->icmph.icmp6_override  = 0; //!!override;


	/* Set the target address. */
	ipv6_addr_copy(&msg->target, saddr);

	//        if (send_llinfo)
	// no opt                ndisc_fill_option((void*)(msg+1), ND_OPT_TARGET_LL_ADDR, dev->dev_addr, dev->addr_len);

	/* checksum */
	msg->icmph.icmp6_cksum = csum_ipv6_magic(&skb->nh.ipv6h->saddr,
			daddr, len,
			IPPROTO_ICMPV6,
			csum_partial((__u8 *) msg,
				len, 0));

	fl.proto = IPPROTO_ICMPV6;
	fl.nl_u.ip6_u.daddr = &oldskb->nh.ipv6h->saddr;
	fl.nl_u.ip6_u.saddr = saddr;
	fl.oif = skb->dev->ifindex;
	fl.fl6_flowlabel = 0;
	fl.uli_u.icmpt.type = NDISC_NEIGHBOUR_ADVERTISEMENT; 
	fl.uli_u.icmpt.code = 0;
//may need more effort ?      if (icmpv6_xmit_lock_bh())
//  			              return;

	ip6_build_xmit(sk, ndisc_getfrag, msg, &fl, len, NULL, -1, -1, MSG_DONTWAIT);
	idev = in6_dev_get(dev);
	ICMP6_INC_STATS(idev,Icmp6OutNeighborSolicits);
	ICMP6_INC_STATS(idev,Icmp6OutMsgs);
	if (idev)
		in6_dev_put(idev);

	kfree_skb(skb);
//may need more effort ?  icmpv6_xmit_unlock_bh();
}
#endif
static void ndisc_recv_ns(struct sk_buff *skb)
{
	struct nd_msg *msg = (struct nd_msg *) skb->h.raw;
	struct in6_addr *saddr = &skb->nh.ipv6h->saddr;
	struct in6_addr *daddr = &skb->nh.ipv6h->daddr;
	struct neighbour *neigh;
	u8 *lladdr = NULL;
	int lladdrlen = 0;
	u32 ndoptlen = skb->len - sizeof(*msg);
	struct ndisc_options ndopts;
	struct net_device *dev = skb->dev;
	struct inet6_ifaddr *ifp;
	int addr_type = ipv6_addr_type(saddr);

	if (skb->len < sizeof(*msg))
		return;

	if (addr_type == IPV6_ADDR_ANY) {
		/* dst has to be solicited node multicast address */
		if (!(daddr->s6_addr32[0] == htonl(0xff020000) &&
		      daddr->s6_addr32[1] == htonl(0x00000000) &&
		      daddr->s6_addr32[2] == htonl(0x00000001) &&
		      daddr->s6_addr [12] == 0xff )) {
			if (net_ratelimit())
				ND_PRINTK2(KERN_WARNING 
					   "ICMP6 NS: bad DAD packet (wrong ip6 dst)\n");
			goto bad;
		}
	}

	if (ipv6_addr_type(&msg->target)&IPV6_ADDR_MULTICAST) {
		if (net_ratelimit())
			ND_PRINTK2(KERN_WARNING 
				   "ICMP6 NS: bad ND target (multicast)\n");
		goto bad;
	}

	if (!ndisc_parse_options(msg->opt, ndoptlen, &ndopts)) {
		if (net_ratelimit())
			ND_PRINTK2(KERN_WARNING 
				   "ICMP6 ND: invalid ND option, ignored\n");
		goto bad;
	}

	if (ndopts.nd_opts_src_lladdr) {
		lladdr = (u8*)(ndopts.nd_opts_src_lladdr + 1);
		lladdrlen = ndopts.nd_opts_src_lladdr->nd_opt_len << 3;
		if (lladdrlen != NDISC_OPT_SPACE(dev->addr_len))
			goto bad;
	}

	/* XXX: RFC2461 7.1.1:
	 * 	If the IP source address is the unspecified address, there
	 *	MUST NOT be source link-layer address option in the message.
	 *
	 *	NOTE! Linux kernel < 2.4.4 broke this rule.
	 */
	if (addr_type == IPV6_ADDR_ANY && lladdr) {
		if (net_ratelimit())
			ND_PRINTK2(KERN_WARNING 
				   "ICMP6 NS: bad DAD packet (link-layer address option)\n");
		goto bad;
	}

	if ((ifp = ipv6_get_ifaddr(&msg->target, dev)) != NULL) {
		if (ifp->flags & IFA_F_TENTATIVE) {
			ND_PRINTK1(KERN_INFO 
				   "ICMP6 NS : Received DAD NS but our address is TENTATIVE.\n");
			/* Address is tentative. If the source
			   is unspecified address, it is someone
			   does DAD, otherwise we ignore solicitations
			   until DAD timer expires.
			 */
			if (addr_type == IPV6_ADDR_ANY) {
				if (dev->type == ARPHRD_IEEE802_TR) { 
					/*
					 * IMHO, this should be handled in
					 * network drivers.
					 * - yoshfuji (2001/01/07)
					 */
					struct trh_hdr *thdr = (struct trh_hdr *)skb->mac.raw;
					if (((thdr->saddr[0] ^ dev->dev_addr[0]) & 0x7f) ||
					    memcmp(&thdr->saddr[1], &dev->dev_addr[1], dev->addr_len - 1))
						addrconf_dad_failure(ifp) ; 
				} else
					addrconf_dad_failure(ifp);
			} else
				in6_ifa_put(ifp);
			return;
		}
	
		if (addr_type == IPV6_ADDR_ANY) {
			struct in6_addr maddr;

			ipv6_addr_all_nodes(&maddr);
			ndisc_send_na(dev, NULL, &maddr, &ifp->addr, 
				      ifp->idev->cnf.forwarding, 0, 1, 1);
			in6_ifa_put(ifp);
			return;
		}

		if (addr_type & IPV6_ADDR_UNICAST) {
			int inc = ipv6_addr_type(daddr)&IPV6_ADDR_MULTICAST;

			if (inc)
				NEIGH_CACHE_STAT_INC(&nd_tbl, rcv_probes_mcast);
			else
				NEIGH_CACHE_STAT_INC(&nd_tbl, rcv_probes_ucast);

			/* 
			 *	update / create cache entry
			 *	for the source address
			 */

			neigh = __neigh_lookup(&nd_tbl, saddr, skb->dev, !inc || lladdr || !skb->dev->addr_len);
			if (neigh) {
				/* XXX: !skb->dev->addr_len case? */
				ndisc_update(neigh, lladdr, NEIGH_UPDATE_F_IP6NS);
				ndisc_send_na(dev, neigh, saddr, &ifp->addr, 
					      ifp->idev->cnf.forwarding, 1, inc, inc);
				neigh_release(neigh);
			} else {
#define IPV6READYLOGO
#ifdef IPV6READYLOGO
				no_neigh_send_na(dev, skb);
#else
				if (net_ratelimit())
					ND_PRINTK3(KERN_DEBUG 
						   "ICMP6 NS: Received Solicited NS but ignored.\n");
#endif
			}
		}
		in6_ifa_put(ifp);
	} else if (ipv6_chk_acast_addr(dev, &msg->target)) {
		struct inet6_dev *idev = in6_dev_get(dev);

		/* anycast */

		if (!idev) {
			/* XXX: count this drop? */
			return;
		}

		if (addr_type == IPV6_ADDR_ANY) {
			struct in6_addr maddr;

			ipv6_addr_all_nodes(&maddr);
			ndisc_send_na(dev, NULL, &maddr, &msg->target,
				      idev->cnf.forwarding, 0, 0, 1);
			in6_dev_put(idev);
			return;
		}

		if (addr_type & IPV6_ADDR_UNICAST) {
			int inc = ipv6_addr_type(daddr)&IPV6_ADDR_MULTICAST;
			if (inc)  
				NEIGH_CACHE_STAT_INC(&nd_tbl, rcv_probes_mcast);
 			else
				NEIGH_CACHE_STAT_INC(&nd_tbl, rcv_probes_ucast);

			/*
			 *   update / create cache entry
			 *   for the source address
			 */

			neigh = __neigh_lookup(&nd_tbl, saddr, skb->dev,
						!inc || lladdr || !skb->dev->addr_len);

			if (neigh) {
				/* XXX: !skb->dev->addr_len case? */
				ndisc_update(neigh, lladdr, NEIGH_UPDATE_F_IP6NS);
				ndisc_send_na(dev, neigh, saddr, &msg->target,
					      idev->cnf.forwarding, 1, 0, inc);
				neigh_release(neigh);
			} else if (net_ratelimit())
				ND_PRINTK3(KERN_DEBUG 
					   "ICMP6 NS: Received Solicited NS but ignored.\n");
			}
		in6_dev_put(idev);
	} else {
		struct inet6_dev *in6_dev = in6_dev_get(dev);

		if (in6_dev && in6_dev->cnf.forwarding &&
		    (addr_type & IPV6_ADDR_UNICAST) &&
			pneigh_lookup(&nd_tbl, &msg->target, dev, 0)) {
			int inc = ipv6_addr_type(daddr)&IPV6_ADDR_MULTICAST;

			if (skb->stamp.tv_sec == 0 ||
			    skb->pkt_type == PACKET_HOST ||
			    inc == 0 ||
			    in6_dev->nd_parms->proxy_delay == 0) {
				struct neighbour *neigh;
				if (inc)
					NEIGH_CACHE_STAT_INC(&nd_tbl, 
							rcv_probes_mcast);
				else
				neigh = neigh_event_ns(&nd_tbl, lladdr, saddr, dev);

				if (neigh) {
					/* XXX: !skb->dev->addr_len case? */
					ndisc_update(neigh, lladdr, 
						     NEIGH_UPDATE_F_IP6NS);
					ndisc_send_na(dev, neigh, saddr, &msg->target,
						      0, 1, 0, inc);
					neigh_release(neigh);
				}
			} else {
				struct sk_buff *n = skb_clone(skb, GFP_ATOMIC);
				if (n)
					pneigh_enqueue(&nd_tbl, in6_dev->nd_parms, n);
				in6_dev_put(in6_dev);
				return;
			}
		}
		if (in6_dev)
			in6_dev_put(in6_dev);
		
	}
bad:
	return;
}

static void ndisc_recv_na(struct sk_buff *skb)
{
	struct net_device *dev = skb->dev;
	struct ipv6hdr *ip6 = skb->nh.ipv6h;
	struct in6_addr *saddr = &ip6->saddr;
	struct in6_addr *daddr = &ip6->daddr;
	struct nd_msg *msg = (struct nd_msg *) skb->h.raw;
	unsigned long ndoptlen = skb->len - sizeof(*msg);
	u8 *lladdr = NULL;
	int lladdrlen = 0;
	struct inet6_ifaddr *ifp;
	struct neighbour *neigh;
	struct ndisc_options ndopts;

	if (skb->len < sizeof(struct nd_msg))
		return;

	if (ipv6_addr_type(&msg->target) & IPV6_ADDR_MULTICAST) {
		if (net_ratelimit()) {
			char abuf[128];
			in6_ntop(&msg->target, abuf);
			ND_PRINTK2(KERN_WARNING 
				   "ICMP6 NA: invalid target address %s\n", 
				   abuf);
		}
		goto bad;
	}

	if ((ipv6_addr_type(daddr)&IPV6_ADDR_MULTICAST) &&
	    msg->icmph.icmp6_solicited) {
		if (net_ratelimit())
			ND_PRINTK2(KERN_WARNING 
				   "ICMP6 NA: solicited NA is multicasted\n");
		return;
	}
	if (!ndisc_parse_options(msg->opt, ndoptlen, &ndopts)) {
		if (net_ratelimit())
			ND_PRINTK2(KERN_WARNING 
				   "ICMP6 NA: invalid ND option, ignored\n");
		goto bad;
	}
	if (ndopts.nd_opts_tgt_lladdr) {
		lladdr = (u8*)(ndopts.nd_opts_tgt_lladdr + 1);
		lladdrlen = ndopts.nd_opts_tgt_lladdr->nd_opt_len << 3;
		if (lladdrlen != NDISC_OPT_SPACE(skb->dev->addr_len))
			goto bad;
	}
	if ((ifp = ipv6_get_ifaddr(&msg->target, dev))) {
		if (ifp->flags & IFA_F_TENTATIVE) {
			addrconf_dad_failure(ifp);
			return;
		}
		/* What should we make now? The advertisement
		   is invalid, but ndisc specs say nothing
		   about it. It could be misconfiguration, or
		   an smart proxy agent tries to help us :-)
		*/
		if (net_ratelimit())
			ND_PRINTK1(KERN_WARNING 
				   "ICMP6 NA: someone advertise our address on %s!\n",
			   ifp->idev->dev->name);
		in6_ifa_put(ifp);
		return;
	}
	neigh = neigh_lookup(&nd_tbl, &msg->target, skb->dev);

	if (neigh) {
		int notify = 0;
		int was_router = 0;

		write_lock_bh(&neigh->lock);
		if (!(neigh->nud_state&~NUD_FAILED))
			goto ignore;

		was_router = neigh->flags&NTF_ROUTER;

		notify = __neigh_update(neigh, lladdr,
					msg->icmph.icmp6_solicited ? NUD_REACHABLE : NUD_STALE,
					(NEIGH_UPDATE_F_IP6NA|
					 (msg->icmph.icmp6_override ? NEIGH_UPDATE_F_OVERRIDE : 0) |
					 (msg->icmph.icmp6_router   ? NEIGH_UPDATE_F_ISROUTER : 0)));

		if (was_router && !(neigh->flags&NTF_ROUTER)) {
			/* XXX: race, ok? */
			/*
			 *	Change: router to host
			 */
			struct rt6_info *rt;
			rt = rt6_get_dflt_router(saddr, skb->dev);
			if (rt) {
				/* It is safe only because
				   we are in BH */
				dst_release(&rt->u.dst);
				ip6_del_rt(rt, NULL, NULL);
			}
		}
ignore:
#ifdef CONFIG_ARPD
		if (notify > 0 && neigh->parms->app_probes) {
			write_unlock_bh(&neigh->lock);
			neigh_app_notify(neigh);
		} else
#endif
		write_unlock_bh(&neigh->lock);
		neigh_release(neigh);
	}
bad:
	return;
}

static void ndisc_recv_rs(struct sk_buff *skb)
{
	struct rs_msg *rs_msg = (struct rs_msg *) skb->h.raw;
	unsigned long ndoptlen = skb->len - sizeof(*rs_msg);
	struct neighbour *neigh;
	struct inet6_dev *idev;
	struct in6_addr *saddr = &skb->nh.ipv6h->saddr;
	struct ndisc_options ndopts;
	u8 *lladdr = NULL;
	int lladdrlen = 0;

	ND_PRINTK3(KERN_DEBUG "%s()\n", __FUNCTION__);

	if (skb->len < sizeof(*rs_msg))
		return;

	idev = in6_dev_get(skb->dev);
	if (!idev) {
		ND_PRINTK1(KERN_WARNING 
			   "%s: can't find in6 device\n",
			   __FUNCTION__);
		return;
	}

	/* Ignore if I'm not a router */
	if (!idev->cnf.forwarding || idev->cnf.accept_ra)
		goto out;

	/* sanity check */
	if (!ndisc_parse_options(rs_msg->opt, ndoptlen, &ndopts)) {
		if (net_ratelimit())
			ND_PRINTK2(KERN_WARNING 
				   "ICMP6 NS: invalid ND option, ignored\n");
		goto out;
	}

	/* Don't update NCE if src = :: */
	if (ipv6_addr_any(saddr)) {
		/*
		 * this is ok but ignore because this indicates that
		 * source has no ip address assigned yet.
		 */
		goto out;
	}

	if (ndopts.nd_opts_src_lladdr) {
		lladdr = (u8 *)(ndopts.nd_opts_src_lladdr + 1);
		lladdrlen = ndopts.nd_opts_src_lladdr->nd_opt_len << 3;
		if (lladdrlen != NDISC_OPT_SPACE(skb->dev->addr_len))
			goto out;
	}

	neigh = __neigh_lookup(&nd_tbl, saddr, skb->dev, 1);
	if (neigh) {
		ndisc_update(neigh, lladdr, NEIGH_UPDATE_F_IP6RS);
		neigh_release(neigh);
	}
out:
	in6_dev_put(idev);
}

static void ndisc_recv_ra(struct sk_buff *skb)
{
        struct ra_msg *ra_msg = (struct ra_msg *) skb->h.raw;
	unsigned long ndoptlen = skb->len - sizeof(*ra_msg);
	struct neighbour *neigh;
	struct inet6_dev *in6_dev;
	struct rt6_info *rt = NULL;
	int lifetime;
	struct in6_addr *saddr = &skb->nh.ipv6h->saddr;
	struct ndisc_options ndopts;
	int pref = 0;

	ND_PRINTK3(KERN_DEBUG "%s()\n", __FUNCTION__);

	if (skb->len < sizeof(*ra_msg))
		return;

	if ((ipv6_addr_type(saddr)&(IPV6_ADDR_UNICAST|IPV6_ADDR_LINKLOCAL)) != (IPV6_ADDR_UNICAST|IPV6_ADDR_LINKLOCAL)) {
		if (net_ratelimit()) {
			char abuf[128];
			in6_ntop(saddr, abuf);
			ND_PRINTK2(KERN_WARNING 
				   "ICMP6 RA: src %s is not link-local\n", abuf);
		}
		return;
	}

	/*
	 *	set the RA_RECV flag in the interface
	 */

	in6_dev = in6_dev_get(skb->dev);
	if (in6_dev == NULL) {
		ND_PRINTK1(KERN_WARNING 
			   "ICMP6 RA: can't find in6 device\n");
		return;
	}

	if (in6_dev->cnf.forwarding || !in6_dev->cnf.accept_ra)
		goto out;

	if (!ndisc_parse_options(ra_msg->opt, ndoptlen, &ndopts)) {
		if (net_ratelimit())
			ND_PRINTK2(KERN_WARNING 
				   "ICMP6 RA: invalid ND option, ignored\n");
		goto out;
	}

	if (in6_dev->if_flags & IF_RS_SENT) {
		/*
		 *	flag that an RA was received after an RS was sent
		 *	out on this interface.
		 */
		in6_dev->if_flags |= IF_RA_RCVD;
	}

	/*
	 * Remember the managed/otherconf flags from most recently
	 * received RA message (RFC 2462) -- yoshfuji
	 */
	in6_dev->if_flags = (in6_dev->if_flags & ~(IF_RA_MANAGED |
				IF_RA_OTHERCONF)) |
				(ra_msg->icmph.icmp6_addrconf_managed ?
					IF_RA_MANAGED : 0) |
				(ra_msg->icmph.icmp6_addrconf_other ?
					IF_RA_OTHERCONF : 0);

	lifetime = ntohs(ra_msg->icmph.icmp6_rt_lifetime);
	ND_PRINTK3(KERN_DEBUG
		   "ICMP6 RA: router lifetime %u\n", lifetime);

#ifdef CONFIG_IPV6_ROUTER_PREF
	pref = IPV6_SIGNEDPREF(ra_msg->icmph.icmp6_router_pref);
	if (pref < -1) {
		if (net_ratelimit())
			ND_PRINTK2(KERN_WARNING
				   "ICMP6 RA: invalid RA preference; zero lifetime\n");
		lifetime = 0;
	}
#endif
	
	rt = rt6_get_dflt_router(saddr, skb->dev);

	if (rt && lifetime == 0) {
		ip6_del_rt(rt, NULL, NULL);
		rt = NULL;
	}

	if (rt == NULL && lifetime) {
		ND_PRINTK2(KERN_DEBUG
			   "ndisc_rdisc: adding default router; lifetime=%u\n", 
			   lifetime);

		rt = rt6_add_dflt_router(saddr, skb->dev, pref);
		if (rt == NULL) {
			ND_PRINTK1(KERN_WARNING 
				   "%s(): route_add failed\n",
				   __FUNCTION__);
			in6_dev_put(in6_dev);
			return;
		}

		neigh = rt->rt6i_nexthop;
		if (neigh == NULL) {
			ND_PRINTK1(KERN_WARNING 
				   "%s(): add default router: null neighbour\n",
				   __FUNCTION__);
			dst_release(&rt->u.dst);
			in6_dev_put(in6_dev);
			return;
		}
		neigh->flags |= NTF_ROUTER;

		/*
		 *	If we where using an "all destinations on link" route
		 *	delete it
		 */

		rt6_purge_dflt_routers(RTF_ALLONLINK);
	}

	if (rt) {
		rt->rt6i_expires = jiffies + (HZ * lifetime);
#ifdef CONFIG_IPV6_ROUTER_PREF
		 rt->rt6i_flags = (rt->rt6i_flags & ~RTF_PREF_MASK) | RTF_PREF(pref);
#endif
	}

	if (ra_msg->icmph.icmp6_hop_limit)
		in6_dev->cnf.hop_limit = ra_msg->icmph.icmp6_hop_limit;

	/*
	 *	Update Reachable Time and Retrans Timer
	 */

	if (in6_dev->nd_parms) {
		unsigned long rtime = ntohl(ra_msg->retrans_timer);

		if (rtime && rtime/1000 < MAX_SCHEDULE_TIMEOUT/HZ) {
			rtime = (rtime*HZ)/1000;
			if (rtime < HZ/10)
				rtime = HZ/10;
			in6_dev->nd_parms->retrans_time = rtime;
		}

		rtime = ntohl(ra_msg->reachable_time);
		if (rtime && rtime/1000 < MAX_SCHEDULE_TIMEOUT/(3*HZ)) {
			rtime = (rtime*HZ)/1000;

			if (rtime < HZ/10)
				rtime = HZ/10;

			if (rtime != in6_dev->nd_parms->base_reachable_time) {
				in6_dev->nd_parms->base_reachable_time = rtime;
				in6_dev->nd_parms->reachable_time = neigh_rand_reach_time(rtime);
			}
		}
	}

	/*
	 *	Process options.
	 */

#if defined(CONFIG_IPV6_ROUTE_INFO)
	if (ndopts.nd_opts_ri) {
		struct nd_opt_hdr *p;
		for (p = ndopts.nd_opts_ri;
		     p;
		     p = ndisc_next_option(p, ndopts.nd_opts_ri_end)) {
			rt6_route_rcv(skb->dev, (u8*)p, (p->nd_opt_len) << 3, saddr);
		}
	}
#endif
	if (ndopts.nd_opts_pi) {
		struct nd_opt_hdr *p;
		for (p = ndopts.nd_opts_pi; 
		     p;
		     p = ndisc_next_option(p, ndopts.nd_opts_pi_end)) {
			addrconf_prefix_rcv(skb->dev, (u8*)p, (p->nd_opt_len) << 3);
		}
	}
	if (ndopts.nd_opts_mtu) {
		u32 mtu;
		memcpy(&mtu, ((u8*)(ndopts.nd_opts_mtu+1))+2, sizeof(mtu));
		mtu = ntohl(mtu);
		if (mtu < IPV6_MIN_MTU || mtu > skb->dev->mtu) {
			if (net_ratelimit())
				ND_PRINTK2(KERN_DEBUG 
					   "ICMP6 RA: router advertisement with invalid mtu = %d\n", 
					   mtu);
		} else if (in6_dev->cnf.mtu6 != mtu) {
			in6_dev->cnf.mtu6 = mtu;
			if (rt)
				rt->u.dst.pmtu = mtu;
			rt6_mtu_change(skb->dev, mtu);
		}
	}
	if (rt && (neigh = rt->rt6i_nexthop) != NULL) {
		u8 *lladdr = NULL;
		int lladdrlen;
		if (ndopts.nd_opts_src_lladdr) {
			lladdr = (u8*)((ndopts.nd_opts_src_lladdr)+1);
			lladdrlen = ndopts.nd_opts_src_lladdr->nd_opt_len << 3;
			if (lladdrlen != NDISC_OPT_SPACE(skb->dev->addr_len))
				goto out;
		}
		ndisc_update(neigh, lladdr, NEIGH_UPDATE_F_IP6RA);
	}
	if (ndopts.nd_opts_tgt_lladdr || ndopts.nd_opts_rh) {
		if (net_ratelimit())
			ND_PRINTK2(KERN_WARNING 
				   "ICMP6 RA: got illegal option with RA");
	}
  out:
	if (rt)
		dst_release(&rt->u.dst);
	in6_dev_put(in6_dev);
}

static void ndisc_redirect_rcv(struct sk_buff *skb)
{
	struct inet6_dev *in6_dev;
	struct rd_msg *rdmsg;
	unsigned long ndoptlen = skb->len - sizeof(*rdmsg);
	u8 *lladdr = NULL;
	int lladdrlen = 0;
	struct in6_addr *saddr = &skb->nh.ipv6h->saddr;
	struct in6_addr *dest;
	struct in6_addr *target;	/* new first hop to destination */
	struct neighbour *neigh;
	struct rt6_info *rt6;
	int on_link = 0;
	struct ndisc_options ndopts;

	if (skb->len < sizeof(struct rd_msg))
		return;

	if (!(ipv6_addr_type(saddr) & IPV6_ADDR_LINKLOCAL)) {
		if (net_ratelimit())
			ND_PRINTK2(KERN_WARNING 
				   "ICMP6 Redirect: source address is not linklocal\n");
		return;
	}

	rdmsg = (struct rd_msg *) skb->h.raw;
	target = &rdmsg->rd_target;
	dest = &rdmsg->rd_dst;

	if (ipv6_addr_type(dest) & IPV6_ADDR_MULTICAST) {
		if (net_ratelimit())
			ND_PRINTK2(KERN_WARNING 
				   "ICMP6 Redirect: redirect for multicast addr\n");
		return;
	}

	if (ipv6_addr_cmp(dest, target) == 0) {
		on_link = 1;
	} else if (!(ipv6_addr_type(target) & IPV6_ADDR_LINKLOCAL)) {
		if (net_ratelimit())
			ND_PRINTK2(KERN_WARNING 
				   "ICMP6 Redirect: target address is not linklocal\n");
		return;
	}

#ifndef CONFIG_IPV6_NDISC_NEW
	rt6 = rt6_lookup(dest, NULL, skb->dev->ifindex, RT6_LOOKUP_FLAG_STRICT|RT6_LOOKUP_FLAG_NOUSE);
	if (!rt6) {
		char sbuf[128], dbuf[128], tbuf[128];
		in6_ntop(saddr, sbuf);
		in6_ntop(dest, dbuf);
		in6_ntop(target, tbuf);
		if (net_ratelimit())
			ND_PRINTK2(KERN_WARNING 
				   "ICMP6 Redirect: no route found for redirect dst:"
				   "src=%s, dst=%s, tgt=%s\n",
				   sbuf, dbuf, tbuf);
		return;
	} else if (ipv6_addr_cmp(saddr, &rt6->rt6i_gateway) != 0) {
		char sbuf[128], dbuf[128], tbuf[128], gbuf[128];
		in6_ntop(saddr, sbuf);
		in6_ntop(dest, dbuf);
		in6_ntop(target, tbuf);
		in6_ntop(&rt6->rt6i_gateway, gbuf);
		if (net_ratelimit())
			ND_PRINTK2(KERN_WARNING 
				   "ICMP6 Redirect: not equal to gw-for-src=%s (must be same): "
				   "src=%s, dst=%s, tgt=%s\n",
				   gbuf, sbuf, dbuf, tbuf);
		dst_release(&rt6->u.dst);
		return;
	}
	dst_release(&rt6->u.dst);
#endif

	in6_dev = in6_dev_get(skb->dev);
	if (!in6_dev)
		return;
	if (in6_dev->cnf.forwarding || !in6_dev->cnf.accept_redirects) {
		in6_dev_put(in6_dev);
		return;
	}

	/* XXX: RFC2461 8.1: 
	 *	The IP source address of the Redirect MUST be the same as the current
	 *	first-hop router for the specified ICMP Destination Address.
	 */
		
	/* passed validation tests */

	if (!ndisc_parse_options(rdmsg->opt, ndoptlen, &ndopts)) {
		char sbuf[128], dbuf[128], tbuf[128];
		in6_ntop(saddr, sbuf);
		in6_ntop(dest, dbuf);
		in6_ntop(target, tbuf);
		if (net_ratelimit())
			ND_PRINTK2(KERN_WARNING
				   "ICMP6 Redirect: invalid ND options, rejected: "
				   "src=%s, dst=%s, tgt=%s\n",
				   sbuf, dbuf, tbuf);
		goto out;
	}
	if (ndopts.nd_opts_tgt_lladdr) {
		lladdr = (u8*)(ndopts.nd_opts_tgt_lladdr + 1);
		lladdrlen = ndopts.nd_opts_tgt_lladdr->nd_opt_len << 3;
		if (lladdrlen != NDISC_OPT_SPACE(skb->dev->addr_len))
			goto out;
	}

	/*
	   We install redirect only if nexthop state is valid.
	 */

	neigh = __neigh_lookup(&nd_tbl, target, skb->dev, 1);
	if (neigh) {
#ifndef CONFIG_IPV6_NDISC_NEW
		int notify;
		write_lock_bh(&neigh->lock);
		notify = __neigh_update(neigh, lladdr, NUD_STALE, 
					NEIGH_UPDATE_F_IP6REDIRECT|
					(on_link ? 0 : (NEIGH_UPDATE_F_OVERRIDE_VALID_ISROUTER|
							NEIGH_UPDATE_F_ISROUTER)));
#ifdef CONFIG_ARPD
		if (notify > 0 && !neigh->parms->app_probes)
			notify = 0;
#endif
		if (neigh->nud_state&NUD_VALID) {
			write_unlock_bh(&neigh->lock);
			rt6_redirect(dest, &skb->nh.ipv6h->saddr, neigh, NULL, on_link);
		} else {
			__neigh_event_send(neigh, NULL);
			write_unlock_bh(&neigh->lock);
		}
#ifdef CONFIG_ARPD
		if (notify > 0)
			neigh_app_notify(neigh);
#endif
#else
		rt6_redirect(dest, &skb->nh.ipv6h->saddr, neigh, lladdr, on_link);
#endif
		neigh_release(neigh);
	}
out:
	in6_dev_put(in6_dev);
}

void ndisc_send_redirect(struct sk_buff *skb, struct neighbour *neigh,
			 struct in6_addr *target)
{
	struct sock *sk = ndisc_socket->sk;
	int len = sizeof(struct rd_msg);
	struct sk_buff *buff;
	struct rd_msg *rdmsg;
	struct in6_addr saddr_buf;
	struct net_device *dev;
	struct inet6_dev *idev;
	struct rt6_info *rt;
	u8 *opt;
	int rd_len;
	int err;
	int hlen;
	u8 ha[MAX_ADDR_LEN];

	dev = skb->dev;
	rt = rt6_lookup(&skb->nh.ipv6h->saddr, NULL, dev->ifindex, RT6_LOOKUP_FLAG_STRICT);

	if (rt == NULL)
		return;

	if (rt->rt6i_flags & RTF_GATEWAY) {
		if (net_ratelimit())
			ND_PRINTK2(KERN_WARNING 
				   "redirect: not a neighbour\n");
		dst_release(&rt->u.dst);
		return;
	}
	if (!xrlim_allow(&rt->u.dst, 1*HZ)) {
		dst_release(&rt->u.dst);
		return;
	}
	dst_release(&rt->u.dst);

	if (dev->addr_len) {
		read_lock_bh(&neigh->lock);
		if (neigh->nud_state&NUD_VALID) {
			len  += NDISC_OPT_SPACE(dev->addr_len);
			memcpy(ha, neigh->ha, dev->addr_len);
			read_unlock_bh(&neigh->lock);
		} else {
			read_unlock_bh(&neigh->lock);
			/* If nexthop is not valid, do not redirect!
			   We will make it later, when will be sure,
			   that it is alive.
			 */
			return;
		}
	}

	rd_len = min_t(unsigned int,
		     IPV6_MIN_MTU-sizeof(struct ipv6hdr)-len, skb->len + 8);
	rd_len &= ~0x7;
	len += rd_len;

	if (ipv6_get_lladdr(dev, &saddr_buf)) {
 		if (net_ratelimit())
			ND_PRINTK2(KERN_WARNING 
				   "redirect: no link_local addr for dev\n");
 		return;
 	}

	buff = sock_alloc_send_skb(sk, MAX_HEADER + len + dev->hard_header_len + 15,
				   1, &err);
	if (buff == NULL) {
		if (net_ratelimit())
			ND_PRINTK1(KERN_WARNING 
				   "redirect: alloc_skb failed\n");
		return;
	}

	hlen = 0;

	if (ndisc_build_ll_hdr(buff, dev, &skb->nh.ipv6h->saddr, NULL, len) == 0) {
		kfree_skb(buff);
		return;
	}

	ip6_nd_hdr(sk, buff, dev, &saddr_buf, &skb->nh.ipv6h->saddr,
		   IPPROTO_ICMPV6, len);

	rdmsg = (struct rd_msg *) skb_put(buff, len);

	memset(rdmsg, 0, sizeof(struct rd_msg));
	rdmsg->icmph.icmp6_type = NDISC_REDIRECT;

	/*
	 *	copy target and destination addresses
	 */

	ipv6_addr_copy(&rdmsg->rd_target, target);
	ipv6_addr_copy(&rdmsg->rd_dst, &skb->nh.ipv6h->daddr);

	opt = rdmsg->opt;

	/*
	 *	include target_address option
	 */

	if (dev->addr_len)
		opt = ndisc_fill_option(opt, ND_OPT_TARGET_LL_ADDR, ha, dev->addr_len);

	/*
	 *	build redirect option and copy skb over to the new packet.
	 */

	memset(opt, 0, 8);	
	*(opt++) = ND_OPT_REDIRECT_HDR;
	*(opt++) = (rd_len >> 3);
	opt += 6;

	memcpy(opt, skb->nh.ipv6h, rd_len - 8);

	rdmsg->icmph.icmp6_cksum = csum_ipv6_magic(&saddr_buf, 
					&skb->nh.ipv6h->saddr,
					len, IPPROTO_ICMPV6,
					csum_partial((u8 *) rdmsg, len, 0));

	dev_queue_xmit(buff);

	idev = in6_dev_get(dev);
	ICMP6_INC_STATS(idev,Icmp6OutRedirects);
	ICMP6_INC_STATS(idev,Icmp6OutMsgs);
	if (idev)
		in6_dev_put(idev);
}

static void pndisc_redo(struct sk_buff *skb)
{
	ndisc_rcv(skb);
	kfree_skb(skb);
}

int ndisc_rcv(struct sk_buff *skb)
{
	struct nd_msg *msg = (struct nd_msg *) skb->h.raw;

	__skb_push(skb, skb->data-skb->h.raw);

	if (skb->nh.ipv6h->hop_limit != 255) {
		if (net_ratelimit())
			ND_PRINTK2(KERN_WARNING
				   "ICMP6 NDISC: fake message with non-255 Hop Limit received: %d\n",
				   skb->nh.ipv6h->hop_limit);
		return 0;
	}

	if (msg->icmph.icmp6_code != 0) {
		if (net_ratelimit())
			ND_PRINTK2(KERN_WARNING
				   "ICMP6 NDISC: code is not zero\n");
		return 0;
	}
	
	switch (msg->icmph.icmp6_type) {
	case NDISC_NEIGHBOUR_SOLICITATION:
		ndisc_recv_ns(skb);
		break;

	case NDISC_NEIGHBOUR_ADVERTISEMENT:
		ndisc_recv_na(skb);
		break;

	case NDISC_ROUTER_ADVERTISEMENT:
		ndisc_recv_ra(skb);
		break;

	case NDISC_ROUTER_SOLICITATION:
		ndisc_recv_rs(skb);
		break;

	case NDISC_REDIRECT:
		ndisc_redirect_rcv(skb);
		break;
	};

	return 0;
}

#ifdef CONFIG_PROC_FS
#ifdef CONFIG_IPV6_NDISC_DEBUG
static int ndisc_get_info(char *buffer, char **start, off_t offset, int length)
{
	int len=0;
	off_t pos=0;
	int size;
	unsigned long now = jiffies;
	int i;

	for (i = 0; i <= NEIGH_HASHMASK; i++) {
		struct neighbour *neigh;

		read_lock_bh(&nd_tbl.lock);
		for (neigh = nd_tbl.hash_buckets[i]; neigh; neigh = neigh->next) {
			int j;

			size = 0;
			for (j=0; j<16; j++) {
				sprintf(buffer+len+size, "%02x", neigh->primary_key[j]);
				size += 2;
			}

			read_lock(&neigh->lock);
			size += sprintf(buffer+len+size,
				       " %02x %02x %02x %02x %08lx %08lx %08x %04x %04x %04x %8s ", i,
				       128,
				       neigh->type,
				       neigh->nud_state,
				       now - neigh->used,
				       now - neigh->confirmed,
				       neigh->parms->reachable_time,
				       neigh->parms->gc_staletime,
				       atomic_read(&neigh->refcnt) - 1,
				       neigh->flags | (!neigh->hh ? 0 : (neigh->hh->hh_output==dev_queue_xmit ? 4 : 2)),
				       neigh->dev->name);

			if ((neigh->nud_state&NUD_VALID) && neigh->dev->addr_len) {
				for (j=0; j < neigh->dev->addr_len; j++) {
					sprintf(buffer+len+size, "%02x", neigh->ha[j]);
					size += 2;
				}
			} else {
                                size += sprintf(buffer+len+size, "000000000000");
			}
			read_unlock(&neigh->lock);
			size += sprintf(buffer+len+size, "\n");
			len += size;
			pos += size;
		  
			if (pos <= offset)
				len=0;
			if (pos >= offset+length) {
				read_unlock_bh(&nd_tbl.lock);
				goto done;
			}
		}
		read_unlock_bh(&nd_tbl.lock);
	}

done:

	*start = buffer+len-(pos-offset);	/* Start of wanted data */
	len = pos-offset;			/* Start slop */
	if (len>length)
		len = length;			/* Ending slop */
	if (len<0)
		len = 0;
	return len;
}

#endif
#endif	/* CONFIG_PROC_FS */


static int ndisc_netdev_event(struct notifier_block *this, unsigned long event, void *ptr)
{
	struct net_device *dev = ptr;

	switch (event) {
	case NETDEV_CHANGEADDR:
		neigh_changeaddr(&nd_tbl, dev);
		fib6_run_gc(0);
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}

struct notifier_block ndisc_netdev_notifier = {
	.notifier_call = ndisc_netdev_event,
};

int __init ndisc_init(struct net_proto_family *ops)
{
	struct sock *sk;
        int err;

	ndisc_socket = sock_alloc();
	if (ndisc_socket == NULL) {
		ND_PRINTK1(KERN_ERR
			   "ndisc_init(): Failed to create the NDISC control socket.\n");
		return -1;
	}
	ndisc_socket->inode->i_uid = 0;
	ndisc_socket->inode->i_gid = 0;
	ndisc_socket->type = SOCK_RAW;

	if((err = ops->create(ndisc_socket, IPPROTO_ICMPV6)) < 0) {
		ND_PRINTK1(KERN_ERR
			   "ndisc_init(): Failed to initialize the NDISC control socket (err %d).\n",
			   err);
		sock_release(ndisc_socket);
		ndisc_socket = NULL; /* For safety. */
		return err;
	}

	sk = ndisc_socket->sk;
	sk->allocation = GFP_ATOMIC;
	sk->net_pinfo.af_inet6.hop_limit = 255;
	/* Do not loopback ndisc messages */
	sk->net_pinfo.af_inet6.mc_loop = 0;
	sk->prot->unhash(sk);

        /*
         * Initialize the neighbour table
         */
	
	neigh_table_init(&nd_tbl);

#ifdef CONFIG_PROC_FS
#ifdef CONFIG_IPV6_NDISC_DEBUG
	proc_net_create("ndisc", 0, ndisc_get_info);
#endif
#endif
#ifdef CONFIG_SYSCTL
	neigh_sysctl_register(NULL, &nd_tbl.parms, NET_IPV6, NET_IPV6_NEIGH, "ipv6");
#endif

	register_netdevice_notifier(&ndisc_netdev_notifier);
	return 0;
}

void ndisc_cleanup(void)
{
#ifdef CONFIG_PROC_FS
#ifdef CONFIG_IPV6_NDISC_DEBUG
        proc_net_remove("ndisc");
#endif
#endif
	neigh_table_clear(&nd_tbl);
	sock_release(ndisc_socket);
	ndisc_socket = NULL; /* For safety. */
}
