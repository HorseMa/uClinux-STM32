/* $USAGI: addrconf.c,v 1.235 2004/01/15 11:35:15 yoshfuji Exp $ */

/*
 *	IPv6 Address [auto]configuration
 *	Linux INET6 implementation
 *
 *	Authors:
 *	Pedro Roque		<pedro_m@yahoo.com>	
 *	Alexey Kuznetsov	<kuznet@ms2.inr.ac.ru>
 *
 *	$Id: addrconf.c,v 1.69 2001/10/31 21:55:54 davem Exp $
 *
 *	This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

/*
 *	Changes:
 *
 *	Janos Farkas			:	delete timer on ifdown
 *	<chexum@bankinf.banki.hu>
 *	Andi Kleen			:	kill double kfree on module
 *						unload.
 *	Maciej W. Rozycki		:	FDDI support
 *	sekiya@USAGI			:	Don't send too many RS
 *						packets.
 *	yoshfuji@USAGI			:       Fixed interval between DAD
 *						packets.
 *	YOSHIFUJI Hideaki @USAGI	:	improved accuracy of
 *						address validation timer.
 *	Yuji SEKIYA @USAGI		:	Don't assign a same IPv6
 *						address on a same interface.
 *	YOSHIFUJI Hideaki @USAGI	:	ARCnet support
 *	YOSHIFUJI Hideaki @USAGI	:	improved source address
 *						selection; consider scope,
 *						status etc.
 *	yoshfuji@USAGI			:	Privacy Extensions (RFC 3041)
 *						support.
 *	Antonio Tapiador		:	Label table user level
 *	<atapiador@dit.upm.es>			configuration
 */

#include <linux/config.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/random.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/sched.h>
#include <linux/net.h>
#include <linux/in6.h>
#ifdef CONFIG_IPV6_NODEINFO
#include <linux/icmpv6.h>
#endif
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/if_arcnet.h>
#include <linux/route.h>
#include <linux/inetdevice.h>
#include <linux/init.h>
#ifdef CONFIG_SYSCTL
#include <linux/sysctl.h>
#endif
#include <linux/delay.h>
#include <linux/notifier.h>

#include <linux/proc_fs.h>
#include <net/sock.h>
#include <net/snmp.h>

#include <net/addrconf.h>
#include <net/ipv6.h>
#include <net/protocol.h>
#include <net/ndisc.h>
#include <net/ip6_route.h>
#include <net/addrconf.h>
#include <net/ip.h>
#include <linux/if_tunnel.h>
#include <linux/rtnetlink.h>

#ifdef CONFIG_IPV6_PRIVACY
#include <linux/crypto.h>
#include <asm/scatterlist.h>
#endif

#include <asm/uaccess.h>

#ifdef CONFIG_IPV6_ACONF_DEBUG
#include <linux/inet.h>
#endif

/* Set to 3 to get tracing... */
#ifdef CONFIG_IPV6_ACONF_DEBUG
#define ACONF_DEBUG 3
#else
#define ACONF_DEBUG 1
#endif

#define ADBG(x)		printk x
#define NOADBG(x)	do { ; } while(0)
#if ACONF_DEBUG >= 3
#define ADBG3(x)	ADBG(x)
#else
#define ADBG3(x)	NOADBG(x)
#endif

#if ACONF_DEBUG >= 2
#define ADBG2(x)	ADBG(x)
#else
#define ADBG2(x)	NOADBG(x)
#endif

#if ACONF_DEBUG >=1
#define ADBG1(x)	ADBG(x)
#else
#define ADBG1(x)	NOADBG(x)
#endif

/* /proc/net/snmp6, /proc/net/dev_snmp6/ things */
#ifdef CONFIG_PROC_FS
extern struct proc_dir_entry *proc_net_devsnmp6;
extern int afinet6_read_devsnmp(char *buffer, 
				char **start, off_t offset, int length, 
				int *eof, void *data);
#endif

#if HZ == 100
#define	timeticks(jif)	(jif)
#elif HZ < 100
#define timeticks(jif)	({ unsigned long _j=(jif);			\
			   unsigned long _j1 = _j/HZ, _j2 = _j%HZ;	\
			   (100/HZ)*_j1*HZ + 				\
			   (100/HZ)*_j2+_j1*(100%HZ) +			\
			   ((100%HZ)*_j2)/HZ;				\
			})
#else
#define timeticks(jif)	({ unsigned long _j=(jif);			\
			   (_j/HZ)*(100%HZ)+((100%HZ)*(_j%HZ))/HZ;	\
			})
#endif

#define INFINITE	0xffffffff

#ifdef CONFIG_SYSCTL
static void addrconf_sysctl_register(struct inet6_dev *idev, struct ipv6_devconf *p);
static void addrconf_sysctl_unregister(struct ipv6_devconf *p);
#endif

int inet6_dev_count;
int inet6_ifa_count;

#ifdef CONFIG_IPV6_PRIVACY
static int __ipv6_regen_rndid(struct inet6_dev *idev);
static int __ipv6_try_regen_rndid(struct inet6_dev *idev, struct in6_addr *tmpaddr); 
static void ipv6_regen_rndid(unsigned long data);

static int desync_factor = MAX_DESYNC_FACTOR * HZ;
static struct crypto_tfm *md5_tfm;
static spinlock_t md5_tfm_lock = SPIN_LOCK_UNLOCKED;
#endif

int ipv6_count_addresses(struct inet6_dev *idev);
static u32 ipv6_addrselect_label_lookup(const struct in6_addr *addr, int ifindex);

struct ipv6_addrselect_label 
{
	struct ipv6_addrselect_label *next;
	struct in6_addr addr;
	u16     plen;
	u32     ifindex;
	u32     label;
};

struct ipv6_addrselect_label 	*ipv6_addrselect_label_table;
rwlock_t	addrselect_label_lock = RW_LOCK_UNLOCKED;

/*
 *	Configured unicast address hash table
 */
static struct inet6_ifaddr		*inet6_addr_lst[IN6_ADDR_HSIZE];
static rwlock_t	addrconf_hash_lock = RW_LOCK_UNLOCKED;

/* Protects inet6 devices */
rwlock_t addrconf_lock = RW_LOCK_UNLOCKED;

void addrconf_verify(unsigned long);

static struct timer_list addr_chk_timer = { function: addrconf_verify };
static spinlock_t addrconf_verify_lock = SPIN_LOCK_UNLOCKED;

static int addrconf_ifdown(struct net_device *dev, int how);

static void addrconf_dad_start(struct inet6_ifaddr *ifp, int flags);
static void addrconf_dad_timer(unsigned long data);
static void addrconf_dad_completed(struct inet6_ifaddr *ifp);
static void addrconf_rs_timer(unsigned long data);
static void ipv6_ifa_notify(int event, struct inet6_ifaddr *ifa);

static int ipv6_chk_same_addr(const struct in6_addr *addr, struct net_device *dev);

static struct notifier_block *inet6addr_chain;

struct ipv6_devconf ipv6_devconf =
{
	0,				/* forwarding		*/
	IPV6_DEFAULT_HOPLIMIT,		/* hop limit		*/
	IPV6_MIN_MTU,			/* mtu			*/
	1,				/* accept RAs		*/
	1,				/* accept redirects	*/
	1,				/* autoconfiguration	*/
	1,				/* dad transmits	*/
	MAX_RTR_SOLICITATIONS,		/* router solicits	*/
	RTR_SOLICITATION_INTERVAL,	/* rtr solicit interval	*/
	MAX_RTR_SOLICITATION_DELAY,	/* rtr solicit delay	*/
	max_addresses: 		IPV6_MAX_ADDRESSES,
#ifdef CONFIG_IPV6_NODEINFO
	accept_ni:		1,
#endif
#ifdef CONFIG_IPV6_PRIVACY
	use_tempaddr:		1,
	temp_valid_lft:		TEMP_VALID_LIFETIME,
	temp_prefered_lft:	TEMP_PREFERRED_LIFETIME,
	regen_max_retry:	REGEN_MAX_RETRY,
	max_desync_factor:	MAX_DESYNC_FACTOR,
#endif
};

static struct ipv6_devconf ipv6_devconf_dflt =
{
	0,				/* forwarding		*/
	IPV6_DEFAULT_HOPLIMIT,		/* hop limit		*/
	IPV6_MIN_MTU,			/* mtu			*/
	1,				/* accept RAs		*/
	1,				/* accept redirects	*/
	1,				/* autoconfiguration	*/
	1,				/* dad transmits	*/
	MAX_RTR_SOLICITATIONS,		/* router solicits	*/
	RTR_SOLICITATION_INTERVAL,	/* rtr solicit interval	*/
	MAX_RTR_SOLICITATION_DELAY,	/* rtr solicit delay	*/
	max_addresses: 		IPV6_MAX_ADDRESSES,
#ifdef CONFIG_IPV6_NODEINFO
	accept_ni:		1,
#endif
#ifdef CONFIG_IPV6_PRIVACY
	use_tempaddr:		1,
	temp_valid_lft:		TEMP_VALID_LIFETIME,
	temp_prefered_lft:	TEMP_PREFERRED_LIFETIME,
	regen_max_retry:	REGEN_MAX_RETRY,
	max_desync_factor:	MAX_DESYNC_FACTOR,
#endif
};

/* IPv6 Wildcard Address and Loopback Address defined by RFC3493 */
const struct in6_addr in6addr_any = IN6ADDR_ANY_INIT;
const struct in6_addr in6addr_loopback = IN6ADDR_LOOPBACK_INIT;

int __ipv6_addr_type(const struct in6_addr *addr)
{
	int type;
	u32 st;

	st = addr->s6_addr32[0];

	if ((st & htonl(0xFF000000)) == htonl(0xFF000000)) {
		/* multicast */

		type = IPV6_ADDR_MC_SCOPE(addr)<<16;	/* addr-select 3.1 */
		switch(type) {
		case IPV6_ADDR_SCOPE_NODELOCAL<<16:
			type |= IPV6_ADDR_LOOPBACK;
			break;

		case IPV6_ADDR_SCOPE_LINKLOCAL<<16:
			type |= IPV6_ADDR_LINKLOCAL;
			break;

		case IPV6_ADDR_SCOPE_SITELOCAL<<16:
			type |= IPV6_ADDR_SITELOCAL;
			break;
#if 0
		default:
			/* XXX: orglocal, global and other scopes */
#endif
		}
		type |= IPV6_ADDR_MULTICAST;
		return type;
	}

	if ((st & htonl(0xE0000000)) == htonl(0x00000000)) {
		if (st == htonl(0x00000000)) {
			/* ipv4-compatible, ipv4-mapped and reserved addresses */
			if ((addr->s6_addr32[1]|addr->s6_addr32[2]) == htonl(0x00000000)) {
				if (addr->s6_addr32[3] == htonl(0x00000000))
					return IPV6_ADDR_ANY;
				if (addr->s6_addr32[3] == htonl(0x00000001))
					return (IPV6_ADDR_LOOPBACK | IPV6_ADDR_UNICAST |
						IPV6_ADDR_SCOPE_LINKLOCAL<<16);	/* addr-select 3.4 */
				return (IPV6_ADDR_COMPATv4 | 
					IPV6_ADDR_SCOPE_GLOBAL<<16);		/* addr-select 3.3 */
			}
			if (addr->s6_addr32[1] == htonl(0x00000000) &&
			    addr->s6_addr32[2] == htonl(0x0000ffff)) {
				return (IPV6_ADDR_MAPPED | 
					IPV6_ADDR_SCOPE_GLOBAL<<16);	/* addr-select 3.3 */
			}
		}
		/* NSAP and IPX addresses, too */
		return (IPV6_ADDR_RESERVED | 
			IPV6_ADDR_SCOPE_GLOBAL<<16);		/* addr-select 3.3 */
	}

	/* check for reserved anycast addresses */
	if ((st & htonl(0xE0000000)) &&
		((addr->s6_addr32[2] == htonl(0xFDFFFFFF) &&
		(addr->s6_addr32[3] | htonl(0x0000007F)) == (u32)~0) ||
		(addr->s6_addr32[2] == 0 && addr->s6_addr32[3] == 0))) {
		type = IPV6_ADDR_ANYCAST;
	} else
		type = IPV6_ADDR_UNICAST;

	/* check for local-use unicasts */
	if ((st & htonl(0xE0000000)) == htonl(0xE0000000)) {
		switch(st & htonl(0xFFC00000)) {
		case __constant_htonl(0xFE800000):
			type |= IPV6_ADDR_LINKLOCAL | IPV6_ADDR_SCOPE_LINKLOCAL<<16;
			break;
		case __constant_htonl(0xFEC00000):
			type |= IPV6_ADDR_SITELOCAL | IPV6_ADDR_SCOPE_SITELOCAL<<16;
			break;
		default:
			return IPV6_ADDR_RESERVED | IPV6_ADDR_SCOPE_GLOBAL<<16;
		}
	} else {
		type |= IPV6_ADDR_SCOPE_GLOBAL<<16;	/* addr-select 3.3 */
	}

	return type;
}

static void addrconf_del_timer(struct inet6_ifaddr *ifp)
{
	ADBG3((KERN_DEBUG
	       "addrconf_del_timer(ifp=%p)\n",
	       ifp));

	if (del_timer(&ifp->timer))
		__in6_ifa_put(ifp);
}

enum addrconf_timer_t
{
	AC_NONE,
	AC_DAD,
	AC_RS,
};

static void addrconf_mod_timer(struct inet6_ifaddr *ifp,
			       enum addrconf_timer_t what,
			       unsigned long when)
{
	ADBG3((KERN_DEBUG
	       "addrconf_mod_timer(ifp=%p, what=%d, when=%lu): jiffies=%lu\n",
	       ifp, what, when, jiffies));

	if (!del_timer(&ifp->timer))
		in6_ifa_hold(ifp);

	switch (what) {
	case AC_DAD:
		ifp->timer.function = addrconf_dad_timer;
		ADBG3((KERN_DEBUG "AC_DAD timer = %d\n", ifp->probes));
		break;
	case AC_RS:
		ifp->timer.function = addrconf_rs_timer;
		ADBG3((KERN_DEBUG "AC_RS timer = %d\n", ifp->probes));

		/* Check whether entering AC_RS function after receiving RA */
		if (ifp->idev->if_flags & IF_RA_RCVD)
			ADBG1((KERN_DEBUG "AC_RS timer is called when IF_RA_RCVD is on.\n"));
		break;
	default:;
	}
	ifp->timer.expires = jiffies + when;
	add_timer(&ifp->timer);
}

/* Nobody refers to this device, we may destroy it. */

void in6_dev_finish_destroy(struct inet6_dev *idev)
{
	struct net_device *dev = idev->dev;
	BUG_TRAP(idev->addr_list==NULL);
	BUG_TRAP(idev->mc_list==NULL);
#ifdef CONFIG_IPV6_PREFIXLIST
	BUG_TRAP(list_empty(&idev->prefix_list) == 1);
#endif
#ifdef NET_REFCNT_DEBUG
	ADBG3((KERN_DEBUG "in6_dev_finish_destroy: %s\n", dev ? dev->name : "NIL"));
#endif
	dev_put(dev);
	if (!idev->dead) {
		ADBG1((KERN_WARNING
		       "Freeing alive inet6 device %p\n", idev));
		return;
	}
#ifdef CONFIG_PROC_FS
	if (idev->stats.proc_dir_entry){
		remove_proc_entry(idev->stats.proc_dir_entry->name, 
				  proc_net_devsnmp6);
	}
#endif
	ipv6_statistics[0].Ip6LastChange = timeticks(jiffies);
	inet6_dev_count--;
	kfree(idev);
}

static struct inet6_dev * ipv6_add_dev(struct net_device *dev)
{
	struct inet6_dev *ndev;

	ASSERT_RTNL();

	ADBG3((KERN_DEBUG
	       "ipv6_add_dev(dev=%p(%s))\n",
		dev, dev->name));

	if (dev->mtu < IPV6_MIN_MTU)
		return NULL;

	ndev = kmalloc(sizeof(struct inet6_dev), GFP_KERNEL);

	if (ndev) {
#ifdef CONFIG_IPV6_ZONE
		int i;
#endif
		memset(ndev, 0, sizeof(struct inet6_dev));

		INIT_LIST_HEAD(&ndev->prefix_list);
		ndev->prefix_lock = RW_LOCK_UNLOCKED;
		ndev->lock = RW_LOCK_UNLOCKED;
		ndev->dev = dev;
		ndev->prefix_lock = RW_LOCK_UNLOCKED;
		INIT_LIST_HEAD(&ndev->prefix_list);
		memcpy(&ndev->cnf, &ipv6_devconf_dflt, sizeof(ndev->cnf));
		ndev->cnf.mtu6 = dev->mtu;
		ndev->cnf.sysctl = NULL;
		ndev->nd_parms = neigh_parms_alloc(dev, &nd_tbl);
		if (ndev->nd_parms == NULL) {
			kfree(ndev);
			return NULL;
		}

		inet6_dev_count++;
		/* We refer to the device */
		dev_hold(dev);

#ifdef CONFIG_IPV6_ZONE
		memset(&ndev->zone, 0, sizeof(ndev->zone));
		for (i=0; i<16; i++) {
			if (
#ifndef CONFIG_IPV6_ZONE_SITELOCAL
			    i <= IPV6_ADDR_SCOPE_LINKLOCAL
#else
			    i <= IPV6_ADDR_SCOPE_SITELOCAL
#endif
			   )
				ndev->zone.zoneid[i] = dev->ifindex;
			else
				ndev->zone.zoneid[i] = 0x0000001;
		}
#endif

		if ((dev->flags&IFF_LOOPBACK) ||
#ifdef CONFIG_NET_IPIP_IPV6
		    dev->type == ARPHRD_TUNNEL
#else
		    dev->type == ARPHRD_SIT
#endif
		    ) {
		}

		/* One reference from device */
		in6_dev_hold(ndev);

#ifdef CONFIG_IPV6_PRIVACY
		get_random_bytes(ndev->rndid, sizeof(ndev->rndid));
		get_random_bytes(ndev->entropy, sizeof(ndev->entropy));
		init_timer(&ndev->regen_timer);
		ndev->regen_timer.function = ipv6_regen_rndid;
		ndev->regen_timer.data = (unsigned long) ndev;
		if ((dev->flags&IFF_LOOPBACK) ||
		    dev->type == ARPHRD_TUNNEL ||
		    dev->type == ARPHRD_SIT) {
			ADBG2((KERN_DEBUG
			       "Disabled Privacy Extensions on device %p(%s)\n",
			       dev, dev->name));
			ndev->cnf.use_tempaddr = -1;
		} else {
			__ipv6_regen_rndid(ndev);
		}
#endif

		write_lock_bh(&addrconf_lock);
		dev->ip6_ptr = ndev;
		write_unlock_bh(&addrconf_lock);
		ipv6_statistics[0].Ip6LastChange = timeticks(jiffies);
		ndev->stats.ipv6[0].Ip6LastChange = timeticks(jiffies);

#ifdef CONFIG_PROC_FS
		ndev->stats.proc_dir_entry = create_proc_read_entry(dev->name, 0, 
								    proc_net_devsnmp6, 
								    afinet6_read_devsnmp, 
								    ndev);
		if (!ndev->stats.proc_dir_entry)
			ADBG1((KERN_WARNING
			       "addrconf_notify(): cannot create /proc/net/dev_snmp6/%s\n", 
			       dev->name));
#endif

		ipv6_mc_init_dev(ndev);

#ifdef CONFIG_SYSCTL
		neigh_sysctl_register(dev, ndev->nd_parms, NET_IPV6, NET_IPV6_NEIGH, "ipv6");
		addrconf_sysctl_register(ndev, &ndev->cnf);
#endif
	}
	return ndev;
}

static struct inet6_dev * ipv6_find_idev(struct net_device *dev)
{
	struct inet6_dev *idev;

	ASSERT_RTNL();

	ADBG3((KERN_DEBUG
	       "ipv6_find_idev(dev=%p(%s))\n",
	       dev, dev->name));

	if ((idev = __in6_dev_get(dev)) == NULL) {
		if ((idev = ipv6_add_dev(dev)) == NULL)
			return NULL;
	}
	if (dev->flags&IFF_UP)
		ipv6_mc_up(idev);
	return idev;
}

static void dev_forward_change(struct inet6_dev *idev)
{
	struct net_device *dev;
	struct inet6_ifaddr *ifa;
	struct in6_addr addr;

	ADBG3((KERN_DEBUG
	       "dev_forward_change(idev=%p)\n", idev));

	if (!idev || idev->dead)
		return;

	dev = idev->dev;
	if (!dev)
		return;

	ipv6_addr_all_routers(&addr);

	read_lock(&idev->lock);
	if (idev->cnf.forwarding) {
		read_unlock(&idev->lock);
		ipv6_dev_mc_inc(dev, &addr);	/* XXX: race */
	} else {
		read_unlock(&idev->lock);
		ipv6_dev_mc_dec(dev, &addr);	/* XXX: race */
	}

	for (ifa=idev->addr_list; ifa; ifa=ifa->if_next) {
		ipv6_addr_prefix(&addr, &ifa->addr, ifa->prefix_len);
		if (ipv6_addr_any(&addr))
			continue;
		read_lock(&idev->lock);
		if (idev->cnf.forwarding) {
			read_unlock(&idev->lock);
			ipv6_dev_ac_inc(idev->dev, &addr);
		} else {
			read_unlock(&idev->lock);
			ipv6_dev_ac_dec(idev->dev, &addr);
		}
	}
}

static void addrconf_forward_change(void)
{
	struct net_device *dev;

	ADBG3((KERN_DEBUG "addrconf_forward_change()\n"));

	read_lock(&dev_base_lock);
	read_lock(&addrconf_lock);
	for (dev=dev_base; dev; dev=dev->next) {
		struct inet6_dev *idev = __in6_dev_get(dev);
		if (idev) {
			int changed = 0;
			write_lock(&idev->lock);
			if (!idev->dead) {
				changed = idev->cnf.forwarding != ipv6_devconf.forwarding;
				idev->cnf.forwarding = ipv6_devconf.forwarding;
			}
			write_unlock(&idev->lock);
			if (changed)
				dev_forward_change(idev);
		}
	}
	ipv6_devconf_dflt.forwarding = ipv6_devconf.forwarding;
	read_unlock(&addrconf_lock);
	read_unlock(&dev_base_lock);
}


/* Nobody refers to this ifaddr, destroy it */

void inet6_ifa_finish_destroy(struct inet6_ifaddr *ifp)
{
	ADBG3((KERN_DEBUG
	       "inet6_ifa_finish_destroy(ifp=%p)\n",
	       ifp));

	BUG_TRAP(ifp->if_next==NULL);
	BUG_TRAP(ifp->lst_next==NULL);
#ifdef NET_REFCNT_DEBUG
	ADBG3((KERN_DEBUG "inet6_ifa_finish_destroy\n"));
#endif

	in6_dev_put(ifp->idev);

	if (del_timer(&ifp->timer))
		ADBG1((KERN_WARNING
		       "Timer is still running, when freeing ifa=%p\n", ifp));

	if (!ifp->dead) {
		ADBG1((KERN_WARNING
		       "Freeing alive inet6 address %p\n", ifp));
		return;
	}
	inet6_ifa_count--;
	kfree(ifp);
}

/* On success it returns ifp with increased reference count */

static struct inet6_ifaddr *
ipv6_add_addr(struct inet6_dev *idev, const struct in6_addr *addr, int pfxlen,
	      int scope, unsigned flags)
{
	struct inet6_ifaddr *ifa;
	int hash;
	static spinlock_t lock = SPIN_LOCK_UNLOCKED;
#ifdef CONFIG_IPV6_ACONF_DEBUG
	char abuf[128];

	in6_ntop(addr, abuf);
	ADBG3((KERN_DEBUG
	       "ipv6_add_addr(idev=%p, addr=%s, pfxlen=%d, scope=%d, flags=%08X)\n",
		idev, abuf, pfxlen, scope, flags));
#endif

	spin_lock_bh(&lock);

	/* Ignore adding duplicate addresses on an interface */
	if (ipv6_chk_same_addr(addr, idev->dev)) {
		spin_unlock_bh(&lock);
		ADBG(("ipv6_add_addr: already assigned\n"));
		return ERR_PTR(-EEXIST);
	}

	ifa = kmalloc(sizeof(struct inet6_ifaddr), GFP_ATOMIC);

	if (ifa == NULL) {
		spin_unlock_bh(&lock);
		ADBG1((KERN_WARNING
		       "ipv6_add_addr: malloc failed\n"));
		return NULL;
	}

	memset(ifa, 0, sizeof(struct inet6_ifaddr));
	ipv6_addr_copy(&ifa->addr, addr);

	spin_lock_init(&ifa->lock);
	init_timer(&ifa->timer);
	ifa->timer.data = (unsigned long) ifa;
	ifa->scope = scope;
	ifa->prefix_len = pfxlen;
	ifa->flags = flags | IFA_F_TENTATIVE;

	read_lock(&addrconf_lock);
	if (idev->dead) {
		read_unlock(&addrconf_lock);
		spin_unlock_bh(&lock);
		kfree(ifa);
		return ERR_PTR(-ENODEV);	/*XXX*/
	}

	inet6_ifa_count++;
	ifa->idev = idev;
	in6_dev_hold(idev);
	/* For caller */
	in6_ifa_hold(ifa);

	/* Add to big hash table */
	hash = ipv6_addr_hash(addr);

	write_lock_bh(&addrconf_hash_lock);
	ifa->lst_next = inet6_addr_lst[hash];
	inet6_addr_lst[hash] = ifa;
	in6_ifa_hold(ifa);
	write_unlock_bh(&addrconf_hash_lock);

	write_lock_bh(&idev->lock);
	/* Add to inet6_dev unicast addr list. */
	ifa->if_next = idev->addr_list;
	idev->addr_list = ifa;
	in6_ifa_hold(ifa);
#ifdef CONFIG_IPV6_PRIVACY
	ifa->regen_count = 0;
	if (ifa->flags&IFA_F_TEMPORARY) {
		ifa->tmp_next = idev->tempaddr_list;
		idev->tempaddr_list = ifa;
		in6_ifa_hold(ifa);
	} else {
		ifa->tmp_next = NULL;
	}
#endif
	write_unlock_bh(&idev->lock);
	read_unlock(&addrconf_lock);
	spin_unlock_bh(&lock);

	notifier_call_chain(&inet6addr_chain,NETDEV_UP,ifa);

	return ifa;
}

/* This function wants to get referenced ifp and releases it before return */

static void ipv6_del_addr(struct inet6_ifaddr *ifp)
{
	struct inet6_ifaddr *ifa, **ifap;
	struct inet6_dev *idev = ifp->idev;
	int hash;

	ADBG3((KERN_DEBUG
	       "ipv6_del_addr(ifp=%p)\n", ifp));

	ifp->dead = 1;

#ifdef CONFIG_IPV6_PRIVACY
	spin_lock_bh(&ifp->lock);
	if (ifp->ifpub) {
		__in6_ifa_put(ifp->ifpub);
		ifp->ifpub = NULL;
	}
	spin_unlock_bh(&ifp->lock);
#endif

	hash = ipv6_addr_hash(&ifp->addr);

	write_lock_bh(&addrconf_hash_lock);
	for (ifap = &inet6_addr_lst[hash]; (ifa=*ifap) != NULL;
	     ifap = &ifa->lst_next) {
		if (ifa == ifp) {
			*ifap = ifa->lst_next;
			__in6_ifa_put(ifp);
			ifa->lst_next = NULL;
			break;
		}
	}
	write_unlock_bh(&addrconf_hash_lock);

	write_lock_bh(&idev->lock);
#ifdef CONFIG_IPV6_PRIVACY
	if (ifp->flags&IFA_F_TEMPORARY) {
		for (ifap = &idev->tempaddr_list; (ifa=*ifap) != NULL;
		     ifap = &ifa->tmp_next) {
			if (ifa == ifp) {
				*ifap = ifa->tmp_next;
				if (ifp->ifpub) {
					__in6_ifa_put(ifp->ifpub);
					ifp->ifpub = NULL;
				}
				__in6_ifa_put(ifp);
				ifa->tmp_next = NULL;
				break;
			}
		}
	}
#endif
	for (ifap = &idev->addr_list; (ifa=*ifap) != NULL;
	     ifap = &ifa->if_next) {
		if (ifa == ifp) {
			*ifap = ifa->if_next;
			__in6_ifa_put(ifp);
			ifa->if_next = NULL;
			break;
		}
	}

	/* XXX: if ifp is public, also delete tmporary adddress */

	write_unlock_bh(&idev->lock);

	ipv6_ifa_notify(RTM_DELADDR, ifp);

	notifier_call_chain(&inet6addr_chain,NETDEV_DOWN,ifp);

	addrconf_del_timer(ifp);

	in6_ifa_put(ifp);
}

#ifdef CONFIG_IPV6_PRIVACY
static int ipv6_create_tempaddr(struct inet6_ifaddr *ifp, struct inet6_ifaddr *ift)
{
	struct inet6_dev *idev;
	struct in6_addr addr, *tmpaddr;
	unsigned long tmp_prefered_lft, tmp_valid_lft;
	int tmp_plen;
	int ret = 0;
	int max_addresses;

	ADBG3((KERN_DEBUG
	       "ipv6_create_tempaddr(ifp=%p, ift=%p)\n", 
	       ifp, ift));

	if (ift) {
		spin_lock_bh(&ift->lock);
		memcpy(&addr.s6_addr[8], &ift->addr.s6_addr[8], 8);
		spin_unlock_bh(&ift->lock);
		tmpaddr = &addr;
	} else {
		tmpaddr = NULL;
	}
retry:
	spin_lock_bh(&ifp->lock);
	in6_ifa_hold(ifp);
	idev = ifp->idev;
	in6_dev_hold(idev);
	memcpy(addr.s6_addr, ifp->addr.s6_addr, 8);
	write_lock(&idev->lock);
	if (idev->cnf.use_tempaddr <= 0) {
		write_unlock(&idev->lock);
		spin_unlock_bh(&ifp->lock);
		ADBG2((KERN_INFO
		       "ipv6_create_tempaddr(): use_tempaddr is disabled.\n"));
		in6_dev_put(idev);
		in6_ifa_put(ifp);
		ret = -1;
		goto out;
	}
	if (ifp->regen_count++ >= idev->cnf.regen_max_retry) {
		idev->cnf.use_tempaddr = -1;	/*XXX*/
		write_unlock(&idev->lock);
		spin_unlock_bh(&ifp->lock);
		ADBG1((KERN_WARNING
		       "ipv6_create_tempaddr(): regeneration time exceeded. disabled temporary address support.\n"));
		in6_dev_put(idev);
		in6_ifa_put(ifp);
		ret = -1;
		goto out;
	}
	if (__ipv6_try_regen_rndid(idev, tmpaddr) < 0) {
		write_unlock(&idev->lock);
		spin_unlock_bh(&ifp->lock);
		ADBG1((KERN_WARNING
		       "ipv6_create_tempaddr(): regeneration of randomized interface id failed.\n"));
		in6_dev_put(idev);
		in6_ifa_put(ifp);
		ret = -1;
		goto out;
	}
	memcpy(&addr.s6_addr[8], idev->rndid, 8);
	tmp_valid_lft = min_t(__u32,
			      ifp->valid_lft,
			      idev->cnf.temp_valid_lft);
	tmp_prefered_lft = min_t(__u32, 
				 ifp->prefered_lft, 
				 idev->cnf.temp_prefered_lft - desync_factor / HZ);
	tmp_plen = ifp->prefix_len;
	max_addresses = idev->cnf.max_addresses;
	write_unlock(&idev->lock);
	spin_unlock_bh(&ifp->lock);
	ift = !max_addresses ||
	      ipv6_count_addresses(idev) < max_addresses ?
		ipv6_add_addr(idev, &addr, tmp_plen,
			      ipv6_addr_type(&addr)&IPV6_ADDR_SCOPE_MASK, IFA_F_TEMPORARY) : 0;
	if (!ift) {
		in6_dev_put(idev);
		in6_ifa_put(ifp);
		ADBG2((KERN_WARNING
		       "ipv6_create_tempaddr(): retry temporary address regeneration.\n"));
		tmpaddr = &addr;
		goto retry;
	}
	spin_lock_bh(&ift->lock);
	ift->ifpub = ifp;
	ift->valid_lft = tmp_valid_lft;
	ift->prefered_lft = tmp_prefered_lft;
	ift->tstamp = ifp->tstamp;
	spin_unlock_bh(&ift->lock);
	addrconf_dad_start(ift, RTF_ADDRCONF|RTF_PREFIX_RT);
	in6_ifa_put(ift);
	in6_dev_put(idev);
out:
	return ret;
}

#endif

int ipv6_get_prefix_entries(struct prefix_info **plist, int ifindex, int plen)
{
	int count;
	struct net_device *dev;
	struct inet6_dev *idev;
	struct list_head *head;
	struct prefix_element *p;

	if (plist == NULL) {
		BUG_TRAP(plist != NULL);
		return -EINVAL;
	}
	if ((dev = dev_get_by_index(ifindex)) == NULL) {
		printk(KERN_WARNING "Bad I/F (%d) in ipv6_get_prefix_entries\n",
		       ifindex);
		return -EINVAL;
	}

	if ((idev = __in6_dev_get(dev)) == NULL) {
		dev_put(dev);
		return -EINVAL;
	}

	read_lock_bh(&idev->lock);
	if (!(count = idev->prefix_count)) {
		/* No elements on list */
		goto out;
	}
	if ((*plist = kmalloc(count * sizeof(struct prefix_info),
			      GFP_ATOMIC)) == NULL) {
		count = -ENOMEM;
		goto out;
	}
	count = 0;
	spin_lock_bh(&idev->prefix_lock);
	list_for_each(head, &idev->prefix_list) {
		p = list_entry(head, struct prefix_element, list);
		if (plen == 0 || p->pinfo.prefix_len == plen) {
			memcpy(*plist + count, &p->pinfo,
			       sizeof(struct prefix_info));
			count++;
		}
	}
	spin_unlock_bh(&idev->prefix_lock);
 out:
	read_unlock_bh(&idev->lock);
	dev_put(dev);
	return count;
}

/*
 *	Choose an appropriate source address
 *	draft-ietf-ipv6-default-addr-select-09.txt
 */
#define IPV6_SADDRSELECT_SELF		0x01
#define IPV6_SADDRSELECT_PREFERRED	0x02
#define IPV6_SADDRSELECT_HOME		0x04
#define IPV6_SADDRSELECT_PUBLIC		0x08
#define IPV6_SADDRSELECT_INTERFACE	0x10
#define IPV6_SADDRSELECT_LABEL		0x20

struct addrselect_attrs {
	struct inet6_ifaddr *ifp;
	u16	flags;
	s16	matchlen;
	u8	scope;
};

static int __inline__ ipv6_addrselect_preferred(int type)
{
#if 0
	if (type == IPV6_ADDR_ANY)
		return -1;
#endif
	/* section 3.3, 3.4 */
	if (type&(IPV6_ADDR_MAPPED|IPV6_ADDR_COMPATv4|
		  IPV6_ADDR_LOOPBACK|IPV6_ADDR_RESERVED))
		return 1;
	return 0;
}

int ipv6_dev_get_saddr(struct net_device *daddr_dev,
		       struct in6_addr *daddr, struct in6_addr *saddr,
		       int pref_privacy)
{
	int daddr_type, daddr_scope;
	u32 daddr_label;
	struct inet6_ifaddr *ifp0, *ifp = NULL;
	struct net_device *dev;
	struct inet6_dev *idev;
#ifdef CONFIG_IPV6_ACONF_DEBUG
	char daddrbuf[128];
	char saddrbuf[128];
#endif

	int err;
	int update;
	struct addrselect_attrs candidate = {NULL,0,0,0};
#if defined(CONFIG_IPV6_PRIVACY)
	u16 invpref = 0;
#endif

#ifdef CONFIG_IPV6_PRIVACY
	if (pref_privacy > 0 ||
	    (pref_privacy == 0 && ipv6_devconf.use_tempaddr > 1))
		invpref |= IPV6_SADDRSELECT_PUBLIC;
#endif

	daddr_type = __ipv6_addr_type(daddr);
	daddr_scope = __ipv6_addr_src_scope(daddr_type);
	daddr_label = ipv6_addrselect_label_lookup(daddr, daddr_dev?daddr_dev->ifindex:0);

#ifdef CONFIG_IPV6_ACONF_DEBUG
	in6_ntop(daddr, daddrbuf);
	ADBG3((KERN_DEBUG 
	       "ipv6_dev_get_saddr(daddr_dev=%p(%s), daddr=%s, saddr=%p, pref_privacy=%d)\n", 
	       daddr_dev, daddr_dev ? daddr_dev->name : "<null>", 
	       daddrbuf, saddr, pref_privacy));
#endif

	read_lock(&dev_base_lock);
	read_lock(&addrconf_lock);
	for (dev = dev_base; dev; dev=dev->next) {
#ifdef CONFIG_IPV6_ACONF_DEBUG_SADDR
		ADBG3((KERN_DEBUG 
		       "ipv6_dev_get_saddr():  dev=%p(%s)\n", dev, dev->name));
#endif

		/* Rule 0: Candidate Source Address (section 4)
		 *  - multicast and link-local destination address,
		 *    the set of candidate source address MUST only
		 *    include addresses assigned to interfaces
		 *    belonging to the same link as the outgoing
		 *    interface.
		 * (- For site-local destination addresses, the 
		 *    set of candidate source addresses MUST only
		 *    include addresses assigned to interfaces 
		 *    belonging to the same site as the outgoing 
		 *    interface.)
		 */
		if ((daddr_type&IPV6_ADDR_MULTICAST ||
		     daddr_scope <= IPV6_ADDR_SCOPE_LINKLOCAL) &&
		    daddr_dev && dev != daddr_dev)
			continue;

		idev = __in6_dev_get(dev);
#ifdef CONFIG_IPV6_ACONF_DEBUG_SADDR
		ADBG3((KERN_DEBUG
		       "ipv6_dev_get_saddr():  idev=%p\n", idev));
#endif
		if (!idev)
			continue;

		read_lock_bh(&idev->lock);
		ifp0 = idev->addr_list;
		for (ifp=ifp0; ifp; ifp=ifp->if_next) {
			struct addrselect_attrs temp = {NULL,0,0,0};
			int addr_type;
			update = 0;
#ifdef CONFIG_IPV6_ACONF_DEBUG_SADDR
			in6_ntop(&ifp->addr, saddrbuf);
			ADBG3((KERN_DEBUG
			       "ipv6_dev_get_saddr():   ifp=%p(ifp->addr=%s)\n", ifp, saddrbuf));
#endif

			/* Rule 0: Candidate Source Address (section 4)
			 *  - In any case, anycast addresses, multicast
			 *    addresses, and the unspecified address MUST
			 *    NOT be included in a candidate set.
			 */
			addr_type = __ipv6_addr_type(&ifp->addr);
			if (addr_type == IPV6_ADDR_ANY ||
			    addr_type&IPV6_ADDR_MULTICAST)
				continue;

			/* Rule 1: Prefer same address */
			if (ipv6_addr_cmp(&ifp->addr, daddr) == 0)
				temp.flags |= IPV6_SADDRSELECT_SELF;
#ifdef CONFIG_IPV6_ACONF_DEBUG_SADDR
				ADBG3((KERN_DEBUG
				       "ipv6_dev_get_saddr():    update=%d, Rule 1: %d vs %d\n",
				       update,
				       temp.flags&IPV6_SADDRSELECT_SELF?1:0,
				       candidate.flags&IPV6_SADDRSELECT_SELF?1:0));
#endif
			if ((temp.flags^candidate.flags)&IPV6_SADDRSELECT_SELF) {
				update = temp.flags&IPV6_SADDRSELECT_SELF;
				if (!update) {
#ifdef CONFIG_IPV6_ACONF_DEBUG_SADDR
					ADBG3((KERN_DEBUG
					       "ipv6_dev_get_saddr():    lose at Rule 1\n"));
#endif
					continue;
				}
			}

			/* Rule 2: Prefer appropriate scope */
			temp.scope = __ipv6_addr_src_scope(addr_type);
#ifdef CONFIG_IPV6_ACONF_DEBUG_SADDR
			ADBG3((KERN_DEBUG
			       "ipv6_dev_get_saddr():    update=%d, Rule 2: %u vs %u\n",
			       update,
			       temp.scope, candidate.scope));
#endif
			if (!update) {
				update = temp.scope - candidate.scope;
				if (update > 0) {
					update = candidate.scope < daddr_scope ? 1 : -1;
				} else if (update < 0) {
					update = temp.scope < daddr_scope ? -1 : 1;
				}
				if (update < 0) {
#ifdef CONFIG_IPV6_ACONF_DEBUG_SADDR
					ADBG3((KERN_DEBUG
					       "ipv6_dev_get_saddr():    lose at Rule 2\n"));
#endif
					continue;
				}
			}

			/* Rule 3: Avoid deprecated address */
			if (ipv6_addrselect_preferred(addr_type) ||
			    !(ifp->flags & IFA_F_DEPRECATED))
				temp.flags |= IPV6_SADDRSELECT_PREFERRED;
#ifdef CONFIG_IPV6_ACONF_DEBUG_SADDR
			ADBG3((KERN_DEBUG
			       "ipv6_dev_get_saddr():    update=%d, Rule 3: %d vs %d\n",
			       update,
			       temp.flags&IPV6_SADDRSELECT_PREFERRED?1:0,
			       candidate.flags&IPV6_SADDRSELECT_PREFERRED?1:0));
#endif
			if (!update && (temp.flags^candidate.flags)&IPV6_SADDRSELECT_PREFERRED) {
				update = temp.flags&IPV6_SADDRSELECT_PREFERRED;
				if (!update) {
#ifdef CONFIG_IPV6_ACONF_DEBUG_SADDR
					ADBG3((KERN_DEBUG
					       "ipv6_dev_get_saddr():    lose at Rule 3\n"));
#endif
					continue;
				}
			}

			/* XXX: Rule 4: Prefer home address */

			/* Rule 5: Prefer outgoing interface */
			if (daddr_dev == NULL || daddr_dev == dev)
				temp.flags |= IPV6_SADDRSELECT_INTERFACE;
#ifdef CONFIG_IPV6_ACONF_DEBUG_SADDR
			ADBG3((KERN_DEBUG
			       "ipv6_dev_get_saddr():    update=%d, Rule 5: %d vs %d\n",
			       update,
			       temp.flags&IPV6_SADDRSELECT_INTERFACE?1:0,
			       candidate.flags&IPV6_SADDRSELECT_INTERFACE?1:0));
#endif
			if (!update && (temp.flags^candidate.flags)&IPV6_SADDRSELECT_INTERFACE) {
				update = temp.flags&IPV6_SADDRSELECT_INTERFACE;
				if (!update) {
#ifdef CONFIG_IPV6_ACONF_DEBUG_SADDR
					ADBG3((KERN_DEBUG
					       "ipv6_dev_get_saddr():    lose at Rule 5\n"));
#endif
					continue;
				}
			}

			/* Rule 6: Prefer matching label */
			if (ipv6_addrselect_label_lookup(&ifp->addr, dev->ifindex) == daddr_label)
				temp.flags |= IPV6_SADDRSELECT_LABEL;
#ifdef CONFIG_IPV6_ACONF_DEBUG_SADDR
			ADBG3((KERN_DEBUG
			       "ipv6_dev_get_saddr():    update=%d, Rule 6: %d vs %d\n",
				update,
				temp.flags&IPV6_SADDRSELECT_LABEL?1:0,
				candidate.flags&IPV6_SADDRSELECT_LABEL?1:0));
#endif
			if (!update && (temp.flags^candidate.flags)&IPV6_SADDRSELECT_LABEL) {
				update = temp.flags&IPV6_SADDRSELECT_LABEL;
				if (!update) {
#ifdef CONFIG_IPV6_ACONF_DEBUG_SADDR
					ADBG3((KERN_DEBUG
					       "ipv6_dev_get_saddr():    lose at Rule 6\n"));
#endif
					continue;
				}
			}

			/* Rule 7: Prefer public address */
#ifdef CONFIG_IPV6_PRIVACY
			if (!(ifp->flags & IFA_F_TEMPORARY))
				temp.flags |= IPV6_SADDRSELECT_PUBLIC;
#ifdef CONFIG_IPV6_ACONF_DEBUG_SADDR
			ADBG3((KERN_DEBUG
			       "ipv6_dev_get_saddr():    update=%d, Rule 7: %d vs %d\n",
				update,
				temp.flags&IPV6_SADDRSELECT_PUBLIC?1:0,
				candidate.flags&IPV6_SADDRSELECT_PUBLIC?1:0));
#endif
			if (!update && (temp.flags^candidate.flags)&IPV6_SADDRSELECT_PUBLIC) {
				update = (temp.flags^invpref)&IPV6_SADDRSELECT_PUBLIC;
				if (!update) {
#ifdef CONFIG_IPV6_ACONF_DEBUG_SADDR
					ADBG3((KERN_DEBUG
					       "ipv6_dev_get_saddr():    lose at Rule 7\n"));
#endif
					continue;
				}
			}
#endif

			/* Rule 8: Use longest matching prefix */
			temp.matchlen = ipv6_addr_diff(&ifp->addr, daddr);
#ifdef CONFIG_IPV6_ACONF_DEBUG_SADDR
			ADBG3((KERN_DEBUG
			       "ipv6_dev_get_saddr():    update=%d, Rule 8: %d vs %d\n",
				update,
				temp.matchlen,candidate.matchlen));
#endif
			if (!update) {
				update = temp.matchlen - candidate.matchlen;
				if (update < 0) {
#ifdef CONFIG_IPV6_ACONF_DEBUG_SADDR
					ADBG3((KERN_DEBUG
					       "ipv6_dev_get_saddr():    lose at Rule 8\n"));
#endif
					continue;
				}
			}

			/* Final Rule */
			if (!update && candidate.ifp) {
				ADBG3((KERN_DEBUG
				       "ipv6_dev_get_saddr():    candidate already exists\n"));
				continue;
			}
#ifdef CONFIG_IPV6_ACONF_DEBUG_SADDR
			ADBG3((KERN_DEBUG
			       "ipv6_dev_get_saddr():    update=%d, candidate updated; %s\n", 
			       update,
			       saddrbuf));
#endif

			/* update candidate */
			temp.ifp = ifp;
			in6_ifa_hold(ifp);
			if (candidate.ifp)
				in6_ifa_put(candidate.ifp);
			candidate = temp;
		}
		read_unlock_bh(&idev->lock);
	}
	read_unlock(&addrconf_lock);
	read_unlock(&dev_base_lock);

	if (candidate.ifp) {
		ipv6_addr_copy(saddr, &candidate.ifp->addr);
		in6_ifa_put(candidate.ifp);
		err = 0;
#ifdef CONFIG_IPV6_ACONF_DEBUG
		in6_ntop(saddr, saddrbuf);
		ADBG3((KERN_DEBUG "ipv6_dev_get_saddr(): %s is selected\n", saddrbuf));
#endif
	} else {
		err = -EADDRNOTAVAIL;
	}
	return err;
}

int ipv6_get_saddr(struct dst_entry *dst,
		   struct in6_addr *daddr, struct in6_addr *saddr,
		   int pref_privacy)
{
	return ipv6_dev_get_saddr(dst ? ((struct rt6_info *)dst)->rt6i_dev : NULL,
				  daddr, saddr, pref_privacy);
}

int ipv6_get_lladdr(struct net_device *dev, struct in6_addr *addr)
{
	struct inet6_dev *idev;
	int err = -EADDRNOTAVAIL;

	read_lock(&addrconf_lock);
	if ((idev = __in6_dev_get(dev)) != NULL) {
		struct inet6_ifaddr *ifp;

		read_lock_bh(&idev->lock);
		for (ifp=idev->addr_list; ifp; ifp=ifp->if_next) {
			if (ifp->scope == IFA_LINK && !(ifp->flags&IFA_F_TENTATIVE)) {
				ipv6_addr_copy(addr, &ifp->addr);
				err = 0;
				break;
			}
		}
		read_unlock_bh(&idev->lock);
	}
	read_unlock(&addrconf_lock);
	return err;
}

static int __ipv6_count_addresses(struct inet6_dev *idev)
{
	int cnt = 0;
	struct inet6_ifaddr *ifp;

	for (ifp=idev->addr_list; ifp; ifp=ifp->if_next)
		cnt++;
	return cnt;
}

int ipv6_count_addresses(struct inet6_dev *idev)
{
	int cnt;

	read_lock_bh(&idev->lock);
	cnt = __ipv6_count_addresses(idev);
	read_unlock_bh(&idev->lock);
	return cnt;
}

int ipv6_chk_addr(struct in6_addr *addr, struct net_device *dev)
{
	struct inet6_ifaddr * ifp;
	u8 hash = ipv6_addr_hash(addr);

	read_lock_bh(&addrconf_hash_lock);
	for(ifp = inet6_addr_lst[hash]; ifp; ifp=ifp->lst_next) {
		if (ipv6_addr_cmp(&ifp->addr, addr) == 0 &&
		    !(ifp->flags&IFA_F_TENTATIVE)) {
			if (dev == NULL || ifp->idev->dev == dev ||
			    !(ifp->scope&(IFA_LINK|IFA_HOST)))
				break;
		}
	}
	read_unlock_bh(&addrconf_hash_lock);
	return ifp != NULL;
}

static
int ipv6_chk_same_addr(const struct in6_addr *addr, struct net_device *dev)
{
	struct inet6_ifaddr * ifp;
	u8 hash = ipv6_addr_hash(addr);

	read_lock_bh(&addrconf_hash_lock);
	for(ifp = inet6_addr_lst[hash]; ifp; ifp=ifp->lst_next) {
		if (ipv6_addr_cmp(&ifp->addr, addr) == 0) {
			if (dev == NULL || ifp->idev->dev == dev)
				break;
		}
	}
	read_unlock_bh(&addrconf_hash_lock);
	return ifp != NULL;
}

struct inet6_ifaddr * ipv6_get_ifaddr(struct in6_addr *addr, struct net_device *dev)
{
	struct inet6_ifaddr * ifp;
	u8 hash = ipv6_addr_hash(addr);

	read_lock_bh(&addrconf_hash_lock);
	for(ifp = inet6_addr_lst[hash]; ifp; ifp=ifp->lst_next) {
		if (ipv6_addr_cmp(&ifp->addr, addr) == 0) {
			if (dev == NULL || ifp->idev->dev == dev) {
				if (!(ifp->scope&IFA_HOST)) {
					in6_ifa_hold(ifp);
					break;
				}
			}
		}
	}
	read_unlock_bh(&addrconf_hash_lock);

	return ifp;
}

/* address selection: policy label */
/* default policy table in rfc 3484 */
int ipv6_addrselect_add_label(int ifindex, struct in6_addr *addr, int plen, int label)
{
	struct ipv6_addrselect_label *p, *p_last;
#ifdef CONFIG_IPV6_ACONF_DEBUG_SADDR
	char abuf[128];

	in6_ntop(addr, abuf);
	ADBG3((KERN_DEBUG
	       "ipv6_addrselect_add_label(ifindex=%d, addr=%s, plen=%d, label=%d)\n",
	       ifindex, abuf, plen, label));
#endif

	/* Update label on duplicate addresses */
	write_lock_bh(&addrselect_label_lock);
	p_last = ipv6_addrselect_label_table;
	for (p = ipv6_addrselect_label_table;
	     p;
	     p = p->next) {
		if (!ipv6_prefix_cmp(addr, &p->addr, p->plen) && plen == p->plen) {
			p->ifindex = ifindex;
			p->label = label;
			write_unlock_bh(&addrselect_label_lock);
			return 0;
		}
		p_last = p;
	}

	p = kmalloc(sizeof(struct ipv6_addrselect_label), GFP_KERNEL);
	if (p == NULL) {
		ADBG1((KERN_WARNING
		       "ipv6_addrselect_add_label: malloc failed\n"));
		write_unlock_bh(&addrselect_label_lock);
		return -ENOMEM;
	}

	memset(p, 0, sizeof(struct ipv6_addrselect_label));

	if (!ipv6_addrselect_label_table) 
		ipv6_addrselect_label_table = p;
	else 
        	p_last->next = p;
	p->addr = *addr;
	p->plen = plen;
	p->ifindex = ifindex;
	p->label = label;
	write_unlock_bh(&addrselect_label_lock);
	return 0;
}

int ipv6_addrselect_del_label(struct in6_addr *addr, int plen)
{
	struct ipv6_addrselect_label *p, *p_last;
#ifdef CONFIG_IPV6_ACONF_DEBUG_SADDR
	char abuf[128];

	in6_ntop(addr, abuf);
	ADBG3((KERN_DEBUG
	       "ipv6_addrselect_del_label(addr=%s, plen=%d)\n",
	       abuf, plen));
#endif

	write_lock_bh(&addrselect_label_lock);
	p_last = ipv6_addrselect_label_table;
	for (p = ipv6_addrselect_label_table;
	     p;
	     p = p->next) {
		if (!ipv6_prefix_cmp(addr, &p->addr, p->plen) && plen == p->plen) {

 			if (ipv6_addrselect_label_table == p) {
 				ipv6_addrselect_label_table = p->next;
 			} else {
				p_last->next = p->next;
 			}
			kfree(p);
			write_unlock_bh(&addrselect_label_lock);
			return 0;
		}
		p_last = p;
	}
	write_unlock_bh(&addrselect_label_lock);
	return -ENOENT;
}

int addrconf_label_ioctl(unsigned int cmd, void *arg)
{
	struct in6_addrlabelreq req;
	int err = -EINVAL;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;
	switch(cmd) {
	case SIOCSDADDRLABEL:
		err = copy_from_user(&req, arg, sizeof(struct in6_addrlabelreq));
		if (err)
			return -EFAULT;
		err = ipv6_addrselect_add_label(req.ifindex, &req.addr, req.plen, req.label);
		break;

	case SIOCDDADDRLABEL:
		err = copy_from_user(&req, arg, sizeof(struct in6_addrlabelreq));
		if (err)
			return -EFAULT;
		err = ipv6_addrselect_del_label(&req.addr, req.plen);
		break;
	}
	return err;
}

static u32 ipv6_addrselect_label_lookup(const struct in6_addr *addr, int ifindex)
{
	struct ipv6_addrselect_label *p;
	int plen, matchlen = -1;
	u32 label = 0xffffffff;
#ifdef CONFIG_IPV6_ACONF_DEBUG_SADDR
	char buf[64];

	in6_ntop(addr, buf);
	ADBG3((KERN_DEBUG
	       "ipv6_addrselect_label_lookup(addr=%s, ifindex=%d)\n",
	       buf, ifindex));
#endif
	
	read_lock_bh(&addrselect_label_lock);
	for (p = ipv6_addrselect_label_table;
	     p;
	     p = p->next) {
		if (ifindex && p->ifindex && ifindex != p->ifindex)
			continue;
		plen = ipv6_addr_diff(addr, &p->addr);
		if (plen < p->plen || plen < matchlen)
			continue;
		matchlen = plen;
		label = p->label;
	}
#ifdef CONFIG_IPV6_ACONF_DEBUG_SADDR
	ADBG3((KERN_DEBUG
	       "ipv6_addrselect_label_lookup() = %u\n",
	       label));
#endif
	read_unlock_bh(&addrselect_label_lock);
	return label;
}


static struct ipv6_addrselect_label ipv6_addrselect_label_init_table[] = {
	/* prefix = ::1/128, label = 0 */
	{
		addr:	{{{ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 }}},
		plen:	128,
		label:	0,
	},
	/* prefix = ::/0, label = 1 */
	{
		label:	1,
	},
	/* prefix = 2002::/16, label = 2 */
	{
		addr:	{{{ 0x20,0x02,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }}},
		plen:	16,
		label:	2,
	},
	/* prefix = ::/96, label = 3 */
	{
		plen:	96,
		label:	3,
	},
	/* prefix = ::ffff:0:0/96, label = 4 */
	{
		addr:	{{{ 0,0,0,0,0,0,0,0,0,0xff,0xff,0,0,0,0 }}},
		plen:	96,
		label:	4,
	},
	/* sentinel */
	{
		label:	0xffffffff,
	}
};

void ipv6_addrselect_init(void)
{
	struct ipv6_addrselect_label **pnext, *p0;
	
	for (p0 = ipv6_addrselect_label_init_table,
	     pnext = &ipv6_addrselect_label_table;
	     p0->label != 0xffffffff;
	     p0++,
	     pnext = &(*pnext)->next) {
		*pnext = kmalloc(sizeof(**pnext), GFP_ATOMIC);
		if (*pnext == NULL) {
			printk(KERN_WARNING
				"%s(): failed to allocate memory\n",
				__FUNCTION__);
			break;
		}
		memcpy(*pnext, p0, sizeof(**pnext));
	}
};

#ifdef CONFIG_PROC_FS
static int ipv6_addrselect_label_proc_info(char *buffer, char **start, off_t offset,
					   int length)
{
	struct ipv6_addrselect_label *p;
	int len = 0;
	off_t pos = 0;
	off_t begin = 0;

	read_lock_bh(&addrselect_label_lock);
	for (p = ipv6_addrselect_label_table;
	     p;
	     p = p->next) {
		int i;
		for (i = 0; i < 16; i++) {
		    sprintf(buffer + len, "%02x", p->addr.s6_addr[i]);
		    len += 2;
		}

		len += sprintf(buffer + len,
			   " %02x %02x %02x\n",
			   p->plen,
			   p->ifindex,
			   p->label);
		pos = begin + len;
		if (pos < offset) {
		        len = 0;
		        begin = pos;
		}
		if(pos > offset + length) {
		        read_unlock_bh(&addrselect_label_lock);
		        goto done;
		}                
	}
	read_unlock_bh(&addrselect_label_lock);

 done:
	*start = buffer + (offset - begin);
	len -= (offset - begin);
	if (len > length)
		len = length;
	if (len < 0)
		len = 0;
	return len;
}
#endif  /* CONFIG_PROC_FS */


/* Gets referenced address, destroys ifaddr */

void addrconf_dad_failure(struct inet6_ifaddr *ifp)
{
#ifdef CONFIG_IPV6_ACONF_DEBUG
	ADBG3((KERN_DEBUG
	       "addrconf_dad_failure(ifp=%p): jiffies=%lu\n",
	       ifp, jiffies));
#endif
	if (net_ratelimit())
		ADBG1((KERN_WARNING 
		       "%s: duplicate address detected!\n", 
		       ifp->idev->dev->name));
	spin_lock_bh(&ifp->lock);
	if (ifp->flags&IFA_F_PERMANENT) {
		addrconf_del_timer(ifp);
		ifp->flags |= IFA_F_TENTATIVE;
		spin_unlock_bh(&ifp->lock);
		in6_ifa_put(ifp);
#ifdef CONFIG_IPV6_PRIVACY
	} else if (ifp->flags&IFA_F_TEMPORARY) {
		struct inet6_ifaddr *ifpub;
		ifpub = ifp->ifpub;
		if (ifpub) {
			in6_ifa_hold(ifpub);
			spin_unlock_bh(&ifp->lock);
			ipv6_create_tempaddr(ifpub, ifp);
			in6_ifa_put(ifpub);
		} else {
			spin_unlock_bh(&ifp->lock);
		}
		ipv6_del_addr(ifp);
#endif
	} else {
		spin_unlock_bh(&ifp->lock);
		ipv6_del_addr(ifp);
	}
}


/* Join to solicited addr multicast group. */

void addrconf_join_solict(struct net_device *dev, struct in6_addr *addr)
{
	struct in6_addr maddr;

	if (dev->flags&(IFF_LOOPBACK|IFF_NOARP))
		return;

	addrconf_addr_solict_mult(addr, &maddr);
	ipv6_dev_mc_inc(dev, &maddr);
}

void addrconf_leave_solict(struct net_device *dev, struct in6_addr *addr)
{
	struct in6_addr maddr;

	if (dev->flags&(IFF_LOOPBACK|IFF_NOARP))
		return;

	addrconf_addr_solict_mult(addr, &maddr);
	ipv6_dev_mc_dec(dev, &maddr);
}


static int ipv6_generate_eui64(u8 *eui, struct net_device *dev)
{
	switch (dev->type) {
	case ARPHRD_ETHER:
	case ARPHRD_FDDI:
	case ARPHRD_IEEE802_TR:
		if (dev->addr_len != ETH_ALEN)
			return -1;
		memcpy(eui, dev->dev_addr, 3);
		memcpy(eui + 5, dev->dev_addr+3, 3);
		eui[3] = 0xFF;
		eui[4] = 0xFE;
		eui[0] ^= 2;
		return 0;
	case ARPHRD_ARCNET:
		/* XXX: inherit EUI-64 fro mother interface -- yoshfuji */
		if (dev->addr_len != ARCNET_ALEN)
			return -1;
		memset(eui, 0, 7);
		eui[7] = *(u8*)dev->dev_addr;
		return 0;
	}
	return -1;
}

static int ipv6_inherit_eui64(u8 *eui, struct inet6_dev *idev)
{
	int err = -1;
	struct inet6_ifaddr *ifp;

	read_lock_bh(&idev->lock);
	for (ifp=idev->addr_list; ifp; ifp=ifp->if_next) {
		if (ifp->scope == IFA_LINK && !(ifp->flags&IFA_F_TENTATIVE)) {
			memcpy(eui, ifp->addr.s6_addr+8, 8);
			err = 0;
			break;
		}
	}
	read_unlock_bh(&idev->lock);
	return err;
}

#ifdef CONFIG_IPV6_PRIVACY
/* (re)generation of randomized interface identifier (RFC 3041 3.2, 3.5) */
static int __ipv6_regen_rndid(struct inet6_dev *idev)
{
	struct net_device *dev;
	u8 eui64[8];
	u8 digest[16];
	struct scatterlist sg[2];

	ADBG3((KERN_DEBUG
	       "__ipv6_regen_rndid(idev=%p)\n",
	       idev));

	if (!del_timer(&idev->regen_timer))
		in6_dev_hold(idev);

	dev = idev->dev;
#if 0
	if (!dev) {
		panic("__ipv6_regen_rndid(idev=%p): idev->dev=NULL\n");
	}
#endif
	if (ipv6_generate_eui64(eui64, dev)) {
		ADBG1((KERN_WARNING
		       "__ipv6_regen_rndid(idev=%p): cannot get EUI64 identifier; use random bytes.\n", 
		       idev));
		get_random_bytes(eui64, sizeof(eui64));
	}

	sg[0].page = virt_to_page(idev->entropy);
	sg[0].offset = ((long) idev->entropy & ~PAGE_MASK);
	sg[0].length = 8;
	sg[1].page = virt_to_page(eui64);
	sg[1].offset = ((long) eui64 & ~PAGE_MASK);
	sg[1].length = 8;
regen:
	spin_lock(&md5_tfm_lock);
	if (md5_tfm == NULL) {
		spin_unlock(&md5_tfm_lock);
		in6_dev_put(idev);
		return -1;
	}
	crypto_digest_init(md5_tfm);
	crypto_digest_update(md5_tfm, sg, 2);
	crypto_digest_final(md5_tfm, digest);
	spin_unlock(&md5_tfm_lock);
	
	memcpy(idev->rndid, &digest[0], 8);
	idev->rndid[0] &= ~0x02;
	memcpy(idev->entropy, &digest[8], 8);

	/*
	 * <draft-ietf-ipngwg-temp-addresses-v2-00.txt>:
	 * check if generated address is not inappropriate
	 * 
	 *  - Reserved subnet anycast (RFC 2526)
	 * 	11111101 11....11 1xxxxxxx
	 *  - ISATAP (draft-ietf-ngtrans-isatap-01.txt) 4.3
	 * 	00-00-5E-FE-xx-xx-xx-xx
	 *  - value 0
	 *  - XXX: already assigned to an address on the device
	 */
	if (idev->rndid[0] == 0xfd && 
	    (idev->rndid[1]&idev->rndid[2]&idev->rndid[3]&idev->rndid[4]&idev->rndid[5]&idev->rndid[6]) == 0xff &&
	    (idev->rndid[7]&0x80))
		goto regen;
	if ((idev->rndid[0]|idev->rndid[1]) == 0) {
		if (idev->rndid[2] == 0x5e && idev->rndid[3] == 0xfe)
			goto regen;
		if ((idev->rndid[2]|idev->rndid[3]|idev->rndid[4]|idev->rndid[5]|idev->rndid[6]|idev->rndid[7]) == 0x00)
			goto regen;
	}

	ADBG3((KERN_INFO
	       "__ipv6_regen_rndid(): new rndid for %s = %02x%02x:%02x%02x:%02x%02x:%02x%02x\n",
	       idev->dev->name,
	       idev->rndid[0], idev->rndid[1], idev->rndid[2], idev->rndid[3], 
	       idev->rndid[4], idev->rndid[5], idev->rndid[6], idev->rndid[7]));

	idev->regen_timer.expires = jiffies + 
					idev->cnf.temp_prefered_lft * HZ - 
					idev->cnf.regen_max_retry * idev->cnf.dad_transmits * idev->nd_parms->retrans_time - desync_factor;
	if (time_before(idev->regen_timer.expires, jiffies)) {
		idev->regen_timer.expires = 0;
		ADBG1((KERN_WARNING
		       "__ipv6_regen_rndid(): too short regeneration interval; timer diabled for %s.\n",
		       idev->dev->name));
		in6_dev_put(idev);
		return -1;
	}

	ADBG3((KERN_DEBUG
	       "__ipv6_regen_rndid(): next timer = %lu sec(s); temp_preferred_lft = %u\n",
	       (idev->regen_timer.expires - jiffies) / HZ,
	       idev->cnf.temp_prefered_lft));

	add_timer(&idev->regen_timer);
	return 0;
}

static void ipv6_regen_rndid(unsigned long data)
{
	struct inet6_dev *idev = (struct inet6_dev *) data;

	ADBG3((KERN_DEBUG
	       "ipv6_regen_rndid(idev=%p)\n", 
	       idev));

	read_lock_bh(&addrconf_lock);
	write_lock_bh(&idev->lock);
	if (!idev->dead)
		__ipv6_regen_rndid(idev);
	write_unlock_bh(&idev->lock);
	read_unlock_bh(&addrconf_lock);
}

static int __ipv6_try_regen_rndid(struct inet6_dev *idev, struct in6_addr *tmpaddr) {
	int ret = 0;

	ADBG3((KERN_DEBUG
	       "__ipv6_try_regen_rndid(idev=%p,tmpaddr=%p)\n",
	       idev, tmpaddr));

	if (tmpaddr && memcmp(idev->rndid, &tmpaddr->s6_addr[8], 8) == 0)
		ret = __ipv6_regen_rndid(idev);
	return ret;
}
#endif
		
/*
 *	Add prefix route.
 */

static void
addrconf_prefix_route(struct in6_addr *pfx, int plen, struct net_device *dev,
		      unsigned long expires, unsigned flags)
{
	struct in6_rtmsg rtmsg;

	memset(&rtmsg, 0, sizeof(rtmsg));
	ipv6_addr_copy(&rtmsg.rtmsg_dst, pfx);
	rtmsg.rtmsg_dst_len = plen;
	rtmsg.rtmsg_metric = IP6_RT_PRIO_ADDRCONF;
	rtmsg.rtmsg_ifindex = dev->ifindex;
	rtmsg.rtmsg_info = expires;
	rtmsg.rtmsg_flags = RTF_UP|flags;
	rtmsg.rtmsg_type = RTMSG_NEWROUTE;

	/* Prevent useless cloning on PtP SIT.
	   This thing is done here expecting that the whole
	   class of non-broadcast devices need not cloning.
	 */
	if (
#ifdef CONFIG_NET_IPIP_IPV6
	    dev->type == ARPHRD_TUNNEL
#else
	    dev->type == ARPHRD_SIT
#endif
	    && (dev->flags&IFF_POINTOPOINT))
		rtmsg.rtmsg_flags |= RTF_NONEXTHOP;

	ip6_route_add(&rtmsg, NULL, NULL);
}

/* Create "default" multicast route to the interface */

static void addrconf_add_mroute(struct net_device *dev)
{
	struct in6_rtmsg rtmsg;

	memset(&rtmsg, 0, sizeof(rtmsg));
	ipv6_addr_set(&rtmsg.rtmsg_dst,
		      htonl(0xFF000000), 0, 0, 0);
	rtmsg.rtmsg_dst_len = 8;
	rtmsg.rtmsg_metric = IP6_RT_PRIO_ADDRCONF;
	rtmsg.rtmsg_ifindex = dev->ifindex;
	rtmsg.rtmsg_flags = RTF_UP;
	rtmsg.rtmsg_type = RTMSG_NEWROUTE;
	ip6_route_add(&rtmsg, NULL, NULL);
}

static void sit_route_add(struct net_device *dev)
{
	struct in6_rtmsg rtmsg;

	memset(&rtmsg, 0, sizeof(rtmsg));

	rtmsg.rtmsg_type	= RTMSG_NEWROUTE;
	rtmsg.rtmsg_metric	= IP6_RT_PRIO_ADDRCONF;

	/* prefix length - 96 bits "::d.d.d.d" */
	rtmsg.rtmsg_dst_len	= 96;
	rtmsg.rtmsg_flags	= RTF_UP|RTF_NONEXTHOP;
	rtmsg.rtmsg_ifindex	= dev->ifindex;

	ip6_route_add(&rtmsg, NULL, NULL);
}

static void addrconf_add_lroute(struct net_device *dev)
{
	struct in6_addr addr;

	ipv6_addr_set(&addr,  htonl(0xFE800000), 0, 0, 0);
	addrconf_prefix_route(&addr, 64, dev, 0, 0);
}

static struct inet6_dev *addrconf_add_dev(struct net_device *dev)
{
	struct inet6_dev *idev;

	ASSERT_RTNL();

	if ((idev = ipv6_find_idev(dev)) == NULL)
		return NULL;

	/* Add default multicast route */
	addrconf_add_mroute(dev);

	/* Add link local route */
	addrconf_add_lroute(dev);
	return idev;
}

#if 0
#ifdef CONFIG_IPV6_PREFIXLIST
static int ipv6_add_prefix(struct inet6_dev *idev, struct in6_addr *addr,
		__u8 prefix_len, __u32 lifetime)
{
	struct in6_addr		prefix;
	struct list_head	*pos;
	struct inet6_prefix	*pfx;
#ifdef CONFIG_IPV6_PREFIXLIST_DEBUG
	char abuf[64];
#endif

	ipv6_addr_prefix(&prefix, addr, (int) prefix_len);

#ifdef CONFIG_IPV6_PREFIXLIST_DEBUG
	in6_ntop(&prefix, abuf);
#endif

	/* Check if the prefix already exists in the list */
	read_lock_bh(&idev->lock);
	write_lock_bh(&idev->prefix_lock);
	list_for_each(pos, &idev->prefix_list) {
		pfx = list_entry(pos, struct inet6_prefix, list);
		if (pfx->prefix_len == prefix_len && ipv6_addr_cmp(&pfx->prefix,
						&prefix) == 0) {
			pfx->lifetime = lifetime;
			pfx->timestamp = jiffies;
			write_unlock_bh(&idev->prefix_lock);
			read_unlock_bh(&idev->lock);
#ifdef CONFIG_IPV6_PREFIXLIST_DEBUG
			printk(KERN_INFO "%s: changing prefix %s/%d, lifetime = %d\n",
				__FUNCTION__, abuf, prefix_len, lifetime);
#endif
			return 0;
		}
	}

	/* New Prefix, allocate one and fill in */
	if ((pfx = kmalloc(sizeof(struct inet6_prefix), GFP_ATOMIC)) == NULL) {
		ADBG(("ipv6_add_prefix: malloc failed\n"));
		return -1;
	}
	INIT_LIST_HEAD(&pfx->list);
	pfx->lifetime = lifetime;
	pfx->timestamp = jiffies;
	pfx->prefix_len = prefix_len;
	ipv6_addr_copy(&pfx->prefix, &prefix);

	list_add(&pfx->list, idev->prefix_list.prev);	/* add to end of list */
#ifdef CONFIG_IPV6_PREFIXLIST_DEBUG
	printk(KERN_INFO "%s: adding prefix %s/%d, lifetime = %d\n",
		__FUNCTION__, abuf, prefix_len, lifetime);
#endif
	write_unlock_bh(&idev->prefix_lock);
	read_unlock_bh(&idev->lock);
	return 0;
}
#endif
#endif

void ipv6_wash_prefix(struct in6_addr *pfx, int plen)
{
	int b = plen&0x7;
	int o = (plen + 7)>>3;

	if (o < 16)
		memset(pfx->s6_addr + o, 0, 16 - o);
	if (b != 0)
		pfx->s6_addr[plen>>3] &= (0xFF<<(8-b));
}

#ifdef CONFIG_IPV6_PREFIXLIST
static int ipv6_add_prefix(struct inet6_dev *idev, struct prefix_info *pinfo)
{
	int ifindex;
	struct in6_addr	prefix;
	struct list_head *head;
	struct prefix_element *p;

	ipv6_addr_prefix(&prefix, &pinfo->prefix, (int)pinfo->prefix_len);

	/* Check if the prefix already exists in the list */
	read_lock_bh(&idev->lock);
	spin_lock_bh(&idev->prefix_lock);

	ifindex = idev->dev->ifindex;

	list_for_each(head, &idev->prefix_list) {
		p = list_entry(head, struct prefix_element, list);
		if (p->pinfo.prefix_len == pinfo->prefix_len
		    && ipv6_addr_cmp(&p->pinfo.prefix, &prefix) == 0) {
			p->pinfo.valid = pinfo->valid;
			p->timestamp = jiffies;
			spin_unlock_bh(&idev->prefix_lock);
			read_unlock_bh(&idev->lock);
			return 0;
		}
	}

	/* New Prefix, allocate one and fill in */
	if ((p = kmalloc(sizeof(struct prefix_element), GFP_ATOMIC)) == NULL) {
		ADBG(("ipv6_add_prefix: malloc failed\n"));
		spin_unlock_bh(&idev->prefix_lock);
		read_unlock_bh(&idev->lock);
		return -1;
	}

	memcpy(&p->pinfo, pinfo, sizeof(struct prefix_info));
	ipv6_wash_prefix(&p->pinfo.prefix, pinfo->prefix_len);
	p->timestamp = jiffies;

	idev->prefix_count++;
	list_add(&p->list, idev->prefix_list.prev);	/* add to end of list */

	spin_unlock_bh(&idev->prefix_lock);
	read_unlock_bh(&idev->lock);

	return 0;
}
#endif

void addrconf_prefix_rcv(struct net_device *dev, u8 *opt, int len)
{
	struct prefix_info *pinfo;
	__u32 valid_lft;
	__u32 prefered_lft;
	int addr_type;
	struct inet6_dev *in6_dev;

	pinfo = (struct prefix_info *) opt;
	
	if (len < sizeof(struct prefix_info)) {
		ADBG2((KERN_WARNING
		       "addrconf: prefix option too short\n"));
		return;
	}
	
	/*
	 *	Validation checks ([ADDRCONF], page 19)
	 */

	addr_type = ipv6_addr_type(&pinfo->prefix);

	if (addr_type & (IPV6_ADDR_MULTICAST|IPV6_ADDR_LINKLOCAL))
		return;

	valid_lft = ntohl(pinfo->valid);
	prefered_lft = ntohl(pinfo->prefered);
	ADBG3((KERN_INFO
	       "Valid Lifetime of Received Prefix = %d\n", valid_lft));
	ADBG3((KERN_INFO
	       "Prefered Lifetime of Received Prefix = %d\n", prefered_lft));

	if (prefered_lft > valid_lft) {
		if (net_ratelimit())
			ADBG2((KERN_WARNING "addrconf: prefix option has invalid lifetime\n"));
		return;
	}

	if (valid_lft >= 0x7FFFFFFF/HZ) {
		/* Avoid arithemtic overflow */
		if (net_ratelimit())
			ADBG1((KERN_DEBUG "addrconf: valid lifetime %u is too long; adjusted to %u.\n",
			       valid_lft, 0x7FFFFFFF/HZ-1));
		valid_lft = 0x7FFFFFFF/HZ-1;
		if (prefered_lft > valid_lft)
			prefered_lft = valid_lft;
	}

	in6_dev = in6_dev_get(dev);

	if (in6_dev == NULL) {
		if (net_ratelimit())
			ADBG1((KERN_DEBUG 
			       "addrconf: device %s not configured\n", dev->name));
		return;
	}

	/*
	 *	Two things going on here:
	 *	1) Add routes for on-link prefixes
	 *	2) Configure prefixes with the auto flag set
	 */

	if (pinfo->onlink) {
		struct rt6_info *rt;
		unsigned long rt_expires = jiffies + valid_lft + HZ;

		rt = rt6_lookup(&pinfo->prefix, NULL, dev->ifindex, 
					 RT6_LOOKUP_FLAG_STRICT|RT6_LOOKUP_FLAG_NOUSE);
	
		if (rt && ((rt->rt6i_flags & (RTF_GATEWAY | RTF_DEFAULT)) == 0)) {
			if (rt->rt6i_flags&RTF_EXPIRES) {
				if (valid_lft) {
					rt->rt6i_expires = rt_expires;
				} else {
					ip6_del_rt(rt, NULL, NULL);
					rt = NULL;
				}
			}
		} else if (valid_lft) {
			addrconf_prefix_route(&pinfo->prefix, pinfo->prefix_len,
					      dev, rt_expires, RTF_ADDRCONF|RTF_EXPIRES|RTF_PREFIX_RT);
		}
		if (rt)
			dst_release(&rt->u.dst);
#ifdef CONFIG_IPV6_PREFIXLIST
		/* Add this prefix to the list of prefixes on this interface */
		ipv6_add_prefix(in6_dev, pinfo);
#endif
	}

	/* Try to figure out our local address for this prefix */

	if (pinfo->autoconf && in6_dev->cnf.autoconf) {
		struct inet6_ifaddr *ifp = NULL;
#ifdef CONFIG_IPV6_PRIVACY
		struct inet6_ifaddr *ift = NULL;
#endif
		int flags;
		struct in6_addr addr;
		int update_lft = 0, create = 0;
		unsigned long now, stored_lft;

		if (pinfo->prefix_len != 64) {
			if (net_ratelimit())
				ADBG2((KERN_DEBUG
				       "IPv6 addrconf: prefix with wrong length %d\n", 
				       pinfo->prefix_len));
			goto autoconf_fail;
		}

		memcpy(&addr, &pinfo->prefix, 8);
		if (ipv6_generate_eui64(addr.s6_addr + 8, dev) &&
		    ipv6_inherit_eui64(addr.s6_addr + 8, in6_dev))
			goto autoconf_fail;

		ifp = ipv6_get_ifaddr(&addr, dev);

		if (!ifp) {
			int max_addresses = in6_dev->cnf.max_addresses;

			if (!valid_lft)
				goto autoconf_fail;

			if (!max_addresses ||
			    ipv6_count_addresses(in6_dev) < max_addresses)
				ifp = ipv6_add_addr(in6_dev, &addr, pinfo->prefix_len,
						    addr_type&IPV6_ADDR_SCOPE_MASK, 0);
			if (!ifp)
				goto autoconf_fail;

			addrconf_dad_start(ifp, RTF_ADDRCONF|RTF_PREFIX_RT);
			update_lft = 1;
			create = 1;
		}

		spin_lock(&ifp->lock);
		now = jiffies;
		if (ifp->valid_lft > (now - ifp->tstamp) / HZ)
			stored_lft = ifp->valid_lft - (now - ifp->tstamp) / HZ;
		else
			stored_lft = 0;
#define TWO_HOURS	7200
		if (!update_lft && stored_lft) {
			if (valid_lft > TWO_HOURS ||
			    valid_lft > stored_lft) {
				update_lft = 1;
			} else if (stored_lft <= TWO_HOURS
#if 0	/* this rule is logically redundant */
				   && valid_lft <= stored_lft
#endif
				   ) {
				update_lft = 0;
			} else {
				valid_lft = TWO_HOURS;
				if (valid_lft < prefered_lft)
					prefered_lft = valid_lft;
				update_lft = 1;
			}
		}
		if (update_lft) {
			ifp->valid_lft = valid_lft;
			ifp->prefered_lft = prefered_lft;
			ifp->tstamp = jiffies;
			flags = ifp->flags;
			ifp->flags &= ~IFA_F_DEPRECATED;
			spin_unlock(&ifp->lock);

			if (!(flags&IFA_F_TENTATIVE))
				ipv6_ifa_notify((flags&IFA_F_DEPRECATED) ?
						0 : RTM_NEWADDR, ifp);
		} else {
			spin_unlock(&ifp->lock);
		}
#ifdef CONFIG_IPV6_PRIVACY
		read_lock_bh(&in6_dev->lock);

		/* update all temporary addresses in the list */
		for (ift=in6_dev->tempaddr_list; ift; ift=ift->tmp_next) {
			/*
			 * When adjusting the lifetimes of an existing 
			 * temporary address, only lower the lifetimes.  
			 * Implementations must not increase the 
			 * lifetimes of an existing temporary address 
			 * when processing a Prefix Information Option.
			 */
			spin_lock(&ift->lock);
			flags = ift->flags;
			if (ift->valid_lft > valid_lft &&
			    ift->valid_lft - valid_lft > (jiffies - ift->tstamp) / HZ)
				ift->valid_lft = valid_lft + (jiffies - ift->tstamp) / HZ;
			if (ift->prefered_lft > prefered_lft &&
			    ift->prefered_lft - prefered_lft > (jiffies - ift->tstamp) / HZ)
				ift->prefered_lft = prefered_lft + (jiffies - ift->tstamp) / HZ;
			spin_unlock(&ift->lock);
			if (!(flags&IFA_F_TENTATIVE))
				ipv6_ifa_notify(0, ift);
		}

		if (create && in6_dev->cnf.use_tempaddr > 0) {
			/*
			 * When a new public address is created as described in [ADDRCONF],
			 * also create a new temporary address.
			 */
			read_unlock_bh(&in6_dev->lock);	
			ipv6_create_tempaddr(ifp, NULL);
		} else {
			read_unlock_bh(&in6_dev->lock);
		}
#endif
		addrconf_verify(0);
		in6_ifa_put(ifp);
	}
autoconf_fail:
	in6_dev_put(in6_dev);
}

/*
 *	Set destination address.
 *	Special case for SIT interfaces where we create a new "virtual"
 *	device.
 */
int addrconf_set_dstaddr(void *arg)
{
	struct in6_ifreq ireq;
	struct net_device *dev;
	int err = -EINVAL;

	rtnl_lock();

	err = -EFAULT;
	if (copy_from_user(&ireq, arg, sizeof(struct in6_ifreq)))
		goto err_exit;

	dev = __dev_get_by_index(ireq.ifr6_ifindex);

	err = -ENODEV;
	if (dev == NULL)
		goto err_exit;

#ifdef CONFIG_NET_IPIP_IPV6
	if (dev->type == ARPHRD_TUNNEL)
#else
	if (dev->type == ARPHRD_SIT)
#endif
	{
		struct ifreq ifr;
		mm_segment_t	oldfs;
		struct ip_tunnel_parm p;

		err = -EADDRNOTAVAIL;
		if (!(ipv6_addr_type(&ireq.ifr6_addr) & IPV6_ADDR_COMPATv4))
			goto err_exit;

		memset(&p, 0, sizeof(p));
		p.iph.daddr = ireq.ifr6_addr.s6_addr32[3];
		p.iph.saddr = 0;
		p.iph.version = 4;
		p.iph.ihl = 5;
		p.iph.protocol = IPPROTO_IPV6;
		p.iph.ttl = 64;
		ifr.ifr_ifru.ifru_data = (void*)&p;

		oldfs = get_fs(); set_fs(KERNEL_DS);
		err = dev->do_ioctl(dev, &ifr, SIOCADDTUNNEL);
		set_fs(oldfs);

		if (err == 0) {
			err = -ENOBUFS;
			if ((dev = __dev_get_by_name(p.name)) == NULL)
				goto err_exit;
			err = dev_open(dev);
		}
	}

err_exit:
	rtnl_unlock();
	return err;
}

/*
 *	Manual configuration of address on an interface
 */
static int inet6_addr_add(int ifindex, struct in6_addr *pfx, int plen)
{
	struct inet6_ifaddr *ifp;
	struct inet6_dev *idev;
	struct net_device *dev;
	int type, scope;
#ifdef CONFIG_IPV6_ACONF_DEBUG
	char abuf[128];

	in6_ntop(pfx, abuf);
	ADBG3((KERN_DEBUG
	       "inet6_addr_addr(ifindex=%d, pfx=%s, plen=%d)\n",
	       ifindex, abuf, plen));
#endif

	ASSERT_RTNL();
	
	if ((dev = __dev_get_by_index(ifindex)) == NULL)
		return -ENODEV;
	
	if (!(dev->flags&IFF_UP))
		return -ENETDOWN;

	if ((idev = addrconf_add_dev(dev)) == NULL)
		return -ENOBUFS;

	type = ipv6_addr_type(pfx);
	if (type & IPV6_ADDR_MULTICAST)
		return -EINVAL;

	scope = ipv6_addr_scope(pfx);

	ifp = ipv6_add_addr(idev, pfx, plen, scope, IFA_F_PERMANENT);
	if (!IS_ERR(ifp)) {
		addrconf_dad_start(ifp, 0);
		in6_ifa_put(ifp);
		return 0;
	}

	return PTR_ERR(ifp);
}

static int inet6_addr_del(int ifindex, struct in6_addr *pfx, int plen)
{
	struct inet6_ifaddr *ifp;
	struct inet6_dev *idev;
	struct net_device *dev;
	
	if ((dev = __dev_get_by_index(ifindex)) == NULL)
		return -ENODEV;

	if ((idev = __in6_dev_get(dev)) == NULL)
		return -ENXIO;

	read_lock_bh(&idev->lock);
	for (ifp = idev->addr_list; ifp; ifp=ifp->if_next) {
		if (ifp->prefix_len == plen &&
		    (!memcmp(pfx, &ifp->addr, sizeof(struct in6_addr)))) {
			in6_ifa_hold(ifp);
			read_unlock_bh(&idev->lock);
			
			ipv6_del_addr(ifp);

			/* If the last address is deleted administratively,
			   disable IPv6 on this interface.
			 */
			if (idev->addr_list == NULL)
				addrconf_ifdown(idev->dev, 1);
			return 0;
		}
	}
	read_unlock_bh(&idev->lock);
	return -EADDRNOTAVAIL;
}


int addrconf_add_ifaddr(void *arg)
{
	struct in6_ifreq ireq;
	int err;

#ifdef CONFIG_IPV6_ACONF_DEBUG
	ADBG3((KERN_DEBUG
	       "addrconf_add_ifaddr(arg=%p)\n",
	       arg));
#endif
	
	if (!capable(CAP_NET_ADMIN))
		return -EPERM;
	
	if (copy_from_user(&ireq, arg, sizeof(struct in6_ifreq)))
		return -EFAULT;

	rtnl_lock();
	err = inet6_addr_add(ireq.ifr6_ifindex, &ireq.ifr6_addr, ireq.ifr6_prefixlen);
	rtnl_unlock();
	return err;
}

int addrconf_del_ifaddr(void *arg)
{
	struct in6_ifreq ireq;
	int err;

#ifdef CONFIG_IPV6_ACONF_DEBUG
	ADBG3((KERN_DEBUG
	       "addrconf_add_ifaddr(arg=%p)\n",
	       arg));
#endif
	
	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	if (copy_from_user(&ireq, arg, sizeof(struct in6_ifreq)))
		return -EFAULT;

	rtnl_lock();
	err = inet6_addr_del(ireq.ifr6_ifindex, &ireq.ifr6_addr, ireq.ifr6_prefixlen);
	rtnl_unlock();
	return err;
}

static void __sit_add_v4_addr(struct inet6_dev *idev, struct in6_addr *addr, int plen, int scope)
{
	struct inet6_ifaddr *ifp = ipv6_add_addr(idev, addr, plen, scope, IFA_F_PERMANENT);
	if (!IS_ERR(ifp)) {
		spin_lock_bh(&ifp->lock);
		ifp->flags &= ~IFA_F_TENTATIVE;
		spin_unlock_bh(&ifp->lock);
		ipv6_ifa_notify(RTM_NEWADDR, ifp);
		in6_ifa_put(ifp);
	}
}

static void sit_add_v4_addrs(struct inet6_dev *idev)
{
	struct in6_addr addr;
	int plen;
	struct net_device *dev;
	int scope;
	__u32 dev_addr = *(__u32 *)idev->dev->dev_addr;

	ASSERT_RTNL();

	if (idev->dev->flags&IFF_POINTOPOINT) {
		ipv6_addr_set(&addr, 
			      htonl(0xfe800000), 0, 0, dev_addr);
		plen = addr.s6_addr32[3] ? 128 : 64;
		scope = IFA_LINK;
	} else {
		ipv6_addr_set(&addr, 
			      0, 0, 0, dev_addr);
		plen = 96;
		scope = IPV6_ADDR_COMPATv4;
	}

	if (addr.s6_addr32[3]) {
		__sit_add_v4_addr(idev, &addr, 128, scope);
		return;
	}

        for (dev = dev_base; dev != NULL; dev = dev->next) {
		struct in_device * in_dev = __in_dev_get(dev);
		if (in_dev && (dev->flags & IFF_UP)) {
			struct in_ifaddr * ifa;

			int flag = scope;

			for (ifa = in_dev->ifa_list; ifa; ifa = ifa->ifa_next) {
				addr.s6_addr32[3] = ifa->ifa_local;

				if (ifa->ifa_scope == RT_SCOPE_LINK)
					continue;
				if (ifa->ifa_scope >= RT_SCOPE_HOST) {
					if (idev->dev->flags&IFF_POINTOPOINT)
						continue;
					flag |= IFA_HOST;
				}
				__sit_add_v4_addr(idev, &addr, plen, flag);
			}
		}
        }
}

static struct inet6_dev * init_loopback(struct net_device *dev)
{
	struct inet6_dev  *idev;
	struct inet6_ifaddr * ifp;

	/* ::1 */

	ASSERT_RTNL();

	if ((idev = ipv6_find_idev(dev)) == NULL) {
		ADBG1((KERN_WARNING "init loopback: add_dev failed\n"));
		return NULL;
	}

	ipv6_mc_up(idev);

	ifp = ipv6_add_addr(idev, &in6addr_loopback, 128, IFA_HOST, IFA_F_PERMANENT);
	if (!IS_ERR(ifp)) {
		spin_lock_bh(&ifp->lock);
		ifp->flags &= ~IFA_F_TENTATIVE;
		spin_unlock_bh(&ifp->lock);
		ipv6_ifa_notify(RTM_NEWADDR, ifp);
		in6_ifa_put(ifp);
	}

	return idev;
}

static void addrconf_add_linklocal(struct inet6_dev *idev, struct in6_addr *addr)
{
	struct inet6_ifaddr * ifp;

	ifp = ipv6_add_addr(idev, addr, 64, IFA_LINK, IFA_F_PERMANENT);
	if (!IS_ERR(ifp)) {
		addrconf_dad_start(ifp, 0);
		in6_ifa_put(ifp);
	}
}

static struct inet6_dev * addrconf_dev_config(struct net_device *dev)
{
	struct in6_addr addr;
	struct inet6_dev    * idev;

	ASSERT_RTNL();

	switch(dev->type){
	case ARPHRD_ETHER:
	case ARPHRD_FDDI:
	case ARPHRD_IEEE802_TR:
	case ARPHRD_ARCNET:
		break;
	default:
		/* Alas, we support only Ethernet autoconfiguration. */
		return NULL;
	}

	idev = addrconf_add_dev(dev);
	if (idev == NULL)
		return NULL;

	ipv6_addr_set(&addr, htonl(0xfe800000), 0, 0, 0);

	if (ipv6_generate_eui64(addr.s6_addr + 8, dev) == 0)
		addrconf_add_linklocal(idev, &addr);

	return idev;
}

static struct inet6_dev * addrconf_sit_config(struct net_device *dev)
{
	struct inet6_dev *idev;

	ASSERT_RTNL();

	/* 
	 * Configure the tunnel with one of our IPv4 
	 * addresses... we should configure all of 
	 * our v4 addrs in the tunnel
	 */

	if ((idev = ipv6_find_idev(dev)) == NULL) {
		ADBG1((KERN_WARNING "init sit: add_dev failed\n"));
		return NULL;
	}
	ipv6_mc_up(idev);

	sit_add_v4_addrs(idev);

	if (dev->flags&IFF_POINTOPOINT) {
		addrconf_add_mroute(dev);
		addrconf_add_lroute(dev);
	} else
		sit_route_add(dev);

	return idev;
}


/**
 * addrconf_ipv6_tunnel_config - configure IPv6 tunnel device
 * @dev: tunnel device
 **/

static void addrconf_ipv6_tunnel_config(struct net_device *dev)
{
	struct inet6_dev *idev;

	ASSERT_RTNL();

	/* Assign inet6_dev structure to tunnel device */
	if ((idev = ipv6_find_idev(dev)) == NULL) {
		printk(KERN_DEBUG "init ipv6 tunnel: add_dev failed\n");
		return;
	}
}


int addrconf_notify(struct notifier_block *this, unsigned long event, 
		    void * data)
{
	struct net_device *dev = (struct net_device *) data;

	switch(event) {
	case NETDEV_UP:
	{
		struct inet6_dev *idev = NULL;
		switch(dev->type) {
#ifdef CONFIG_NET_IPIP_IPV6
		case ARPHRD_TUNNEL:
#else
		case ARPHRD_SIT:
#endif
		    {
			idev = addrconf_sit_config(dev);
			break;
		    }

		case ARPHRD_IPV6_IPV6_TUNNEL:
			addrconf_ipv6_tunnel_config(dev);
			break;

		case ARPHRD_LOOPBACK:
			idev = init_loopback(dev);
			break;

		default:
			idev = addrconf_dev_config(dev);
			break;
		};
		if (idev) {
			idev->stats.ipv6[0].Ip6LastChange = timeticks(jiffies);

			/* If the MTU changed during the interface down, when the 
			   interface up, the changed MTU must be reflected in the 
			   idev as well as routers.
			 */
			if (idev->cnf.mtu6 != dev->mtu && dev->mtu >= IPV6_MIN_MTU) {
				rt6_mtu_change(dev, dev->mtu);
				idev->cnf.mtu6 = dev->mtu;
			}
			/* If the changed mtu during down is lower than IPV6_MIN_MTU
			   stop IPv6 on this interface.
			 */
			if (dev->mtu < IPV6_MIN_MTU)
				addrconf_ifdown(dev, event != NETDEV_DOWN);
#ifdef CONFIG_IPV6_NODEINFO
			icmpv6_ni_join_nigroup_dev(dev);
#endif
		}
		break;
	}
	case NETDEV_CHANGEMTU:
	{
		struct inet6_dev *idev = __in6_dev_get(dev);
		if ( idev && dev->mtu >= IPV6_MIN_MTU) {
			rt6_mtu_change(dev, dev->mtu);
			idev->cnf.mtu6 = dev->mtu;
			break;
		}

		/* MTU falled under IPV6_MIN_MTU. Stop IPv6 on this interface. */
	}
	case NETDEV_DOWN:
	case NETDEV_UNREGISTER:
	{
		struct inet6_dev *idev = __in6_dev_get(dev);
		/*
		 *	Remove all addresses from this interface.
		 */
		if (idev)
			idev->stats.ipv6[0].Ip6LastChange = timeticks(jiffies);
		addrconf_ifdown(dev, event != NETDEV_DOWN);
		break;
	}
	case NETDEV_CHANGE:
		break;
	};

	return NOTIFY_OK;
}

static int addrconf_ifdown(struct net_device *dev, int how)
{
	struct inet6_dev *idev;
	struct inet6_ifaddr *ifa, **bifa;
#ifdef CONFIG_IPV6_PREFIXLIST
	struct list_head	*head, *next;
	struct prefix_element	*p;
#endif
	int i;

	ADBG3((KERN_DEBUG
	       "addrconf_ifdown(dev=%p(%s), how=%d)\n",
	       dev, dev->name, how));

	ASSERT_RTNL();

	rt6_ifdown(dev);
	neigh_ifdown(&nd_tbl, dev);

	idev = __in6_dev_get(dev);
	if (idev == NULL)
		return -ENODEV;

	/* Step 1: remove reference to ipv6 device from parent device.
	           Do not dev_put!
	 */
	if (how == 1) {
		write_lock_bh(&addrconf_lock);
		dev->ip6_ptr = NULL;
		idev->dead = 1;
		write_unlock_bh(&addrconf_lock);
	}

	/* Step 2: clear hash table */
	for (i=0; i<IN6_ADDR_HSIZE; i++) {
		bifa = &inet6_addr_lst[i];

		write_lock_bh(&addrconf_hash_lock);
		while ((ifa = *bifa) != NULL) {
			if (ifa->idev == idev) {
				*bifa = ifa->lst_next;
				ifa->lst_next = NULL;
				addrconf_del_timer(ifa);
				in6_ifa_put(ifa);
				continue;
			}
			bifa = &ifa->lst_next;
		}
		write_unlock_bh(&addrconf_hash_lock);
	}

	write_lock_bh(&idev->lock);

	/* Step 3: clear flags for stateless addrconf */
	if (how != 1)
		idev->if_flags &= ~(IF_RS_SENT|IF_RA_RCVD);

	/* Step 4: clear address list */
#ifdef CONFIG_IPV6_PRIVACY
	if (del_timer(&idev->regen_timer))
		in6_dev_put(idev);

	/* clear tempaddr list */
	while ((ifa = idev->tempaddr_list) != NULL) {
		idev->tempaddr_list = ifa->tmp_next;
		ifa->tmp_next = NULL;
		ifa->dead = 1;
		write_unlock_bh(&idev->lock);
		spin_lock_bh(&ifa->lock);
		if (ifa->ifpub) {
			in6_ifa_put(ifa->ifpub);
			ifa->ifpub = NULL;
		}
		spin_unlock_bh(&ifa->lock);
		in6_ifa_put(ifa);
		write_lock_bh(&idev->lock);
	}
#endif
	while ((ifa = idev->addr_list) != NULL) {
		idev->addr_list = ifa->if_next;
		ifa->if_next = NULL;
		ifa->dead = 1;
		addrconf_del_timer(ifa);
		write_unlock_bh(&idev->lock);

		ipv6_ifa_notify(RTM_DELADDR, ifa);
		in6_ifa_put(ifa);

		write_lock_bh(&idev->lock);
	}
	write_unlock_bh(&idev->lock);

	/* Step 5: Discard multicast list */

	if (how == 1)
		ipv6_mc_destroy_dev(idev);
	else
		ipv6_mc_down(idev);

#ifdef CONFIG_IPV6_PREFIXLIST
	/* Step 6: Free up Prefix List */
	spin_lock_bh(&idev->prefix_lock);
	list_for_each_safe(head, next, &idev->prefix_list) {
		p = list_entry(head, struct prefix_element, list);
#ifdef CONFIG_IPV6_PREFIXLIST_DEBUG
		{
			char abuf[64];
			struct inet6_prefix *pl = (struct inet6_prefix *)pfx;
			in6_ntop(&pl->prefix, abuf);
			printk(KERN_INFO "%s: deleting prefix %s/%d, lifetime = %d\n",
				__FUNCTION__, abuf, pl->prefix_len, pl->lifetime);
		}
#endif
		kfree(p);
	}
	INIT_LIST_HEAD(&idev->prefix_list);
	spin_unlock_bh(&idev->prefix_lock);

#endif
	/* Shot the device (if unregistered) */

	if (how == 1) {
		neigh_parms_release(&nd_tbl, idev->nd_parms);
#ifdef CONFIG_SYSCTL
		addrconf_sysctl_unregister(&idev->cnf);
#endif
		in6_dev_put(idev);
	}
	return 0;
}

static void addrconf_rs_timer(unsigned long data)
{
	struct inet6_ifaddr *ifp = (struct inet6_ifaddr *) data;

#ifdef CONFIG_IPV6_ACONF_DEBUG
	ADBG3((KERN_DEBUG
	       "addrconf_rs_timer(ifp=%p): jiffies=%lu\n", ifp, jiffies));
#endif

	if (ifp->idev->cnf.forwarding)
		goto out;

	if (ifp->idev->if_flags & IF_RA_RCVD) {
		/*
		 *	Announcement received after solicitation
		 *	was sent
		 */
		goto out;
	}

	spin_lock(&ifp->lock);
	if (ifp->probes++ < ifp->idev->cnf.rtr_solicits) {
		struct in6_addr all_routers;

		/* The wait after the last probe can be shorter */
		addrconf_mod_timer(ifp, AC_RS,
				   (ifp->probes == ifp->idev->cnf.rtr_solicits) ?
				   ifp->idev->cnf.rtr_solicit_delay :
				   ifp->idev->cnf.rtr_solicit_interval);
		spin_unlock(&ifp->lock);

		ipv6_addr_all_routers(&all_routers);

		ndisc_send_rs(ifp->idev->dev, &ifp->addr, &all_routers);
	} else {
		struct in6_rtmsg rtmsg;

		spin_unlock(&ifp->lock);

		ADBG1((KERN_DEBUG "%s: no IPv6 routers present\n",
		       ifp->idev->dev->name));

		memset(&rtmsg, 0, sizeof(struct in6_rtmsg));
		rtmsg.rtmsg_type = RTMSG_NEWROUTE;
		rtmsg.rtmsg_metric = IP6_RT_PRIO_ADDRCONF;
		rtmsg.rtmsg_flags = (RTF_ALLONLINK | RTF_ADDRCONF |
				     RTF_DEFAULT | RTF_UP);

		rtmsg.rtmsg_ifindex = ifp->idev->dev->ifindex;

		ip6_route_add(&rtmsg, NULL, NULL);
	}

out:
	in6_ifa_put(ifp);
}

/*
 *	Duplicate Address Detection
 */
static void addrconf_dad_start(struct inet6_ifaddr *ifp, int flags)
{
	struct net_device *dev;
	unsigned long rand_num;

	dev = ifp->idev->dev;

#ifdef CONFIG_IPV6_ACONF_DEBUG
	ADBG3((KERN_DEBUG
	       "addrconf_dad_start(ifp=%p): jiffies=%lu\n", ifp, jiffies));
#endif

	/*  Join his own solicit address  */
	addrconf_join_solict(dev, &ifp->addr);

	if (ifp->prefix_len != 128 && (ifp->flags&IFA_F_PERMANENT))
		addrconf_prefix_route(&ifp->addr, ifp->prefix_len, dev, 0, flags);

	get_random_bytes(&rand_num, sizeof(rand_num));
	rand_num %= (ifp->idev->cnf.rtr_solicit_delay ? : 1);

	spin_lock_bh(&ifp->lock);

	if (dev->flags&(IFF_NOARP|IFF_LOOPBACK) ||
	    !(ifp->flags&IFA_F_TENTATIVE)) {
		ifp->flags &= ~IFA_F_TENTATIVE;
		spin_unlock_bh(&ifp->lock);

		addrconf_dad_completed(ifp);
		return;
	}

	ifp->probes = ifp->idev->cnf.dad_transmits;
	addrconf_mod_timer(ifp, AC_DAD, rand_num);

	spin_unlock_bh(&ifp->lock);
}

static void addrconf_dad_timer(unsigned long data)
{
	struct inet6_ifaddr *ifp = (struct inet6_ifaddr *) data;
	struct in6_addr unspec;
	struct in6_addr mcaddr;

#ifdef CONFIG_IPV6_ACONF_DEBUG
	ADBG3((KERN_DEBUG
	       "addrconf_dad_timer(ifp=%p): jiffies=%lu\n", ifp, jiffies));
#endif

	spin_lock_bh(&ifp->lock);
	if (ifp->probes == 0) {
		/*
		 * DAD was successful
		 */

		ifp->flags &= ~IFA_F_TENTATIVE;
		spin_unlock_bh(&ifp->lock);

		addrconf_dad_completed(ifp);

		in6_ifa_put(ifp);
		return;
	}

	ifp->probes--;
	addrconf_mod_timer(ifp, AC_DAD, ifp->idev->nd_parms->retrans_time);
	spin_unlock_bh(&ifp->lock);

	/* send a neighbour solicitation for our addr */
	memset(&unspec, 0, sizeof(unspec));
	addrconf_addr_solict_mult(&ifp->addr, &mcaddr);
	ndisc_send_ns(ifp->idev->dev, NULL, &ifp->addr, &mcaddr, &unspec, 1);

	in6_ifa_put(ifp);
}

static void addrconf_dad_completed(struct inet6_ifaddr *ifp)
{
#ifdef CONFIG_IPV6_ACONF_DEBUG
	ADBG3((KERN_DEBUG
	       "addrconf_dad_completed(ifp=%p): jiffies=%lu\n", ifp, jiffies));
#endif

	/*
	 *	Configure the address for reception. Now it is valid.
	 */

	ipv6_ifa_notify(RTM_NEWADDR, ifp);

	/* If added prefix is link local and forwarding is off,
	   start sending router solicitations.
	 */

#ifdef CONFIG_IPV6_PRIVACY
	spin_lock_bh(&ifp->lock);
	if (ifp->flags&IFA_F_TEMPORARY) {
		if (!ifp->ifpub) {
			ADBG1((KERN_WARNING
			       "addrconf_dad_completed(): ifp->ifpub is NULL.\n"));
		} else {
			spin_lock(&ifp->ifpub->lock);
			ifp->ifpub->regen_count = 0;
			spin_unlock(&ifp->ifpub->lock);
		}
		spin_unlock_bh(&ifp->lock);
		return;
	}
	spin_unlock_bh(&ifp->lock);
#endif

	if (ifp->idev->cnf.forwarding == 0 &&
	    ifp->idev->cnf.accept_ra  &&
	    (ipv6_addr_type(&ifp->addr) & IPV6_ADDR_LINKLOCAL)) {
		struct in6_addr all_routers;

		ipv6_addr_all_routers(&all_routers);

		/*
		 *	If a host as already performed a random delay
		 *	[...] as part of DAD [...] there is no need
		 *	to delay again before sending the first RS
		 */
		ndisc_send_rs(ifp->idev->dev, &ifp->addr, &all_routers);

		spin_lock_bh(&ifp->lock);
		ifp->probes = 1;
		ifp->idev->if_flags |= IF_RS_SENT;
		addrconf_mod_timer(ifp, AC_RS, ifp->idev->cnf.rtr_solicit_interval);
		spin_unlock_bh(&ifp->lock);
	}

	if (ifp->idev->cnf.forwarding) {
		struct in6_addr addr;

		ipv6_addr_prefix(&addr, &ifp->addr, ifp->prefix_len);
		if (!ipv6_addr_any(&addr)) {
			ipv6_dev_ac_inc(ifp->idev->dev, &addr);
		}
	}
}

#ifdef CONFIG_PROC_FS
static int iface_proc_info(char *buffer, char **start, off_t offset,
			   int length)
{
	struct inet6_ifaddr *ifp;
	int i;
	int len = 0;
	off_t pos=0;
	off_t begin=0;

	for (i=0; i < IN6_ADDR_HSIZE; i++) {
		read_lock_bh(&addrconf_hash_lock);
		for (ifp=inet6_addr_lst[i]; ifp; ifp=ifp->lst_next) {
			int j;

			for (j=0; j<16; j++) {
				sprintf(buffer + len, "%02x",
					ifp->addr.s6_addr[j]);
				len += 2;
			}

			len += sprintf(buffer + len,
				       " %02x %02x %02x %02x %8s\n",
				       ifp->idev->dev->ifindex,
				       ifp->prefix_len,
				       ifp->scope,
				       ifp->flags,
				       ifp->idev->dev->name);
			pos=begin+len;
			if(pos<offset) {
				len=0;
				begin=pos;
			}
			if(pos>offset+length) {
				read_unlock_bh(&addrconf_hash_lock);
				goto done;
			}
		}
		read_unlock_bh(&addrconf_hash_lock);
	}

done:

	*start=buffer+(offset-begin);
	len-=(offset-begin);
	if(len>length)
		len=length;
	if(len<0)
		len=0;
	return len;
}

#ifdef CONFIG_IPV6_ACONF_DEBUG

#define print_ifa_flag(_ifp,_flag)	do {		\
	if ((_ifp)->flags&IFA_F_ ## _flag) {		\
		len += sprintf(buffer + len, "%s%s",	\
				fcount ? "," : "",	\
				# _flag);		\
		fcount++;				\
	}						\
} while(0);

static int iface_debug_proc_info(char *buffer, char **start, off_t offset,
			   int length)
{
	struct inet6_ifaddr *ifp;
	int i;
	int len = 0;
	off_t pos=0;
	off_t begin=0;

	for (i=0; i < IN6_ADDR_HSIZE; i++) {
		read_lock_bh(&addrconf_hash_lock);
		for (ifp=inet6_addr_lst[i]; ifp; ifp=ifp->lst_next) {
			int j;
			int fcount = 0;

			for (j=0; j<16; j++) {
				sprintf(buffer + len, "%02x",
					ifp->addr.s6_addr[j]);
				len += 2;
			}

			len += sprintf(buffer + len,
				       " %08lx %08x %08x "
				       "%8s ",
				       (jiffies - ifp->tstamp) / HZ,
				       ifp->prefered_lft, ifp->valid_lft,
				       ifp->idev->dev->name);
			print_ifa_flag(ifp,TENTATIVE);
			print_ifa_flag(ifp,PERMANENT);
			print_ifa_flag(ifp,TEMPORARY);
			print_ifa_flag(ifp,DEPRECATED);
			len += sprintf(buffer + len, "\n");
			pos=begin+len;
			if(pos<offset) {
				len=0;
				begin=pos;
			}
			if(pos>offset+length) {
				read_unlock_bh(&addrconf_hash_lock);
				goto done;
			}
		}
		read_unlock_bh(&addrconf_hash_lock);
	}

done:

	*start=buffer+(offset-begin);
	len-=(offset-begin);
	if(len>length)
		len=length;
	if(len<0)
		len=0;
	return len;
}
#endif

#endif	/* CONFIG_PROC_FS */

#ifdef CONFIG_IPV6_PREFIXLIST
static void ipv6_expire_prefixes(void)
{
	unsigned long		now = jiffies;
	struct net_device	*dev;
	struct inet6_dev	*idev;
	struct list_head	*head, *next;
	struct prefix_element	*p;
	unsigned long		age;

	read_lock(&dev_base_lock);
	for (dev = dev_base; dev; dev = dev->next) {
		if (!(idev = __in6_dev_get(dev))) {
			continue;
		}
		read_lock_bh(&idev->lock);
		spin_lock_bh(&idev->prefix_lock);
		if (list_empty(&idev->prefix_list)) {
			spin_unlock_bh(&idev->prefix_lock);
			read_unlock_bh(&idev->lock);
			continue;
		}
		list_for_each_safe(head, next, &idev->prefix_list) {
			p = list_entry(head, struct prefix_element, list);
			if (p->pinfo.valid != INFINITE) {
				age = (now - p->timestamp) / HZ;
				if (age > ntohl(p->pinfo.valid)) {
					idev->prefix_count--;
					list_del(&p->list);
					kfree(p);
				}
			}
		}
		spin_unlock_bh(&idev->prefix_lock);
		read_unlock_bh(&idev->lock);
	}
	read_unlock(&dev_base_lock);
}
#endif

/*
 *	Periodic address status verification
 */

void addrconf_verify(unsigned long foo)
{
	struct inet6_ifaddr *ifp;
	unsigned long now, next;
	int i;
#ifdef CONFIG_IPV6_PREFIXLIST
	struct net_device *dev;
	struct inet6_dev *idev;
	struct list_head *pos, *n;
	struct inet6_prefix *pfx;
#endif

#ifdef CONFIG_IPV6_ACONF_DEBUG
	char abuf[128];
#endif

	spin_lock_bh(&addrconf_verify_lock);
	now = jiffies;
	next = now + ADDR_CHECK_FREQUENCY;

	del_timer(&addr_chk_timer);

	for (i=0; i < IN6_ADDR_HSIZE; i++) {

restart:
		write_lock(&addrconf_hash_lock);
		for (ifp=inet6_addr_lst[i]; ifp; ifp=ifp->lst_next) {
			unsigned long age;
#ifdef CONFIG_IPV6_PRIVACY
			unsigned long regen_advance;
#endif

#ifdef CONFIG_IPV6_ACONF_DEBUG
			in6_ntop(&ifp->addr, abuf);
#endif

#ifdef CONFIG_IPV6_PRIVACY
			read_lock(&ifp->idev->lock);
			regen_advance = ifp->idev->cnf.regen_max_retry * ifp->idev->cnf.dad_transmits * ifp->idev->nd_parms->retrans_time / HZ;
			read_unlock(&ifp->idev->lock);
#endif

			if (ifp->flags & IFA_F_PERMANENT) {
#ifdef CONFIG_IPV6_ACONF_DEBUG
				ADBG3((KERN_DEBUG
				       "addrconf_verify(): ifp->addr=%s, IFA_F_PERMANENT\n",
				       abuf));
#endif
				continue;
			}

			spin_lock(&ifp->lock);
			age = (now - ifp->tstamp) / HZ;

			if (age >= ifp->valid_lft) {
				/* jiffies - ifp->tsamp > age >= ifp->valid_lft */
#ifdef CONFIG_IPV6_ACONF_DEBUG
				ADBG3((KERN_DEBUG
				       "addrconf_verify(): ifp->addr=%s, valid_lft(%u) <= age(%lu)\n",
				       abuf, ifp->valid_lft, age));
#endif
				spin_unlock(&ifp->lock);
				in6_ifa_hold(ifp);
				write_unlock(&addrconf_hash_lock);
				ipv6_del_addr(ifp);
				goto restart;
			} else if (age >= ifp->prefered_lft) {
				/* jiffies - ifp->tsamp > age >= ifp->prefered_lft */
				int deprecate = 0;

				if (!(ifp->flags&IFA_F_DEPRECATED)) {
					deprecate = 1;
					ifp->flags |= IFA_F_DEPRECATED;
				}

				if (time_before(ifp->tstamp + ifp->valid_lft * HZ, next))
					next = ifp->tstamp + ifp->valid_lft * HZ;

#ifdef CONFIG_IPV6_ACONF_DEBUG
				ADBG3((KERN_DEBUG
				       "addrconf_verify(): "
				       "ifp->addr=%s, "
				       "prefered_lft(%u) <= age(%lu) < valid_lft(%u), "
				       "deprecate=%d, next=%ld\n",
				       abuf, ifp->prefered_lft, age, ifp->valid_lft, deprecate, (long)next-(long)now));
#endif
				spin_unlock(&ifp->lock);

				if (deprecate) {
					in6_ifa_hold(ifp);
					write_unlock(&addrconf_hash_lock);

					ipv6_ifa_notify(0, ifp);
					in6_ifa_put(ifp);
					goto restart;
				}
#ifdef CONFIG_IPV6_PRIVACY
			} else if ((ifp->flags&IFA_F_TEMPORARY) &&
				   !(ifp->flags&IFA_F_TENTATIVE)) {
				if (age >= ifp->prefered_lft - regen_advance) {
					struct inet6_ifaddr *ifpub = ifp->ifpub;
#ifdef CONFIG_IPV6_ACONF_DEBUG
					ADBG3((KERN_DEBUG
					       "addrconf_verify(): "
					       "ifp->addr=%s, "
					       "prefered_lft(%u)-regen_advance(%lu) <= age(%lu) < prefered_lft(%u), "
					       "regen_count=%d, next=%ld\n",
					       abuf, ifp->prefered_lft, regen_advance, age, 
					       ifp->prefered_lft, ifp->regen_count, (long)next-(long)now));
#endif
					if (time_before(ifp->tstamp + ifp->prefered_lft * HZ, next))
						next = ifp->tstamp + ifp->prefered_lft * HZ;
					if (!ifp->regen_count && ifpub) {
						ifp->regen_count++;
						in6_ifa_hold(ifp);
						in6_ifa_hold(ifpub);
						spin_unlock(&ifp->lock);
						write_unlock(&addrconf_hash_lock);
						ipv6_create_tempaddr(ifpub, ifp);
						in6_ifa_put(ifpub);
						in6_ifa_put(ifp);
						goto restart;
					}
				} else if (time_before(ifp->tstamp + ifp->prefered_lft * HZ - regen_advance * HZ, next))
					next = ifp->tstamp + ifp->prefered_lft * HZ - regen_advance * HZ;
				spin_unlock(&ifp->lock);
#endif
			} else {
				/* ifp->prefered_lft <= ifp->valid_lft */
				if (time_before(ifp->tstamp + ifp->prefered_lft * HZ, next))
					next = ifp->tstamp + ifp->prefered_lft * HZ;
#ifdef CONFIG_IPV6_ACONF_DEBUG
 				ADBG3((KERN_DEBUG
				       "addrconf_verify(): "
				       "ifp->addr=%s, "
				       "age(%lu) <= prefered_lft(%u) (<= valid_lft(%u)), "
				       "next=%ld\n",
				       abuf, age, ifp->prefered_lft, ifp->valid_lft, (long)next-(long)now));
#endif
				spin_unlock(&ifp->lock);
			}
		}
		write_unlock(&addrconf_hash_lock);
	}
#ifdef CONFIG_IPV6_PREFIXLIST
	/*
	 * We need to expire prefixes even if no addresses are deleted in the
	 * loop above, since autoconfiguration may not be set in all router
	 * advertisements.
	 */
	read_lock(&dev_base_lock);
	for (dev = dev_base; dev; dev = dev->next) {
		unsigned long age;
		if (!(idev = __in6_dev_get(dev))) {
			continue;
		}
		read_lock_bh(&idev->lock);
		write_lock_bh(&idev->prefix_lock);
		if (list_empty(&idev->prefix_list)) {
			write_unlock_bh(&idev->prefix_lock);
			read_unlock_bh(&idev->lock);
			continue;
		}
		list_for_each_safe(pos, n, &idev->prefix_list) {
			pfx = list_entry(pos, struct inet6_prefix, list);
			if (pfx->lifetime != PINFO_VALID_LIFETIME_INFINITE) {
#ifdef CONFIG_IPV6_PREFIXLIST_DEBUG
#ifndef CONFIG_IPV6_ACONF_DEBUG
				char abuf[64];
#endif
				in6_ntop(&pfx->prefix, abuf);
#endif
				age = (now - pfx->timestamp) / HZ;
				if (age > pfx->lifetime) {
#ifdef CONFIG_IPV6_PREFIXLIST_DEBUG
					printk(KERN_INFO "%s: deleting prefix %04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x/%d, lifetime = %d\n",
					__FUNCTION__, abuf, pfx->prefix_len, pfx->lifetime);
#endif
					list_del(&pfx->list);
					kfree(pfx);
				} else if (time_before(pfx->timestamp + pfx->lifetime * HZ, next))
					next = pfx->timestamp + pfx->lifetime * HZ;
			}
		}
		write_unlock_bh(&idev->prefix_lock);
		read_unlock_bh(&idev->lock);
	}
	read_unlock(&dev_base_lock);
#endif

	if (time_before(now + HZ/2, jiffies)) {
		ADBG1((KERN_WARNING 
		       "addrconf_verify(): too slow; jiffies - now = %lu\n",
		       jiffies - now));
	}
#ifdef CONFIG_IPV6_ACONF_DEBUG
	/* don't fire too frequently */
	ADBG3((KERN_DEBUG
	       "addrconf_verify(): new timer: %lu (jiffies = %lu, after %ld sec)\n",
	       time_before(next, jiffies + HZ/2) ? jiffies + HZ/2 : next,
	       jiffies,
	       time_before(next, jiffies + HZ/2) ? HZ/2 : ((long)next - (long)jiffies)/HZ
	));
#endif
#ifdef CONFIG_IPV6_PREFIXLIST
	ipv6_expire_prefixes();
#endif
	addr_chk_timer.expires = time_before(next, jiffies + HZ/2) ? jiffies + HZ/2 : next;
	add_timer(&addr_chk_timer);
	spin_unlock_bh(&addrconf_verify_lock);
}

static int
inet6_rtm_deladdr(struct sk_buff *skb, struct nlmsghdr *nlh, void *arg)
{
	struct rtattr **rta = arg;
	struct ifaddrmsg *ifm = NLMSG_DATA(nlh);
	struct in6_addr *pfx;

	pfx = NULL;
	if (rta[IFA_ADDRESS-1]) {
		if (RTA_PAYLOAD(rta[IFA_ADDRESS-1]) < sizeof(*pfx))
			return -EINVAL;
		pfx = RTA_DATA(rta[IFA_ADDRESS-1]);
	}
	if (rta[IFA_LOCAL-1]) {
		if (pfx && memcmp(pfx, RTA_DATA(rta[IFA_LOCAL-1]), sizeof(*pfx)))
			return -EINVAL;
		pfx = RTA_DATA(rta[IFA_LOCAL-1]);
	}
	if (pfx == NULL)
		return -EINVAL;

	return inet6_addr_del(ifm->ifa_index, pfx, ifm->ifa_prefixlen);
}

static int
inet6_rtm_newaddr(struct sk_buff *skb, struct nlmsghdr *nlh, void *arg)
{
	struct rtattr  **rta = arg;
	struct ifaddrmsg *ifm = NLMSG_DATA(nlh);
	struct in6_addr *pfx;

	pfx = NULL;
	if (rta[IFA_ADDRESS-1]) {
		if (RTA_PAYLOAD(rta[IFA_ADDRESS-1]) < sizeof(*pfx))
			return -EINVAL;
		pfx = RTA_DATA(rta[IFA_ADDRESS-1]);
	}
	if (rta[IFA_LOCAL-1]) {
		if (pfx && memcmp(pfx, RTA_DATA(rta[IFA_LOCAL-1]), sizeof(*pfx)))
			return -EINVAL;
		pfx = RTA_DATA(rta[IFA_LOCAL-1]);
	}
	if (pfx == NULL)
		return -EINVAL;

	return inet6_addr_add(ifm->ifa_index, pfx, ifm->ifa_prefixlen);
}

static int inet6_fill_ifaddr(struct sk_buff *skb, struct inet6_ifaddr *ifa,
			     u32 pid, u32 seq, int event)
{
	struct ifaddrmsg *ifm;
	struct nlmsghdr  *nlh;
	struct ifa_cacheinfo ci;
	unsigned char	 *b = skb->tail;

	nlh = NLMSG_PUT(skb, pid, seq, event, sizeof(*ifm));
	if (pid) nlh->nlmsg_flags |= NLM_F_MULTI;
	ifm = NLMSG_DATA(nlh);
	ifm->ifa_family = AF_INET6;
	ifm->ifa_prefixlen = ifa->prefix_len;
	ifm->ifa_flags = ifa->flags;
	ifm->ifa_scope = RT_SCOPE_UNIVERSE;
	if (ifa->scope&IFA_HOST)
		ifm->ifa_scope = RT_SCOPE_HOST;
	else if (ifa->scope&IFA_LINK)
		ifm->ifa_scope = RT_SCOPE_LINK;
	else if (ifa->scope&IFA_SITE)
		ifm->ifa_scope = RT_SCOPE_SITE;
	ifm->ifa_index = ifa->idev->dev->ifindex;
	RTA_PUT(skb, IFA_ADDRESS, 16, &ifa->addr);
	if (!(ifa->flags&IFA_F_PERMANENT)) {
		ci.ifa_prefered = ifa->prefered_lft;
		ci.ifa_valid = ifa->valid_lft;
		if (ci.ifa_prefered != 0xFFFFFFFF) {
			long tval = (jiffies - ifa->tstamp)/HZ;
			ci.ifa_prefered -= tval;
			if (ci.ifa_valid != 0xFFFFFFFF)
				ci.ifa_valid -= tval;
		}
		RTA_PUT(skb, IFA_CACHEINFO, sizeof(ci), &ci);
	}
	nlh->nlmsg_len = skb->tail - b;
	return skb->len;

nlmsg_failure:
rtattr_failure:
	skb_trim(skb, b - skb->data);
	return -1;
}

static int inet6_dump_ifaddr(struct sk_buff *skb, struct netlink_callback *cb)
{
	int idx, ip_idx;
	int s_idx, s_ip_idx;
 	struct inet6_ifaddr *ifa;

	s_idx = cb->args[0];
	s_ip_idx = ip_idx = cb->args[1];

	for (idx=0; idx < IN6_ADDR_HSIZE; idx++) {
		if (idx < s_idx)
			continue;
		if (idx > s_idx)
			s_ip_idx = 0;
		read_lock_bh(&addrconf_hash_lock);
		for (ifa=inet6_addr_lst[idx], ip_idx = 0; ifa;
		     ifa = ifa->lst_next, ip_idx++) {
			if (ip_idx < s_ip_idx)
				continue;
			if (inet6_fill_ifaddr(skb, ifa, NETLINK_CB(cb->skb).pid,
					      cb->nlh->nlmsg_seq, RTM_NEWADDR) <= 0) {
				read_unlock_bh(&addrconf_hash_lock);
				goto done;
			}
		}
		read_unlock_bh(&addrconf_hash_lock);
	}
done:
	cb->args[0] = idx;
	cb->args[1] = ip_idx;

	return skb->len;
}

static void inet6_ifa_notify(int event, struct inet6_ifaddr *ifa)
{
	struct sk_buff *skb;
	int size = NLMSG_SPACE(sizeof(struct ifaddrmsg)+128);

	skb = alloc_skb(size, GFP_ATOMIC);
	if (!skb) {
		netlink_set_err(rtnl, 0, RTMGRP_IPV6_IFADDR, ENOBUFS);
		return;
	}
	if (inet6_fill_ifaddr(skb, ifa, 0, 0, event) < 0) {
		kfree_skb(skb);
		netlink_set_err(rtnl, 0, RTMGRP_IPV6_IFADDR, EINVAL);
		return;
	}
	NETLINK_CB(skb).dst_groups = RTMGRP_IPV6_IFADDR;
	netlink_broadcast(rtnl, skb, 0, RTMGRP_IPV6_IFADDR, GFP_ATOMIC);
}

static void inline ipv6_store_devconf(struct ipv6_devconf *cnf,
				__s32 *array, int bytes)
{
	memset(array, 0, bytes);
	array[DEVCONF_FORWARDING] = cnf->forwarding;
	array[DEVCONF_HOPLIMIT] = cnf->hop_limit;
	array[DEVCONF_MTU6] = cnf->mtu6;
	array[DEVCONF_ACCEPT_RA] = cnf->accept_ra;
	array[DEVCONF_ACCEPT_REDIRECTS] = cnf->accept_redirects;
	array[DEVCONF_AUTOCONF] = cnf->autoconf;
	array[DEVCONF_DAD_TRANSMITS] = cnf->dad_transmits;
	array[DEVCONF_RTR_SOLICITS] = cnf->rtr_solicits;
	array[DEVCONF_RTR_SOLICIT_INTERVAL] = cnf->rtr_solicit_interval;
	array[DEVCONF_RTR_SOLICIT_DELAY] = cnf->rtr_solicit_delay;
#ifdef CONFIG_IPV6_PRIVACY
	array[DEVCONF_USE_TEMPADDR] = cnf->use_tempaddr;
	array[DEVCONF_TEMP_VALID_LFT] = cnf->temp_valid_lft;
	array[DEVCONF_TEMP_PREFERED_LFT] = cnf->temp_prefered_lft;
	array[DEVCONF_REGEN_MAX_RETRY] = cnf->regen_max_retry;
	array[DEVCONF_MAX_DESYNC_FACTOR] = cnf->max_desync_factor;
#endif
	array[DEVCONF_MAX_ADDRESSES] = cnf->max_addresses;
#ifdef CONFIG_IPV6_NODEINFO
	array[DEVCONF_ACCEPT_NI] = cnf->accept_ni;
#endif
}

static int inet6_fill_ifinfo(struct sk_buff *skb, struct net_device *dev,
			    struct inet6_dev *idev,
			    int type, u32 pid, u32 seq)
{
	__s32			*array = NULL;
	struct ifinfomsg	*r;
	struct nlmsghdr 	*nlh;
	unsigned char		*b = skb->tail;
	struct rtattr		*subattr;

	nlh = NLMSG_PUT(skb, pid, seq, type, sizeof(*r));
	if (pid) nlh->nlmsg_flags |= NLM_F_MULTI;
	r = NLMSG_DATA(nlh);
	r->ifi_family = AF_INET6;
	r->ifi_type = dev->type;
	r->ifi_index = dev->ifindex;
	r->ifi_flags = dev->flags;
	r->ifi_change = 0;
	if (!netif_running(dev) || !netif_carrier_ok(dev))
		r->ifi_flags &= ~IFF_RUNNING;
	else
		r->ifi_flags |= IFF_RUNNING;

	RTA_PUT(skb, IFLA_IFNAME, strlen(dev->name)+1, dev->name);

	subattr = (struct rtattr*)skb->tail;

	RTA_PUT(skb, IFLA_PROTINFO, 0, NULL);

	/* return the device flags */
	RTA_PUT(skb, IFLA_INET6_FLAGS, sizeof(__u32), &idev->if_flags);

	/* return the device sysctl params */
	if ((array = kmalloc(DEVCONF_MAX * sizeof(*array), GFP_ATOMIC)) == NULL)
		goto rtattr_failure;
	ipv6_store_devconf(&idev->cnf, array, DEVCONF_MAX * sizeof(*array));
	RTA_PUT(skb, IFLA_INET6_CONF, DEVCONF_MAX * sizeof(*array), array);

	/* XXX - Statistics/MC not implemented */
	subattr->rta_len = skb->tail - (u8*)subattr;

	nlh->nlmsg_len = skb->tail - b;
	kfree(array);
	return skb->len;

nlmsg_failure:
rtattr_failure:
	if (array)
		kfree(array);
	skb_trim(skb, b - skb->data);
	return -1;
}

static int inet6_dump_ifinfo(struct sk_buff *skb, struct netlink_callback *cb)
{
	int idx, err;
	int s_idx = cb->args[0];
	struct net_device *dev;
	struct inet6_dev *idev;

	read_lock(&dev_base_lock);
	for (dev=dev_base, idx=0; dev; dev = dev->next, idx++) {
		if (idx < s_idx)
			continue;
		if ((idev = in6_dev_get(dev)) == NULL)
			continue;
		err = inet6_fill_ifinfo(skb, dev, idev, RTM_NEWLINK,
				NETLINK_CB(cb->skb).pid, cb->nlh->nlmsg_seq);
		in6_dev_put(idev);
		if (err <= 0)
			break;
	}
	read_unlock(&dev_base_lock);
	cb->args[0] = idx;

	return skb->len;
}

static struct rtnetlink_link inet6_rtnetlink_table[RTM_MAX-RTM_BASE+1] =
{
	{ NULL,			NULL,			},
	{ NULL,			NULL,			},
	{ NULL,			inet6_dump_ifinfo,	},
	{ NULL,			NULL,			},

	{ inet6_rtm_newaddr,	NULL,			},
	{ inet6_rtm_deladdr,	NULL,			},
	{ NULL,			inet6_dump_ifaddr,	},
	{ NULL,			NULL,			},

	{ inet6_rtm_newroute,	NULL,			},
	{ inet6_rtm_delroute,	NULL,			},
	{ inet6_rtm_getroute,	inet6_dump_fib,		},
	{ NULL,			NULL,			},
};

static void ipv6_ifa_notify(int event, struct inet6_ifaddr *ifp)
{
	inet6_ifa_notify(event ? : RTM_NEWADDR, ifp);

	switch (event) {
	case RTM_NEWADDR:
		ip6_rt_addr_add(&ifp->addr, ifp->idev->dev);
		break;
	case RTM_DELADDR:
		addrconf_leave_solict(ifp->idev->dev, &ifp->addr);
		if (ifp->idev->cnf.forwarding) {
			struct in6_addr addr;

			ipv6_addr_prefix(&addr, &ifp->addr, ifp->prefix_len);
			if (!ipv6_addr_any(&addr)) {
				ipv6_dev_ac_dec(ifp->idev->dev, &addr);
			}
		}
		if (!ipv6_chk_addr(&ifp->addr, NULL))
			ip6_rt_addr_del(&ifp->addr, ifp->idev->dev);
		break;
	}
}

#ifdef CONFIG_SYSCTL

static
int addrconf_sysctl_forward(ctl_table *ctl, int write, struct file * filp,
			   void *buffer, size_t *lenp)
{
	/* XXX: race */
	int *valp = ctl->data;
	int val = *valp;
	int ret;

	ret = proc_dointvec(ctl, write, filp, buffer, lenp);

	if (write && *valp != val && valp != &ipv6_devconf_dflt.forwarding) {
		if (valp == &ipv6_devconf.forwarding)
			addrconf_forward_change();
		else
			dev_forward_change((struct inet6_dev*)ctl->extra1);
		if (*valp) {
			ADBG3((KERN_DEBUG "Purge default routes(0)\n"));
			rt6_purge_dflt_routers(0);
		}
	}

        return ret;
}

static struct addrconf_sysctl_table
{
	struct ctl_table_header *sysctl_header;
	ctl_table addrconf_vars[19];
	ctl_table addrconf_dev[2];
	ctl_table addrconf_conf_dir[2];
	ctl_table addrconf_proto_dir[2];
	ctl_table addrconf_root_dir[2];
} addrconf_sysctl = {
	NULL,
        {{NET_IPV6_FORWARDING, "forwarding",
         &ipv6_devconf.forwarding, sizeof(int), 0644, NULL,
         &addrconf_sysctl_forward},

	{NET_IPV6_HOP_LIMIT, "hop_limit",
         &ipv6_devconf.hop_limit, sizeof(int), 0644, NULL,
         &proc_dointvec},

	{NET_IPV6_MTU, "mtu",
         &ipv6_devconf.mtu6, sizeof(int), 0644, NULL,
         &proc_dointvec},

	{NET_IPV6_ACCEPT_RA, "accept_ra",
         &ipv6_devconf.accept_ra, sizeof(int), 0644, NULL,
         &proc_dointvec},

	{NET_IPV6_ACCEPT_REDIRECTS, "accept_redirects",
         &ipv6_devconf.accept_redirects, sizeof(int), 0644, NULL,
         &proc_dointvec},

	{NET_IPV6_AUTOCONF, "autoconf",
         &ipv6_devconf.autoconf, sizeof(int), 0644, NULL,
         &proc_dointvec},

	{NET_IPV6_DAD_TRANSMITS, "dad_transmits",
         &ipv6_devconf.dad_transmits, sizeof(int), 0644, NULL,
         &proc_dointvec},

	{NET_IPV6_RTR_SOLICITS, "router_solicitations",
         &ipv6_devconf.rtr_solicits, sizeof(int), 0644, NULL,
         &proc_dointvec},

	{NET_IPV6_RTR_SOLICIT_INTERVAL, "router_solicitation_interval",
         &ipv6_devconf.rtr_solicit_interval, sizeof(int), 0644, NULL,
         &proc_dointvec_jiffies},

	{NET_IPV6_RTR_SOLICIT_DELAY, "router_solicitation_delay",
         &ipv6_devconf.rtr_solicit_delay, sizeof(int), 0644, NULL,
         &proc_dointvec_jiffies},

#ifdef CONFIG_IPV6_NODEINFO
	{NET_IPV6_ACCEPT_NI, "accept_ni",
         &ipv6_devconf.accept_ni, sizeof(int), 0644, NULL,
         &proc_dointvec},
#endif

#ifdef CONFIG_IPV6_PRIVACY
	{NET_IPV6_USE_TEMPADDR, "use_tempaddr",
	 &ipv6_devconf.use_tempaddr, sizeof(int), 0644, NULL,
	 &proc_dointvec},

	{NET_IPV6_TEMP_VALID_LFT, "temp_valid_lft",
	 &ipv6_devconf.temp_valid_lft, sizeof(int), 0644, NULL,
	 &proc_dointvec},

	{NET_IPV6_TEMP_PREFERED_LFT, "temp_prefered_lft",
	 &ipv6_devconf.temp_prefered_lft, sizeof(int), 0644, NULL,
	 &proc_dointvec},

	{NET_IPV6_REGEN_MAX_RETRY, "regen_max_retry",
	 &ipv6_devconf.regen_max_retry, sizeof(int), 0644, NULL,
	 &proc_dointvec},

	{NET_IPV6_MAX_DESYNC_FACTOR, "max_desync_factor",
	 &ipv6_devconf.max_desync_factor, sizeof(int), 0644, NULL,
	 &proc_dointvec},
#endif
	{NET_IPV6_MAX_ADDRESSES, "max_addresses",
	 &ipv6_devconf.max_addresses, sizeof(int), 0644, NULL,
	 &proc_dointvec},

	{0}},

	{{NET_PROTO_CONF_ALL, "all", NULL, 0, 0555, addrconf_sysctl.addrconf_vars},{0}},
	{{NET_IPV6_CONF, "conf", NULL, 0, 0555, addrconf_sysctl.addrconf_dev},{0}},
	{{NET_IPV6, "ipv6", NULL, 0, 0555, addrconf_sysctl.addrconf_conf_dir},{0}},
	{{CTL_NET, "net", NULL, 0, 0555, addrconf_sysctl.addrconf_proto_dir},{0}}
};

static void addrconf_sysctl_register(struct inet6_dev *idev, struct ipv6_devconf *p)
{
	int i;
	struct net_device *dev = idev ? idev->dev : NULL;
	struct addrconf_sysctl_table *t;

	t = kmalloc(sizeof(*t), GFP_KERNEL);
	if (t == NULL)
		return;
	memcpy(t, &addrconf_sysctl, sizeof(*t));
	for (i=0; t->addrconf_vars[i].ctl_name; i++) {
		t->addrconf_vars[i].data += (char*)p - (char*)&ipv6_devconf;
		t->addrconf_vars[i].de = NULL;
		t->addrconf_vars[i].extra1 = idev; /* embedded; no ref */
	}
	if (dev) {
		t->addrconf_dev[0].procname = dev->name;
		t->addrconf_dev[0].ctl_name = dev->ifindex;
	} else {
		t->addrconf_dev[0].procname = "default";
		t->addrconf_dev[0].ctl_name = NET_PROTO_CONF_DEFAULT;
	}
	t->addrconf_dev[0].child = t->addrconf_vars;
	t->addrconf_dev[0].de = NULL;
	t->addrconf_conf_dir[0].child = t->addrconf_dev;
	t->addrconf_conf_dir[0].de = NULL;
	t->addrconf_proto_dir[0].child = t->addrconf_conf_dir;
	t->addrconf_proto_dir[0].de = NULL;
	t->addrconf_root_dir[0].child = t->addrconf_proto_dir;
	t->addrconf_root_dir[0].de = NULL;

	t->sysctl_header = register_sysctl_table(t->addrconf_root_dir, 0);
	if (t->sysctl_header == NULL)
		kfree(t);
	else
		p->sysctl = t;
}

static void addrconf_sysctl_unregister(struct ipv6_devconf *p)
{
	if (p->sysctl) {
		struct addrconf_sysctl_table *t = p->sysctl;
		p->sysctl = NULL;
		unregister_sysctl_table(t->sysctl_header);
		kfree(t);
	}
}


#endif

/*
 *      Device notifier
 */

int register_inet6addr_notifier(struct notifier_block *nb)
{
        return notifier_chain_register(&inet6addr_chain, nb);
}

int unregister_inet6addr_notifier(struct notifier_block *nb)
{
        return notifier_chain_unregister(&inet6addr_chain,nb);
}

/*
 *	Init / cleanup code
 */

void __init addrconf_init(void)
{
#ifdef CONFIG_IPV6_PRIVACY
	unsigned long regen_advance;
#endif
#ifdef MODULE
	struct net_device *dev;
#endif

#ifdef CONFIG_IPV6_PRIVACY
	struct crypto_tfm *tfm = crypto_alloc_tfm("md5", 0);
	if (tfm != NULL) {
		spin_lock(&md5_tfm_lock);
		if (md5_tfm == NULL) {
			md5_tfm = tfm;
			spin_unlock(&md5_tfm_lock);
		} else {
			spin_unlock(&md5_tfm_lock);
			crypto_free_tfm(tfm);
		}
	} else {
		printk(KERN_WARNING
			"failed to load transform for md5\n");
	}

	regen_advance = ipv6_devconf.regen_max_retry * ipv6_devconf.dad_transmits * HZ; /* XXX */
	get_random_bytes(&desync_factor, sizeof(desync_factor));
	desync_factor %= MAX_DESYNC_FACTOR * HZ < TEMP_VALID_LIFETIME * HZ - regen_advance ? 
				MAX_DESYNC_FACTOR * HZ : TEMP_VALID_LIFETIME * HZ - regen_advance;
#endif

#ifdef MODULE
	/* This takes sense only during module load. */
	rtnl_lock();
	for (dev = dev_base; dev; dev = dev->next) {
		if (!(dev->flags&IFF_UP))
			continue;

		switch (dev->type) {
		case ARPHRD_LOOPBACK:	
			init_loopback(dev);
			break;
		case ARPHRD_ETHER:
		case ARPHRD_FDDI:
		case ARPHRD_IEEE802_TR:	
		case ARPHRD_ARCNET:
			addrconf_dev_config(dev);
			break;
		default:;
			/* Ignore all other */
		}
	}
	rtnl_unlock();
#endif

#ifdef CONFIG_PROC_FS
	proc_net_create("if_inet6", 0, iface_proc_info);
	proc_net_create("addrselect_label_table", 0, ipv6_addrselect_label_proc_info);
#ifdef CONFIG_IPV6_ACONF_DEBUG
	proc_net_create("if_inet6_debug", 0, iface_debug_proc_info);
#endif
#endif
	
	addrconf_verify(0);
	rtnetlink_links[PF_INET6] = inet6_rtnetlink_table;
#ifdef CONFIG_SYSCTL
	addrconf_sysctl.sysctl_header =
		register_sysctl_table(addrconf_sysctl.addrconf_root_dir, 0);
	addrconf_sysctl_register(NULL, &ipv6_devconf_dflt);
#endif
        ipv6_addrselect_init();
}

#ifdef MODULE
void addrconf_cleanup(void)
{
 	struct net_device *dev;
 	struct inet6_dev *idev;
 	struct inet6_ifaddr *ifa;
	int i;

	rtnetlink_links[PF_INET6] = NULL;
#ifdef CONFIG_SYSCTL
	addrconf_sysctl_unregister(&ipv6_devconf_dflt);
	addrconf_sysctl_unregister(&ipv6_devconf);
#endif 

	rtnl_lock();

	/*
	 *	clean dev list.
	 */

	for (dev=dev_base; dev; dev=dev->next) {
		if ((idev = __in6_dev_get(dev)) == NULL)
			continue;
		addrconf_ifdown(dev, 1);
	}

	/*
	 *	Check hash table.
	 */

	write_lock_bh(&addrconf_hash_lock);
	for (i=0; i < IN6_ADDR_HSIZE; i++) {
		for (ifa=inet6_addr_lst[i]; ifa; ) {
			struct inet6_ifaddr *bifa;

			bifa = ifa;
			ifa = ifa->lst_next;
			ADBG1((KERN_DEBUG 
			       "bug: IPv6 address leakage detected: ifa=%p\n", 
			       bifa));
			/* Do not free it; something is wrong.
			   Now we can investigate it with debugger.
			 */
		}
	}
	write_unlock_bh(&addrconf_hash_lock);

	del_timer(&addr_chk_timer);

	rtnl_unlock();

#ifdef CONFIG_IPV6_PRIVACY
	spin_unlock(&md5_tfm_lock);
	tfm = md5_tfm;
	md5_tfm = NULL;
	spin_unlock(&md5_tfm_lock);
	if (tfm != NULL)
		crypto_free_tfm(tfm);
#endif

#ifdef CONFIG_PROC_FS
#ifdef CONFIG_IPV6_ACONF_DEBUG
	proc_net_remove("if_inet6_debug");
#endif
	proc_net_remove("if_inet6");
#endif
}
#endif	/* MODULE */
