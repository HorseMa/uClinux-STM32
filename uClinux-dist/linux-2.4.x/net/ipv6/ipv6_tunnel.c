/*
 *	IPv6 over IPv6 tunnel device
 *	Linux INET6 implementation
 *
 *	Authors:
 *	Ville Nuorvala		<vnuorval@tml.hut.fi>	
 *
 *	$Id: s.ipv6_tunnel.c 1.23 02/11/29 15:33:51+02:00 vnuorval@amber.hut.mediapoli.com $
 *
 *      Based on:
 *      linux/net/ipv6/sit.c
 *
 *      RFC 2473
 *
 *	This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 */

#define __NO_VERSION__
#include <linux/config.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/if.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/if_tunnel.h>
#include <linux/net.h>
#include <linux/in6.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/icmpv6.h>
#include <linux/init.h>
#include <linux/route.h>
#include <linux/rtnetlink.h>
#include <linux/tqueue.h>

#include <asm/uaccess.h>
#include <asm/atomic.h>

#include <net/sock.h>
#include <net/ipv6.h>
#include <net/protocol.h>
#include <net/ip6_route.h>
#include <net/addrconf.h>
#include <net/ipv6_tunnel.h>

#ifdef MODULE
MODULE_AUTHOR("Ville Nuorvala");
MODULE_DESCRIPTION("IPv6 over IPv6 tunnel");
MODULE_LICENSE("GPL");
#endif

#ifdef IPV6_TNL_DEBUG
#define IPV6_TNL_TRACE(x...) printk(KERN_DEBUG __FUNCTION__ x "\n")
#else
#define IPV6_TNL_TRACE(x...) do {;} while(0)
#endif

/* socket used by ipv6_ipv6_tnl_xmit() for resending packets */
static struct socket *ipv6_socket;

/* current holder of socket */
static int ipv6_xmit_holder = -1;

/* socket locking functions copied from net/ipv6/icmp.c */

static int
ipv6_xmit_lock_bh(void)
{
	if (!spin_trylock(&ipv6_socket->sk->lock.slock)) {
		if (ipv6_xmit_holder == smp_processor_id())
			return -EAGAIN;
		spin_lock(&ipv6_socket->sk->lock.slock);
	}
	ipv6_xmit_holder = smp_processor_id();
	return 0;
}

static inline int
ipv6_xmit_lock(void)
{
	int ret;
	local_bh_disable();
	ret = ipv6_xmit_lock_bh();
	if (ret)
		local_bh_enable();
	return ret;
}

static void
ipv6_xmit_unlock_bh(void)
{
	ipv6_xmit_holder = -1;
	spin_unlock(&ipv6_socket->sk->lock.slock);
}

static inline void
ipv6_xmit_unlock(void)
{
	ipv6_xmit_unlock_bh();
	local_bh_enable();
}

#define HASH_SIZE  32

#define HASH(addr) (((addr)->s6_addr16[0] ^ (addr)->s6_addr16[1] ^ \
	             (addr)->s6_addr16[2] ^ (addr)->s6_addr16[3] ^ \
                     (addr)->s6_addr16[4] ^ (addr)->s6_addr16[5] ^ \
	             (addr)->s6_addr16[6] ^ (addr)->s6_addr16[7]) & \
                    (HASH_SIZE - 1))

static int ipv6_ipv6_fb_tnl_dev_init(struct net_device *dev);
static int ipv6_ipv6_tnl_dev_init(struct net_device *dev);

/* the IPv6 IPv6 tunnel fallback device */
static struct net_device ipv6_ipv6_fb_tnl_dev = {
	name:"ip6tnl0",
	init:ipv6_ipv6_fb_tnl_dev_init
};

/* the IPv6 IPv6 fallback tunnel */
static struct ipv6_tnl ipv6_ipv6_fb_tnl = {
	dev:&ipv6_ipv6_fb_tnl_dev,
      parms:{name: "ip6tnl0", proto:IPPROTO_IPV6}
};

/* lists for storing tunnels in use */
static struct ipv6_tnl *tnls_r_l[HASH_SIZE];
static struct ipv6_tnl *tnls_wc[1];
static struct ipv6_tnl **tnls[2] = { tnls_wc, tnls_r_l };

/* list for unused cached kernel tunnels */
static struct ipv6_tnl *tnls_kernel[1];
/* maximum number of cached kernel tunnels */
static unsigned int max_kdev_count = 0;
/* minimum number of cached kernel tunnels */
static unsigned int min_kdev_count = 0;
/* current number of cached kernel tunnels */
static unsigned int kdev_count = 0;

/* lists for tunnel hook functions */
static struct list_head hooks[IPV6_TNL_MAXHOOKS];

/* locks for the different lists */
static rwlock_t ipv6_ipv6_lock = RW_LOCK_UNLOCKED;
static rwlock_t ipv6_ipv6_kernel_lock = RW_LOCK_UNLOCKED;
static rwlock_t ipv6_ipv6_hook_lock = RW_LOCK_UNLOCKED;

/* flag indicating if the module is being removed */
static int shutdown = 0;

/**
 * ipv6_ipv6_tnl_lookup - fetch tunnel matching the end-point addresses
 *   @remote: the address of the tunnel exit-point 
 *   @local: the address of the tunnel entry-point 
 *
 * Return:  
 *   tunnel matching given end-points if found,
 *   else fallback tunnel if its device is up, 
 *   else %NULL
 **/

struct ipv6_tnl *
ipv6_ipv6_tnl_lookup(struct in6_addr *remote, struct in6_addr *local)
{
	unsigned h0 = HASH(remote);
	unsigned h1 = HASH(local);
	struct ipv6_tnl *t;

	IPV6_TNL_TRACE();

	for (t = tnls_r_l[h0 ^ h1]; t; t = t->next) {
		if (!ipv6_addr_cmp(local, &t->parms.laddr) &&
		    !ipv6_addr_cmp(remote, &t->parms.raddr) &&
		    (t->dev->flags & IFF_UP))
			return t;
	}
	if ((t = tnls_wc[0]) != NULL && (t->dev->flags & IFF_UP))
		return t;

	return NULL;
}

/**
 * ipv6_ipv6_bucket - get head of list matching given tunnel parameters
 *   @p: parameters containing tunnel end-points 
 *
 * Description:
 *   ipv6_ipv6_bucket() returns the head of the list matching the 
 *   &struct in6_addr entries laddr and raddr in @p.
 *
 * Return: head of IPv6 tunnel list 
 **/

static struct ipv6_tnl **
ipv6_ipv6_bucket(struct ipv6_tnl_parm *p)
{
	struct in6_addr *remote = &p->raddr;
	struct in6_addr *local = &p->laddr;
	unsigned h = 0;
	int prio = 0;

	IPV6_TNL_TRACE();

	if (!ipv6_addr_any(remote) || !ipv6_addr_any(local)) {
		prio = 1;
		h = HASH(remote) ^ HASH(local);
	}
	return &tnls[prio][h];
}

/**
 * ipv6_ipv6_kernel_tnl_link - add new kernel tunnel to cache
 *   @t: kernel tunnel
 *
 * Note:
 *   %IPV6_TNL_F_KERNEL_DEV is assumed to be raised in t->parms.flags. 
 *   See the comments on ipv6_ipv6_kernel_tnl_add() for more information.
 **/

static inline void
ipv6_ipv6_kernel_tnl_link(struct ipv6_tnl *t)
{
	IPV6_TNL_TRACE();

	write_lock_bh(&ipv6_ipv6_kernel_lock);
	t->next = tnls_kernel[0];
	tnls_kernel[0] = t;
	kdev_count++;
	write_unlock_bh(&ipv6_ipv6_kernel_lock);
}

/**
 * ipv6_ipv6_kernel_tnl_unlink - remove first kernel tunnel from cache
 *
 * Return: first free kernel tunnel
 *
 * Note:
 *   See the comments on ipv6_ipv6_kernel_tnl_add() for more information.
 **/

static inline struct ipv6_tnl *
ipv6_ipv6_kernel_tnl_unlink(void)
{
	struct ipv6_tnl *t;

	IPV6_TNL_TRACE();

	write_lock_bh(&ipv6_ipv6_kernel_lock);
	if ((t = tnls_kernel[0]) != NULL) {
		tnls_kernel[0] = t->next;
		kdev_count--;
	}
	write_unlock_bh(&ipv6_ipv6_kernel_lock);
	return t;
}

/**
 * ipv6_ipv6_tnl_link - add tunnel to hash table
 *   @t: tunnel to be added
 **/

static void
ipv6_ipv6_tnl_link(struct ipv6_tnl *t)
{
	struct ipv6_tnl **tp = ipv6_ipv6_bucket(&t->parms);

	IPV6_TNL_TRACE();

	write_lock_bh(&ipv6_ipv6_lock);
	t->next = *tp;
	write_unlock_bh(&ipv6_ipv6_lock);
	*tp = t;
}

/**
 * ipv6_ipv6_tnl_unlink - remove tunnel from hash table
 *   @t: tunnel to be removed
 **/

static void
ipv6_ipv6_tnl_unlink(struct ipv6_tnl *t)
{
	struct ipv6_tnl **tp;

	IPV6_TNL_TRACE();

	for (tp = ipv6_ipv6_bucket(&t->parms); *tp; tp = &(*tp)->next) {
		if (t == *tp) {
			write_lock_bh(&ipv6_ipv6_lock);
			*tp = t->next;
			write_unlock_bh(&ipv6_ipv6_lock);
			break;
		}
	}
}

/**
 * ipv6_addr_local() - check if address is local
 *   @addr: address to be checked
 * 
 * Return: 
 *   1 if @addr assigned to any local network device,
 *   0 otherwise
 **/

static inline int
ipv6_addr_local(struct in6_addr *addr)
{
	struct inet6_ifaddr *ifr;
	int local = 0;

	IPV6_TNL_TRACE();

	if ((ifr = ipv6_get_ifaddr(addr, NULL)) != NULL) {
		local = 1;
		in6_ifa_put(ifr);
	}
	return local;
}

/**
 * ipv6_tnl_addrs_sane() - check that the tunnel end points are sane
 *   @laddr: tunnel entry-point
 *   @raddr: tunnel exit-point
 *
 * Description:
 *   Sanity checks performed on tunnel end-points as suggested by
 *   RFCs 1853, 2003 and 2473.
 * 
 * Return: 
 *   1 if sane,
 *   0 otherwise 
 **/

static inline int
ipv6_tnl_addrs_sane(struct in6_addr *laddr, struct in6_addr *raddr)
{
	int laddr_type = ipv6_addr_type(laddr);
	int raddr_type = ipv6_addr_type(raddr);

	IPV6_TNL_TRACE();

	if ((laddr_type &
	     (IPV6_ADDR_ANY | IPV6_ADDR_LOOPBACK | IPV6_ADDR_RESERVED)) ||
	    (raddr_type &
	     (IPV6_ADDR_ANY | IPV6_ADDR_LOOPBACK | IPV6_ADDR_RESERVED)) ||
	    ((laddr_type & IPV6_ADDR_UNICAST) && !ipv6_addr_local(laddr)) ||
	    ((raddr_type & IPV6_ADDR_UNICAST) && ipv6_addr_local(raddr))) {
		return 0;
	}
	return 1;
}

/**
 * ipv6_tnl_create() - create a new tunnel
 *   @p: tunnel parameters
 *   @pt: pointer to new tunnel
 *
 * Description:
 *   Create tunnel matching given parameters. New kernel managed devices are 
 *   not put in the normal hash structure, but are instead cached for later
 *   use.
 * 
 * Return: 
 *   0 on success
 **/

static int
ipv6_tnl_create(struct ipv6_tnl_parm *p, struct ipv6_tnl **pt)
{
	struct net_device *dev;
	int kernel_dev;
	int err = -ENOBUFS;
	struct ipv6_tnl *t;

	IPV6_TNL_TRACE();

	kernel_dev = (p->flags & IPV6_TNL_F_KERNEL_DEV);

	MOD_INC_USE_COUNT;
	dev = kmalloc(sizeof (*dev) + sizeof (*t), GFP_KERNEL);
	if (!dev) {
		MOD_DEC_USE_COUNT;
		return err;
	}
	memset(dev, 0, sizeof (*dev) + sizeof (*t));
	dev->priv = (void *) (dev + 1);
	t = (struct ipv6_tnl *) dev->priv;
	t->dev = dev;
	dev->init = ipv6_ipv6_tnl_dev_init;
	dev->features |= NETIF_F_DYNALLOC;
	if (kernel_dev) {
		memcpy(t->parms.name, p->name, IFNAMSIZ - 1);
		t->parms.proto = IPPROTO_IPV6;
		t->parms.flags = IPV6_TNL_F_KERNEL_DEV;
	} else {
		memcpy(&t->parms, p, sizeof (*p));
	}
	t->parms.name[IFNAMSIZ - 1] = '\0';
	if (t->parms.hop_limit > 255)
		t->parms.hop_limit = -1;
	strcpy(dev->name, t->parms.name);
	if (!dev->name[0]) {
		int i = 0;
		int exists = 0;

		do {
			sprintf(dev->name, "ip6tnl%d", ++i);
			exists = (__dev_get_by_name(dev->name) != NULL);
		} while (i < IPV6_TNL_MAX && exists);

		if (i == IPV6_TNL_MAX) {
			goto failed;
		}
		memcpy(t->parms.name, dev->name, IFNAMSIZ);
	}
	if ((err = register_netdevice(dev)) < 0) {
		goto failed;
	}
	if (kernel_dev) {
		ipv6_ipv6_kernel_tnl_link(t);
	} else {
		ipv6_ipv6_tnl_link(t);
	}
	*pt = t;
	return 0;
      failed:
	kfree(dev);
	MOD_DEC_USE_COUNT;
	return err;
}

/**
 * ipv6_tnl_destroy() - destroy old tunnel
 *   @t: tunnel to be destroyed
 *
 * Return:
 *   whatever unregister_netdevice() returns
 **/

static inline int
ipv6_tnl_destroy(struct ipv6_tnl *t)
{
	IPV6_TNL_TRACE();
	return unregister_netdevice(t->dev);
}

static void manage_kernel_tnls(void *foo);

static struct tq_struct manager_task = {
	routine:manage_kernel_tnls,
	data:NULL
};

/**
 * manage_kernel_tnls() - create and destroy kernel tunnels
 *
 * Description:
 *   manage_kernel_tnls() creates new kernel devices if there
 *   are less than $min_kdev_count of them and deletes old ones if
 *   there are less than $max_kdev_count of them in the cache
 *
 * Note:
 *   Schedules itself to be run later in process context if called from 
 *   interrupt. Therefore only works synchronously when called from process 
 *   context.
 **/

static void
manage_kernel_tnls(void *foo)
{
	struct ipv6_tnl *t = NULL;
	struct ipv6_tnl_parm parm;

	IPV6_TNL_TRACE();

	/* We can't do this processing in interrupt 
	   context so schedule it for later */
	if (in_interrupt()) {
		read_lock(&ipv6_ipv6_kernel_lock);
		if (!shutdown &&
		    (kdev_count < min_kdev_count ||
		     kdev_count > max_kdev_count)) {
			schedule_task(&manager_task);
		}
		read_unlock(&ipv6_ipv6_kernel_lock);
		return;
	}

	rtnl_lock();
	read_lock_bh(&ipv6_ipv6_kernel_lock);
	memset(&parm, 0, sizeof (parm));
	parm.flags = IPV6_TNL_F_KERNEL_DEV;
	/* Create tunnels until there are at least min_kdev_count */
	while (kdev_count < min_kdev_count) {
		read_unlock_bh(&ipv6_ipv6_kernel_lock);
		if (!ipv6_tnl_create(&parm, &t)) {
			dev_open(t->dev);
		} else {
			goto err;
		}
		read_lock_bh(&ipv6_ipv6_kernel_lock);
	}

	/* Destroy tunnels until there are at most max_kdev_count */
	while (kdev_count > max_kdev_count) {
		read_unlock_bh(&ipv6_ipv6_kernel_lock);
		if ((t = ipv6_ipv6_kernel_tnl_unlink()) != NULL) {
			ipv6_tnl_destroy(t);
		} else {
			goto err;
		}
		read_lock_bh(&ipv6_ipv6_kernel_lock);
	}
	read_unlock_bh(&ipv6_ipv6_kernel_lock);
      err:
	rtnl_unlock();
}

/**
 * ipv6_ipv6_tnl_inc_max_kdev_count() - increase max kernel dev cache size
 *   @n: size increase
 * Description:
 *   Increase the upper limit for the number of kernel devices allowed in the 
 *   cache at any on time.
 **/

unsigned int
ipv6_ipv6_tnl_inc_max_kdev_count(unsigned int n)
{
	IPV6_TNL_TRACE();

	write_lock_bh(&ipv6_ipv6_kernel_lock);
	max_kdev_count += n;
	write_unlock_bh(&ipv6_ipv6_kernel_lock);
	manage_kernel_tnls(NULL);
	return max_kdev_count;
}

/**
 * ipv6_ipv6_tnl_dec_max_kdev_count() -  decrease max kernel dev cache size
 *   @n: size decrement
 * Description:
 *   Decrease the upper limit for the number of kernel devices allowed in the 
 *   cache at any on time.
 **/

unsigned int
ipv6_ipv6_tnl_dec_max_kdev_count(unsigned int n)
{
	IPV6_TNL_TRACE();

	write_lock_bh(&ipv6_ipv6_kernel_lock);
	max_kdev_count -= min(max_kdev_count, n);
	if (max_kdev_count < min_kdev_count)
		min_kdev_count = max_kdev_count;
	write_unlock_bh(&ipv6_ipv6_kernel_lock);
	manage_kernel_tnls(NULL);
	return max_kdev_count;
}

/**
 * ipv6_ipv6_tnl_inc_min_kdev_count() - increase min kernel dev cache size
 *   @n: size increase
 * Description:
 *   Increase the lower limit for the number of kernel devices allowed in the 
 *   cache at any on time.
 **/

unsigned int
ipv6_ipv6_tnl_inc_min_kdev_count(unsigned int n)
{
	IPV6_TNL_TRACE();

	write_lock_bh(&ipv6_ipv6_kernel_lock);
	min_kdev_count += n;
	if (min_kdev_count > max_kdev_count)
		max_kdev_count = min_kdev_count;
	write_unlock_bh(&ipv6_ipv6_kernel_lock);
	manage_kernel_tnls(NULL);
	return min_kdev_count;
}

/**
 * ipv6_ipv6_tnl_dec_min_kdev_count() -  decrease min kernel dev cache size
 *   @n: size decrement
 * Description:
 *   Decrease the lower limit for the number of kernel devices allowed in the 
 *   cache at any on time.
 **/

unsigned int
ipv6_ipv6_tnl_dec_min_kdev_count(unsigned int n)
{
	IPV6_TNL_TRACE();

	write_lock_bh(&ipv6_ipv6_kernel_lock);
	min_kdev_count -= min(min_kdev_count, n);
	write_unlock_bh(&ipv6_ipv6_kernel_lock);
	manage_kernel_tnls(NULL);
	return min_kdev_count;
}

/**
 * ipv6_ipv6_tnl_locate - find or create tunnel matching given parameters
 *   @p: tunnel parameters 
 *   @create: != 0 if allowed to create new tunnel if no match found
 *
 * Description:
 *   ipv6_ipv6_tnl_locate() first tries to locate an existing tunnel
 *   based on @parms. If this is unsuccessful, but @create is set a new
 *   tunnel device is created and registered for use.
 *
 * Return:
 *   0 if tunnel located or created,
 *   -EINVAL if parameters incorrect,
 *   -ENODEV if no matching tunnel available
 **/

static int
ipv6_ipv6_tnl_locate(struct ipv6_tnl_parm *p, struct ipv6_tnl **pt, int create)
{
	struct in6_addr *remote = &p->raddr;
	struct in6_addr *local = &p->laddr;
	struct ipv6_tnl *t;

	IPV6_TNL_TRACE();

	if (p->proto != IPPROTO_IPV6 ||
	    (!(p->flags & IPV6_TNL_F_KERNEL_DEV) &&
	     !ipv6_tnl_addrs_sane(local, remote)))
		return -EINVAL;

	for (t = *ipv6_ipv6_bucket(p); t; t = t->next) {
		if (!ipv6_addr_cmp(local, &t->parms.laddr) &&
		    !ipv6_addr_cmp(remote, &t->parms.raddr)) {
			*pt = t;
			return (create ? -EEXIST : 0);
		}
	}
	/* Kernel devices are created on demand in 
	   manage_kernel_tnls() */
	if (!create || (p->flags & IPV6_TNL_F_KERNEL_DEV)) {
		return -ENODEV;
	}
	return ipv6_tnl_create(p, pt);
}

/**
 * ipv6_ipv6_tnl_dev_destructor - tunnel device destructor
 *   @dev: the device to be destroyed
 **/

static void
ipv6_ipv6_tnl_dev_destructor(struct net_device *dev)
{
	IPV6_TNL_TRACE();

	if (dev != &ipv6_ipv6_fb_tnl_dev) {
		MOD_DEC_USE_COUNT;
	}
}

/**
 * ipv6_ipv6_tnl_dev_uninit - tunnel device uninitializer
 *   @dev: the device to be destroyed
 *   
 * Description:
 *   ipv6_ipv6_tnl_dev_uninit() removes tunnel from its list
 **/

static void
ipv6_ipv6_tnl_dev_uninit(struct net_device *dev)
{
	IPV6_TNL_TRACE();

	if (dev == &ipv6_ipv6_fb_tnl_dev) {
		write_lock_bh(&ipv6_ipv6_lock);
		tnls_wc[0] = NULL;
		write_unlock_bh(&ipv6_ipv6_lock);
	} else {
		struct ipv6_tnl *t = (struct ipv6_tnl *) dev->priv;
		ipv6_ipv6_tnl_unlink(t);
	}
}

/**
 * parse_tvl_tnl_enc_lim - handle encapsulation limit option
 *   @skb: received socket buffer
 *
 * Return: 
 *   0 if none was found, 
 *   else index to encapsulation limit
 **/

static __u16
parse_tlv_tnl_enc_lim(struct sk_buff *skb, __u8 * raw)
{
	struct ipv6hdr *ipv6h = (struct ipv6hdr *) raw;
	__u8 nexthdr = ipv6h->nexthdr;
	__u16 off = sizeof (*ipv6h);

	IPV6_TNL_TRACE();

	while (ipv6_ext_hdr(nexthdr) && nexthdr != NEXTHDR_NONE) {
		__u16 optlen = 0;
		struct ipv6_opt_hdr *hdr;
		if (raw + off + sizeof (*hdr) > skb->data &&
		    !pskb_may_pull(skb, raw - skb->data + off + sizeof (*hdr)))
			break;

		hdr = (struct ipv6_opt_hdr *) (raw + off);
		if (nexthdr == NEXTHDR_FRAGMENT) {
			struct frag_hdr *frag_hdr = (struct frag_hdr *) hdr;
			if (frag_hdr->frag_off)
				break;
			optlen = 8;
		} else if (nexthdr == NEXTHDR_AUTH) {
			optlen = (hdr->hdrlen + 2) << 2;
		} else {
			optlen = ipv6_optlen(hdr);
		}
		if (nexthdr == NEXTHDR_DEST) {
			__u16 i = off + 2;
			while (1) {
				struct ipv6_tlv_tnl_enc_lim *tel;

				/* No more room for encapsulation limit */
				if (i + sizeof (*tel) > off + optlen)
					break;

				tel = (struct ipv6_tlv_tnl_enc_lim *) &raw[i];
				/* return index of option if found and valid */
				if (tel->type == IPV6_TLV_TNL_ENCAP_LIMIT &&
				    tel->length == 1)
					return i;
				/* else jump to next option */
				if (tel->type)
					i += tel->length + 2;
				else
					i++;
			}
		}
		nexthdr = hdr->nexthdr;
		off += optlen;
	}
	return 0;
}

/**
 * ipv6_ipv6_err - tunnel error handler
 *
 * Description:
 *   ipv6_ipv6_err() should handle errors in the tunnel according
 *   to the specifications in RFC 2473.
 **/

void
ipv6_ipv6_err(struct sk_buff *skb, struct inet6_skb_parm *opt,
	      int type, int code, int offset, __u32 info)
{
	struct ipv6hdr *tipv6h = (struct ipv6hdr *) skb->data;
	struct ipv6hdr *ipv6h = NULL;
	struct ipv6_tnl *t;
	int rel_msg = 0;
	int rel_type = ICMPV6_DEST_UNREACH;
	int rel_code = ICMPV6_ADDR_UNREACH;
	__u32 rel_info = 0;
	__u16 len;

	IPV6_TNL_TRACE();

	/* If the packet doesn't contain the original IPv6 header we are 
	   in trouble since we might need the source address for furter 
	   processing of the error. */

	if (pskb_may_pull(skb, offset + sizeof (*ipv6h)))
		ipv6h = (struct ipv6hdr *) &skb->data[offset];

	read_lock(&ipv6_ipv6_lock);
	if ((t = ipv6_ipv6_tnl_lookup(&tipv6h->daddr, &tipv6h->saddr)) == NULL)
		goto out;

	switch (type) {
		__u32 teli;
		struct ipv6_tlv_tnl_enc_lim *tel;
		__u32 mtu;
	case ICMPV6_DEST_UNREACH:
		printk(KERN_ERR
		       "%s: Path to destination invalid or inactive!\n",
		       t->parms.name);
		rel_msg = 1;
		break;
	case ICMPV6_TIME_EXCEED:
		if (code == ICMPV6_EXC_HOPLIMIT) {
			printk(KERN_ERR
			       "%s: Too small hop limit or "
			       "routing loop in tunnel!\n", t->parms.name);
			rel_msg = 1;
		}
		break;
	case ICMPV6_PARAMPROB:
		/* ignore if parameter problem not caused by a tunnel
		   encapsulation limit sub-option */
		if (code != ICMPV6_HDR_FIELD) {
			break;
		}
		teli = parse_tlv_tnl_enc_lim(skb, skb->data);

		if (teli && teli == info - 2) {
			tel = (struct ipv6_tlv_tnl_enc_lim *) &skb->data[teli];
			if (tel->encap_limit <= 1) {
				printk(KERN_ERR
				       "%s: Too small encapsulation limit or "
				       "routing loop in tunnel!\n",
				       t->parms.name);
				rel_msg = 1;
			}
		}
		break;
	case ICMPV6_PKT_TOOBIG:
		mtu = info - offset;
		if (mtu <= IPV6_MIN_MTU) {
			mtu = IPV6_MIN_MTU;
		}
		t->dev->mtu = mtu;

		if (ipv6h && (len = sizeof (*ipv6h) + ipv6h->payload_len) > mtu) {
			rel_type = ICMPV6_PKT_TOOBIG;
			rel_code = 0;
			rel_info = mtu;
			rel_msg = 1;
		}
		break;
	}
	if (rel_msg && ipv6h) {
		struct rt6_info *rt6i;
		struct sk_buff *skb2 = skb_clone(skb, GFP_ATOMIC);

		if (!skb2)
			goto out;

		dst_release(skb2->dst);
		skb2->dst = NULL;
		skb_pull(skb2, offset);
		skb2->nh.raw = skb2->data;

		/* Try to guess incoming interface */
		rt6i = rt6_lookup(&ipv6h->saddr, NULL, 0, 0);
		if (rt6i && rt6i->rt6i_dev)
			skb2->dev = rt6i->rt6i_dev;

		icmpv6_send(skb2, rel_type, rel_code, rel_info, skb2->dev);
		kfree_skb(skb2);
	}
      out:
	read_unlock(&ipv6_ipv6_lock);
}

/**
 * call_hooks - call ipv6 tunnel hooks
 *   @hooknum: hook number, either %IPV6_TNL_PRE_ENCAP, or 
 *   %IPV6_TNL_PRE_DECAP
 *   @t: the current tunnel
 *   @skb: the tunneled packet
 *
 * Description:
 *   Pass packet to all the hook functions until %IPV6_TNL_DROP or
 *   %IPV6_TNL_STOLEN returned by one of them.
 *
 * Return:
 *   %IPV6_TNL_ACCEPT, %IPV6_TNL_DROP or %IPV6_TNL_STOLEN
 **/

static inline int
call_hooks(unsigned int hooknum, struct ipv6_tnl *t, struct sk_buff *skb)
{
	struct ipv6_tnl_hook_ops *h;
	int accept = IPV6_TNL_ACCEPT;

	IPV6_TNL_TRACE();

	if (hooknum < IPV6_TNL_MAXHOOKS) {
		struct list_head *i;
		read_lock(&ipv6_ipv6_hook_lock);
		for (i = hooks[hooknum].next; i != &hooks[hooknum]; i = i->next) {
			h = (struct ipv6_tnl_hook_ops *) i;

			if (h->hook) {
				accept = h->hook(t, skb);

				if (accept != IPV6_TNL_ACCEPT)
					break;
			}
		}
		read_unlock(&ipv6_ipv6_hook_lock);
	}
	return accept;
}

/**
 * ipv6_ipv6_rcv - decapsulate IPv6 packet and retransmit it locally
 *   @skb: received socket buffer
 *
 * Return: 0
 **/

int
ipv6_ipv6_rcv(struct sk_buff *skb)
{
	struct ipv6hdr *ipv6h;
	struct ipv6_tnl *t;

	IPV6_TNL_TRACE();

	if (!pskb_may_pull(skb, sizeof (*ipv6h)))
		goto out;

	ipv6h = skb->nh.ipv6h;

	read_lock(&ipv6_ipv6_lock);

	if ((t = ipv6_ipv6_tnl_lookup(&ipv6h->saddr, &ipv6h->daddr)) != NULL) {
		int hookval = call_hooks(IPV6_TNL_PRE_DECAP, t, skb);
		switch (hookval) {
		case IPV6_TNL_ACCEPT:
			break;
		case IPV6_TNL_STOLEN:
			goto ignore_packet;
		default:
			if (hookval != IPV6_TNL_DROP) {
				printk(KERN_ERR
				       "%s: Unknown return value for tunnel hook!\n",
				       t->parms.name);
			}
			t->stat.rx_dropped++;
			goto drop_packet;
		}
		skb->mac.raw = skb->nh.raw;
		skb->nh.raw = skb->data;
		skb->protocol = htons(ETH_P_IPV6);
		skb->pkt_type = PACKET_HOST;
		skb->dev = t->dev;
		dst_release(skb->dst);
		skb->dst = NULL;
		t->stat.rx_packets++;
		t->stat.rx_bytes += skb->len;
		netif_rx(skb);
		read_unlock(&ipv6_ipv6_lock);
		return 0;
	}
	icmpv6_send(skb, ICMPV6_DEST_UNREACH, ICMPV6_ADDR_UNREACH, 0, skb->dev);
      drop_packet:
	kfree_skb(skb);
      ignore_packet:
	read_unlock(&ipv6_ipv6_lock);
      out:
	return 0;
}

/**
 * txopt_len - get necessary size for new &struct ipv6_txoptions
 *   @orig_opt: old options
 *
 * Return:
 *   Size of old one plus size of tunnel encapsulation limit option
 **/

static inline int
txopt_len(struct ipv6_txoptions *orig_opt)
{
	int len = sizeof (*orig_opt) + 8;

	IPV6_TNL_TRACE();

	if (orig_opt && orig_opt->dst0opt)
		len += ipv6_optlen(orig_opt->dst0opt);
	return len;
}

/**
 * merge_options - add encapsulation limit to original options
 *   @encap_limit: number of allowed encapsulation limits
 *   @orig_opt: original options
 * 
 * Return:
 *   Pointer to new &struct ipv6_txoptions containing the tunnel
 *   encapsulation limit
 **/

static inline struct ipv6_txoptions *
merge_options(struct sock *sk, __u8 encap_limit,
	      struct ipv6_txoptions *orig_opt)
{
	struct ipv6_tlv_tnl_enc_lim *tel;
	struct ipv6_txoptions *opt;
	__u8 *raw;
	__u8 pad_to = 8;
	int opt_len = txopt_len(orig_opt);

	IPV6_TNL_TRACE();

	if (!(opt = sock_kmalloc(sk, opt_len, GFP_ATOMIC))) {
		return NULL;
	}

	memset(opt, 0, opt_len);
	opt->tot_len = opt_len;
	opt->dst0opt = (struct ipv6_opt_hdr *) (opt + 1);
	opt->opt_nflen = 8;

	raw = (__u8 *) opt->dst0opt;

	tel = (struct ipv6_tlv_tnl_enc_lim *) (opt->dst0opt + 1);
	tel->type = IPV6_TLV_TNL_ENCAP_LIMIT;
	tel->length = 1;
	tel->encap_limit = encap_limit;

	if (orig_opt) {
		__u8 *orig_raw;

		opt->hopopt = orig_opt->hopopt;

		/* Keep the original destination options properly
		   aligned and merge possible old paddings to the
		   new padding option */
		if ((orig_raw = (__u8 *) orig_opt->dst0opt) != NULL) {
			__u8 type;
			int i = sizeof (struct ipv6_opt_hdr);
			pad_to += sizeof (struct ipv6_opt_hdr);
			while (i < ipv6_optlen(orig_opt->dst0opt)) {
				type = orig_raw[i++];
				if (type == IPV6_TLV_PAD0)
					pad_to++;
				else if (type == IPV6_TLV_PADN) {
					int len = orig_raw[i++];
					i += len;
					pad_to += len + 2;
				} else {
					break;
				}
			}
			opt->dst0opt->hdrlen = orig_opt->dst0opt->hdrlen + 1;
			memcpy(raw + pad_to, orig_raw + pad_to - 8,
			       opt_len - sizeof (*opt) - pad_to);
		}
		opt->srcrt = orig_opt->srcrt;
		opt->opt_nflen += orig_opt->opt_nflen;

		opt->dst1opt = orig_opt->dst1opt;
		opt->auth = orig_opt->auth;
		opt->opt_flen = orig_opt->opt_flen;
	}
	raw[5] = IPV6_TLV_PADN;

	/* subtract lengths of destination suboption header,
	   tunnel encapsulation limit and pad N header */
	raw[6] = pad_to - 7;

	return opt;
}

static int
ipv6_ipv6_getfrag(const void *data, struct in6_addr *addr,
		  char *buff, unsigned int offset, unsigned int len)
{
	memcpy(buff, data + offset, len);
	return 0;
}

/**
 * ipv6_ipv6_tnl_addr_conflict - compare packet addresses to tunnel's own
 *   @t: the outgoing tunnel device
 *   @hdr: IPv6 header from the incoming packet 
 *
 * Description:
 *   Avoid trivial tunneling loop by checking that tunnel exit-point 
 *   doesn't match source of incoming packet.
 *
 * Return: 
 *   1 if conflict,
 *   0 else
 **/

static inline int
ipv6_ipv6_tnl_addr_conflict(struct ipv6_tnl *t, struct ipv6hdr *hdr)
{
	return !ipv6_addr_cmp(&t->parms.raddr, &hdr->saddr);
}

/**
 * ipv6_ipv6_tnl_xmit - encapsulate packet and send 
 *   @skb: the outgoing socket buffer
 *   @dev: the outgoing tunnel device 
 *
 * Description:
 *   Build new header and do some sanity checks on the packet before sending
 *   it to ip6_build_xmit().
 *
 * Return: 
 *   0
 **/

static int
ipv6_ipv6_tnl_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct ipv6_tnl *t = (struct ipv6_tnl *) dev->priv;
	struct net_device_stats *stats = &t->stat;
	struct ipv6hdr *ipv6h = skb->nh.ipv6h;
	int hookval;
	struct ipv6_txoptions *orig_opt = NULL;
	struct ipv6_txoptions *opt = NULL;
	__u8 encap_limit = 0;
	__u16 offset;
	struct flowi fl;
	struct ip6_flowlabel *fl_lbl = NULL;
	int err = 0;
	struct dst_entry *dst;
	int link_failure = 0;
	struct sock *sk = ipv6_socket->sk;
	struct net_device *tdev;
	int mtu;

	IPV6_TNL_TRACE();

	if (t->recursion++) {
		stats->collisions++;
		goto tx_err;
	}
	if (skb->protocol != htons(ETH_P_IPV6) ||
	    (t->parms.flags & IPV6_TNL_F_RCV_ONLY) ||
	    ipv6_ipv6_tnl_addr_conflict(t, ipv6h) ||
	    (!(t->parms.flags & IPV6_TNL_F_ALLOW_LOCAL) &&
	     ipv6_addr_local(&ipv6h->saddr))) {
		goto tx_err;
	}
	if ((offset = parse_tlv_tnl_enc_lim(skb, skb->nh.raw)) > 0) {
		struct ipv6_tlv_tnl_enc_lim *tel;
		tel = (struct ipv6_tlv_tnl_enc_lim *) &skb->nh.raw[offset];
		if (tel->encap_limit <= 1) {
			icmpv6_send(skb, ICMPV6_PARAMPROB,
				    ICMPV6_HDR_FIELD, offset + 2, skb->dev);
			goto tx_err;
		}
		encap_limit = tel->encap_limit - 1;
	} else if (!(t->parms.flags & IPV6_TNL_F_IGN_ENCAP_LIMIT)) {
		encap_limit = t->parms.encap_limit;
	}
	hookval = call_hooks(IPV6_TNL_PRE_ENCAP, t, skb);
	switch (hookval) {
	case IPV6_TNL_ACCEPT:
		break;
	case IPV6_TNL_STOLEN:
		goto ignore_packet;
	default:
		if (hookval != IPV6_TNL_DROP) {
			printk(KERN_ERR
			       "%s: Unknown return value for tunnel hook!\n",
			       t->parms.name);
		}
		goto drop_packet;
	}
	if ((err = ipv6_xmit_lock()))
		goto tx_err;

	memcpy(&fl, &t->fl, sizeof (fl));

	if (fl.fl6_flowlabel) {
		fl_lbl = fl6_sock_lookup(sk, fl.fl6_flowlabel);
		if (fl_lbl)
			orig_opt = fl_lbl->opt;
	}
	if ((t->parms.flags & IPV6_TNL_F_USE_ORIG_TCLASS)) {
		fl.fl6_flowlabel |= (*(__u32 *) ipv6h &
				     IPV6_FLOWINFO_MASK & ~IPV6_FLOWLABEL_MASK);
	}
	if (encap_limit > 0) {
		if (!(opt = merge_options(sk, encap_limit, orig_opt))) {
			goto tx_err_free_fl_lbl;
		}
	} else {
		opt = orig_opt;
	}
	dst = ip6_route_output(sk, &fl);

	if (dst->error) {
		stats->tx_carrier_errors++;
		link_failure = 1;
		goto tx_err_dst_release;
	}
	tdev = dst->dev;

	/* local routing loop */
	if (tdev == dev) {
		stats->collisions++;
		printk(KERN_ERR "%s: Local routing loop detected!\n",
		       t->parms.name);
		goto tx_err_dst_release;
	}
	mtu = dst->pmtu - sizeof (*ipv6h);
	if (opt) {
		mtu -= (opt->opt_nflen + opt->opt_flen);
	}
	if (mtu < IPV6_MIN_MTU)
		mtu = IPV6_MIN_MTU;
	if (skb->dst && mtu < skb->dst->pmtu) {
		struct rt6_info *rt6 = (struct rt6_info *) skb->dst;
		rt6->rt6i_flags |= RTF_MODIFIED;
		rt6->u.dst.pmtu = mtu;
	}
	if (skb->len > mtu && skb->len > IPV6_MIN_MTU) {
		icmpv6_send(skb, ICMPV6_PKT_TOOBIG, 0, 
			    (mtu > IPV6_MIN_MTU ? mtu : IPV6_MIN_MTU), dev);
		goto tx_err_dst_release;
	}

	err = ip6_build_xmit(sk, ipv6_ipv6_getfrag, (void *)skb->nh.raw, 
			     &fl, skb->len, opt, t->parms.hop_limit, 0,
			     MSG_DONTWAIT);

	if (err == NET_XMIT_SUCCESS || err == NET_XMIT_CN) {
		stats->tx_bytes += skb->len;
		stats->tx_packets++;
	} else {
		stats->tx_errors++;
		stats->tx_aborted_errors++;
	}
	dst_release(dst);
	if (opt && opt != orig_opt)
		sock_kfree_s(sk, opt, opt->tot_len);
	fl6_sock_release(fl_lbl);
	ipv6_xmit_unlock();
	kfree_skb(skb);
	t->recursion--;
	return 0;
      tx_err_dst_release:
	dst_release(dst);
	if (opt && opt != orig_opt)
		sock_kfree_s(sk, opt, opt->tot_len);
      tx_err_free_fl_lbl:
	fl6_sock_release(fl_lbl);
	ipv6_xmit_unlock();
	if (link_failure)
		dst_link_failure(skb);
      tx_err:
	stats->tx_errors++;
      drop_packet:
	stats->tx_dropped++;
	kfree_skb(skb);
      ignore_packet:
	t->recursion--;
	return 0;
}

struct ipv6_tnl_link_parm {
	int iflink;
	unsigned short hard_header_len;
	unsigned mtu;
};

static int
ipv6_ipv6_tnl_link_parm_get(struct ipv6_tnl_parm *p,
			    int active, struct ipv6_tnl_link_parm *lp)
{
	int err = 0;

	if (active) {
		struct rt6_info *rt = rt6_lookup(&p->raddr, &p->laddr,
						 p->link, 0);
		struct net_device *rtdev;
		if (!rt) {
			err = -ENOENT;
		} else if ((rtdev = rt->rt6i_dev) == NULL) {
			err = -ENODEV;
		} else if (rtdev->type == ARPHRD_IPV6_IPV6_TUNNEL) {
			/* as long as all tunnels use the same socket 
			   (ipv6_socket) for transmission (locally) 
			   nested tunnels won't work */
			err = -ENOMEDIUM;
		} else {
			lp->iflink = rtdev->ifindex;
			lp->hard_header_len = rtdev->hard_header_len +
			    sizeof (struct ipv6hdr);
			lp->mtu = rtdev->mtu - sizeof (struct ipv6hdr);

			if (lp->mtu < IPV6_MIN_MTU)
				lp->mtu = IPV6_MIN_MTU;
		}
		if (rt) {
			dst_release(&rt->u.dst);
		}
	} else {
		lp->iflink = 0;
		lp->hard_header_len = LL_MAX_HEADER + sizeof (struct ipv6hdr);
		lp->mtu = ETH_DATA_LEN - sizeof (struct ipv6hdr);
	}
	return err;
}

static void
ipv6_ipv6_tnl_dev_config(struct net_device *dev, struct ipv6_tnl_link_parm *lp)
{
	struct ipv6_tnl *t = (struct ipv6_tnl *) dev->priv;
	struct flowi *fl;

	IPV6_TNL_TRACE();

	/* Set up flowi template */
	fl = &t->fl;
	fl->fl6_src = &t->parms.laddr;
	fl->fl6_dst = &t->parms.raddr;
	fl->oif = t->parms.link;
	fl->fl6_flowlabel = IPV6_FLOWLABEL_MASK & htonl(t->parms.flow_lbl);

	dev->iflink = lp->iflink;
	dev->hard_header_len = lp->hard_header_len;
	dev->mtu = lp->mtu;
}

/**
 * ipv6_ipv6_tnl_change - update the tunnel parameters
 *   @t: tunnel to be changed
 *   @p: tunnel configuration parameters
 *   @active: != 0 if tunnel is ready for use
 *
 * Description:
 *   ipv6_ipv6_tnl_change() updates the tunnel parameters
 **/

static int
ipv6_ipv6_tnl_change(struct ipv6_tnl *t, struct ipv6_tnl_parm *p, int active)
{
	struct net_device *dev = t->dev;
	int laddr_uc = (ipv6_addr_type(&p->laddr) & IPV6_ADDR_UNICAST);
	int raddr_uc = (ipv6_addr_type(&p->raddr) & IPV6_ADDR_UNICAST);
	int err;
	struct ipv6_tnl_link_parm lp;

	IPV6_TNL_TRACE();

	if ((err = ipv6_ipv6_tnl_link_parm_get(p, active, &lp)))
		return err;

	if (laddr_uc && raddr_uc) {
		dev->flags |= IFF_POINTOPOINT;
	} else {
		dev->flags &= ~IFF_POINTOPOINT;
	}
	ipv6_addr_copy(&t->parms.laddr, &p->laddr);
	ipv6_addr_copy(&t->parms.raddr, &p->raddr);
	t->parms.flags = p->flags;

	if (active) {
		/* Only allow xmit if the tunnel is configured 
		   with a valid unicast source address */
		if (laddr_uc) {
			t->parms.flags &= ~IPV6_TNL_F_RCV_ONLY;
		} else {
			t->parms.flags |= IPV6_TNL_F_RCV_ONLY;
		}
	}
	t->parms.hop_limit = (p->hop_limit <= 255 ? p->hop_limit : -1);
	t->parms.encap_limit = p->encap_limit;
	t->parms.flow_lbl = p->flow_lbl;

	ipv6_ipv6_tnl_dev_config(dev, &lp);
	return 0;
}

static inline int
ipv6_tnl_flag_cmp(__u32 f1, __u32 f2)
{
	return ((f1 & ~IPV6_TNL_F_RCV_ONLY) != (f2 & ~IPV6_TNL_F_RCV_ONLY));
}

/**
 * ipv6_ipv6_kernel_tnl_add - configure and add kernel tunnel to hash 
 *   @p: kernel tunnel configuration parameters
 *
 * Description:
 *   ipv6_ipv6_kernel_tnl_add() fetches an unused kernel tunnel configures
 *   it according to @p and places it among the active tunnels.
 * 
 * Return:
 *   number of references to tunnel on success,
 *   %-EEXIST if there is already a device matching description
 *   %-EINVAL if p->flags doesn't have %IPV6_TNL_F_KERNEL_DEV raised,
 *   %-ENODEV if there are no unused kernel tunnels available 
 * 
 * Note:
 *   The code for creating, opening, closing and destroying network devices
 *   must be called from process context, while the Mobile IP code, which 
 *   needs the tunnel devices, unfortunately runs in interrupt context. 
 *   
 *   The devices must be created and opened in advance, then placed in a 
 *   list where the kernel can fetch and ready them for use at a later time.
 *
 **/

int
ipv6_ipv6_kernel_tnl_add(struct ipv6_tnl_parm *p)
{
	struct ipv6_tnl *t;
	int err;

	IPV6_TNL_TRACE();

	if (!(p->flags & IPV6_TNL_F_KERNEL_DEV) ||
	    !ipv6_tnl_addrs_sane(&p->laddr, &p->raddr))
		return -EINVAL;
	if ((t = ipv6_ipv6_tnl_lookup(&p->raddr, &p->laddr)) != NULL &&
	    t != &ipv6_ipv6_fb_tnl) {
		if (ipv6_tnl_flag_cmp(p->flags, t->parms.flags)) {
			/* Incompatible tunnel already exists for endpoints */
			return -EEXIST;
		} else {
			/* Handle duplicate tunnels by incrementing 
			   reference count */
			atomic_inc(&t->refcnt);
			goto out;
		}
	}
	if ((t = ipv6_ipv6_kernel_tnl_unlink()) == NULL)
		return -ENODEV;
	if ((err = ipv6_ipv6_tnl_change(t, p, 1))) {
		ipv6_ipv6_kernel_tnl_link(t);
		return err;
	}
	atomic_inc(&t->refcnt);

	ipv6_ipv6_tnl_link(t);

	manage_kernel_tnls(NULL);
      out:
	return atomic_read(&t->refcnt);
}

/**
 * ipv6_ipv6_kernel_tnl_del - delete no longer needed kernel tunnel 
 *   @t: kernel tunnel to be removed from hash
 *
 * Description:
 *   ipv6_ipv6_kernel_tnl_del() removes and deconfigures the tunnel @t
 *   and places it among the unused kernel devices.
 * 
 * Return:
 *   number of references on success,
 *   %-EINVAL if p->flags doesn't have %IPV6_TNL_F_KERNEL_DEV raised,
 * 
 * Note:
 *   See the comments on ipv6_ipv6_kernel_tnl_add() for more information.
 **/

int
ipv6_ipv6_kernel_tnl_del(struct ipv6_tnl *t)
{
	IPV6_TNL_TRACE();

	if (!t)
		return -ENODEV;

	if (!(t->parms.flags & IPV6_TNL_F_KERNEL_DEV))
		return -EINVAL;

	if (atomic_dec_and_test(&t->refcnt)) {
		struct ipv6_tnl_parm p;
		ipv6_ipv6_tnl_unlink(t);
		memset(&p, 0, sizeof (p));
		p.flags = IPV6_TNL_F_KERNEL_DEV;

		ipv6_ipv6_tnl_change(t, &p, 0);

		ipv6_ipv6_kernel_tnl_link(t);

		manage_kernel_tnls(NULL);
	}
	return atomic_read(&t->refcnt);
}

/**
 * ipv6_ipv6_tnl_ioctl - configure ipv6 tunnels from userspace 
 *   @dev: virtual device associated with tunnel
 *   @ifr: parameters passed from userspace
 *   @cmd: command to be performed
 *
 * Description:
 *   ipv6_ipv6_tnl_ioctl() is used for managing IPv6 tunnels 
 *   from userspace. 
 *
 *   The possible commands are the following:
 *     %SIOCGETTUNNEL: get tunnel parameters for device
 *     %SIOCADDTUNNEL: add tunnel matching given tunnel parameters
 *     %SIOCCHGTUNNEL: change tunnel parameters to those given
 *     %SIOCDELTUNNEL: delete tunnel
 *
 *   The fallback device "ip6tnl0", created during module 
 *   initialization, can be used for creating other tunnel devices.
 *
 * Return:
 *   0 on success,
 *   %-EFAULT if unable to copy data to or from userspace,
 *   %-EPERM if current process hasn't %CAP_NET_ADMIN set or attempting
 *   to configure kernel devices from userspace, 
 *   %-EINVAL if passed tunnel parameters are invalid,
 *   %-EEXIST if changing a tunnel's parameters would cause a conflict
 *   %-ENODEV if attempting to change or delete a nonexisting device
 *
 * Note:
 *   See the comments on ipv6_ipv6_kernel_tnl_add() for more information 
 *   about kernel tunnels.
 * **/

static int
ipv6_ipv6_tnl_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	int err = 0;
	int create;
	struct ipv6_tnl_parm p;
	struct ipv6_tnl *t = NULL;

	IPV6_TNL_TRACE();

	MOD_INC_USE_COUNT;

	switch (cmd) {
	case SIOCGETTUNNEL:
		if (dev == &ipv6_ipv6_fb_tnl_dev) {
			if (copy_from_user(&p,
					   ifr->ifr_ifru.ifru_data,
					   sizeof (p))) {
				err = -EFAULT;
				break;
			}
			if ((err = ipv6_ipv6_tnl_locate(&p, &t, 0)) == -ENODEV)
				t = (struct ipv6_tnl *) dev->priv;
			else if (err)
				break;
		} else
			t = (struct ipv6_tnl *) dev->priv;

		memcpy(&p, &t->parms, sizeof (p));
		if (copy_to_user(ifr->ifr_ifru.ifru_data, &p, sizeof (p))) {
			err = -EFAULT;
		}
		break;
	case SIOCADDTUNNEL:
	case SIOCCHGTUNNEL:
		err = -EPERM;
		create = (cmd == SIOCADDTUNNEL);
		if (!capable(CAP_NET_ADMIN))
			break;
		if (copy_from_user(&p, ifr->ifr_ifru.ifru_data, sizeof (p))) {
			err = -EFAULT;
			break;
		}
		if (p.flags & IPV6_TNL_F_KERNEL_DEV) {
			break;
		}
		if (!create && dev != &ipv6_ipv6_fb_tnl_dev) {
			t = (struct ipv6_tnl *) dev->priv;
		}
		if (!t && (err = ipv6_ipv6_tnl_locate(&p, &t, create))) {
			break;
		}
		if (cmd == SIOCCHGTUNNEL) {
			if (t->dev != dev) {
				err = -EEXIST;
				break;
			}
			if (t->parms.flags & IPV6_TNL_F_KERNEL_DEV) {
				err = -EPERM;
				break;
			}
			ipv6_ipv6_tnl_unlink(t);
			err = ipv6_ipv6_tnl_change(t, &p, 1);
			ipv6_ipv6_tnl_link(t);
			netdev_state_change(dev);
		}
		if (copy_to_user(ifr->ifr_ifru.ifru_data,
				 &t->parms, sizeof (p))) {
			err = -EFAULT;
		} else {
			err = 0;
		}
		break;
	case SIOCDELTUNNEL:
		err = -EPERM;
		if (!capable(CAP_NET_ADMIN))
			break;

		if (dev == &ipv6_ipv6_fb_tnl_dev) {
			if (copy_from_user(&p, ifr->ifr_ifru.ifru_data,
					   sizeof (p))) {
				err = -EFAULT;
				break;
			}
			err = ipv6_ipv6_tnl_locate(&p, &t, 0);
			if (err)
				break;
			if (t == &ipv6_ipv6_fb_tnl) {
				err = -EPERM;
				break;
			}
		} else {
			t = (struct ipv6_tnl *) dev->priv;
		}
		if (t->parms.flags & IPV6_TNL_F_KERNEL_DEV)
			err = -EPERM;
		else
			err = ipv6_tnl_destroy(t);
		break;
	default:
		err = -EINVAL;
	}
	MOD_DEC_USE_COUNT;
	return err;
}

/**
 * ipv6_ipv6_tnl_get_stats - return the stats for tunnel device 
 *   @dev: virtual device associated with tunnel
 *
 * Return: stats for device
 **/

static struct net_device_stats *
ipv6_ipv6_tnl_get_stats(struct net_device *dev)
{
	IPV6_TNL_TRACE();

	return &(((struct ipv6_tnl *) dev->priv)->stat);
}

/**
 * ipv6_ipv6_tnl_change_mtu - change mtu manually for tunnel device
 *   @dev: virtual device associated with tunnel
 *   @new_mtu: the new mtu
 *
 * Return:
 *   0 on success,
 *   %-EINVAL if mtu too small
 **/

static int
ipv6_ipv6_tnl_change_mtu(struct net_device *dev, int new_mtu)
{
	IPV6_TNL_TRACE();

	if (new_mtu < IPV6_MIN_MTU) {
		return -EINVAL;
	}
	dev->mtu = new_mtu;
	return 0;
}

/**
 * ipv6_ipv6_tnl_dev_init_gen - general initializer for all tunnel devices
 *   @dev: virtual device associated with tunnel
 *
 * Description:
 *   Set function pointers and initialize the &struct flowi template used
 *   by the tunnel.
 **/

static void
ipv6_ipv6_tnl_dev_init_gen(struct net_device *dev)
{
	struct ipv6_tnl *t = (struct ipv6_tnl *) dev->priv;
	struct flowi *fl = &t->fl;

	IPV6_TNL_TRACE();

	memset(fl, 0, sizeof (*fl));
	fl->proto = IPPROTO_IPV6;

	dev->destructor = ipv6_ipv6_tnl_dev_destructor;
	dev->uninit = ipv6_ipv6_tnl_dev_uninit;
	dev->hard_start_xmit = ipv6_ipv6_tnl_xmit;
	dev->get_stats = ipv6_ipv6_tnl_get_stats;
	dev->do_ioctl = ipv6_ipv6_tnl_ioctl;
	dev->change_mtu = ipv6_ipv6_tnl_change_mtu;
	dev->type = ARPHRD_IPV6_IPV6_TUNNEL;
	dev->flags |= IFF_NOARP;
	if (ipv6_addr_type(&t->parms.raddr) & IPV6_ADDR_UNICAST)
		dev->flags |= IFF_POINTOPOINT;
	dev->iflink = 0;
	/* Hmm... MAX_ADDR_LEN is 8, so the ipv6 addresses can't be 
	   copied to dev->dev_addr and dev->broadcast, like the ipv4
	   addresses were in ipip.c, ip_gre.c and sit.c. */
	dev->addr_len = 0;
}

/**
 * ipv6_ipv6_tnl_dev_init - initializer for all non fallback tunnel devices
 *   @dev: virtual device associated with tunnel
 **/

static int
ipv6_ipv6_tnl_dev_init(struct net_device *dev)
{
	struct ipv6_tnl *t = (struct ipv6_tnl *) dev->priv;
	int active = !(t->parms.flags & IPV6_TNL_F_KERNEL_DEV);
	struct ipv6_tnl_link_parm lp;
	int err;

	IPV6_TNL_TRACE();

	if ((err = ipv6_ipv6_tnl_link_parm_get(&t->parms, active, &lp))) {
		return err;
	}
	ipv6_ipv6_tnl_dev_init_gen(dev);
	ipv6_ipv6_tnl_dev_config(dev, &lp);
	return 0;
}

#ifdef MODULE

/**
 * ipv6_ipv6_fb_tnl_open - function called when fallback device opened
 *   @dev: fallback device
 *
 * Return: 0 
 **/

static int
ipv6_ipv6_fb_tnl_open(struct net_device *dev)
{
	IPV6_TNL_TRACE();

	MOD_INC_USE_COUNT;
	return 0;
}

/**
 * ipv6_ipv6_fb_tnl_close - function called when fallback device closed
 *   @dev: fallback device
 *
 * Return: 0 
 **/

static int
ipv6_ipv6_fb_tnl_close(struct net_device *dev)
{
	IPV6_TNL_TRACE();

	MOD_DEC_USE_COUNT;
	return 0;
}
#endif

/**
 * ipv6_ipv6_fb_tnl_dev_init - initializer for fallback tunnel device
 *   @dev: fallback device
 *
 * Return: 0
 **/

int __init
ipv6_ipv6_fb_tnl_dev_init(struct net_device *dev)
{
	IPV6_TNL_TRACE();

	ipv6_ipv6_tnl_dev_init_gen(dev);
#ifdef MODULE
	dev->open = ipv6_ipv6_fb_tnl_open;
	dev->stop = ipv6_ipv6_fb_tnl_close;
#endif
	tnls_wc[0] = &ipv6_ipv6_fb_tnl;
	return 0;
}

/**
 * ipv6_ipv6_tnl_register_hook - add hook for processing of tunneled packets
 *   @reg: hook function and its parameters
 * 
 * Description:
 *   Add a netfilter like hook function for special handling of tunneled 
 *   packets. The hook functions are called before encapsulation 
 *   (%IPV6_TNL_PRE_ENCAP) and before decapsulation 
 *   (%IPV6_TNL_PRE_DECAP). The possible return values by the hook 
 *   functions are %IPV6_TNL_DROP, %IPV6_TNL_ACCEPT and 
 *   %IPV6_TNL_STOLEN (in case the hook function took care of the packet
 *   and it doesn't have to be processed any further).
 **/

void
ipv6_ipv6_tnl_register_hook(struct ipv6_tnl_hook_ops *reg)
{
	IPV6_TNL_TRACE();

	if (reg->hooknum < IPV6_TNL_MAXHOOKS) {
		struct list_head *i;

		write_lock_bh(&ipv6_ipv6_hook_lock);
		for (i = hooks[reg->hooknum].next;
		     i != &hooks[reg->hooknum]; i = i->next) {
			if (reg->priority <
			    ((struct ipv6_tnl_hook_ops *) i)->priority) {
				break;
			}
		}
		list_add(&reg->list, i->prev);
		write_unlock_bh(&ipv6_ipv6_hook_lock);
	}
}

/**
 * ipv6_ipv6_tnl_unregister_hook - remove tunnel hook
 *   @reg: hook function and its parameters
 **/

void
ipv6_ipv6_tnl_unregister_hook(struct ipv6_tnl_hook_ops *reg)
{
	IPV6_TNL_TRACE();

	if (reg->hooknum < IPV6_TNL_MAXHOOKS) {
		write_lock_bh(&ipv6_ipv6_hook_lock);
		list_del(&reg->list);
		write_unlock_bh(&ipv6_ipv6_hook_lock);
	}
}

/* the IPv6 over IPv6 protocol structure */
static struct inet6_protocol ipv6_ipv6_protocol = {
	ipv6_ipv6_rcv,		/* IPv6 handler         */
	ipv6_ipv6_err,		/* IPv6 error control   */
	NULL,			/* next                 */
	IPPROTO_IPV6,		/* protocol ID          */
	0,			/* copy                 */
	NULL,			/* data                 */
	"IPv6 over IPv6"	/* name                 */
};

/**
 * ipv6_ipv6_tnl_init - register protocol and reserve needed resources
 *
 * Return: 0 on success
 **/

int __init
ipv6_ipv6_tnl_init(void)
{
	int i, err;
	struct sock *sk;

	IPV6_TNL_TRACE();

	ipv6_ipv6_fb_tnl_dev.priv = (void *) &ipv6_ipv6_fb_tnl;
	err = sock_create(PF_INET6, SOCK_RAW, IPPROTO_IPV6, &ipv6_socket);
	if (err < 0) {
		printk(KERN_ERR "Failed to create the IPv6 tunnel socket.\n");
		return err;
	}
	sk = ipv6_socket->sk;
	sk->allocation = GFP_ATOMIC;
	sk->net_pinfo.af_inet6.hop_limit = 254;
	sk->net_pinfo.af_inet6.mc_loop = 0;
	sk->prot->unhash(sk);

	for (i = 0; i < IPV6_TNL_MAXHOOKS; i++) {
		INIT_LIST_HEAD(&hooks[i]);
	}
	register_netdev(&ipv6_ipv6_fb_tnl_dev);
	inet6_add_protocol(&ipv6_ipv6_protocol);
	return 0;
}

/**
 * ipv6_ipv6_tnl_exit - free resources and unregister protocol
 **/

void __exit
ipv6_ipv6_tnl_exit(void)
{
	IPV6_TNL_TRACE();

	write_lock_bh(&ipv6_ipv6_kernel_lock);
	shutdown = 1;
	write_unlock_bh(&ipv6_ipv6_kernel_lock);
	flush_scheduled_tasks();
	manage_kernel_tnls(NULL);
	inet6_del_protocol(&ipv6_ipv6_protocol);
	unregister_netdev(&ipv6_ipv6_fb_tnl_dev);
	sock_release(ipv6_socket);
}

#ifdef MODULE
module_init(ipv6_ipv6_tnl_init);
module_exit(ipv6_ipv6_tnl_exit);
#endif
