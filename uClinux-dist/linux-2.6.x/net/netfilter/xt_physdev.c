/* Kernel module to match the bridge port in and
 * out device for IP packets coming into contact with a bridge. */

/* (C) 2001-2003 Bart De Schuymer <bdschuym@pandora.be>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/netfilter_bridge.h>
#include <linux/netfilter/xt_physdev.h>
#include <linux/netfilter/x_tables.h>
#include <linux/etherdevice.h>
#include <net/dst.h>
#include <net/neighbour.h>
#include "../bridge/br_private.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bart De Schuymer <bdschuym@pandora.be>");
MODULE_DESCRIPTION("Xtables: Bridge physical device match");
MODULE_ALIAS("ipt_physdev");
MODULE_ALIAS("ip6t_physdev");

static inline struct nf_bridge_info *nf_bridge_alloc(struct sk_buff *skb)
{
	skb->nf_bridge = kzalloc(sizeof(struct nf_bridge_info), GFP_ATOMIC);
	if (likely(skb->nf_bridge))
		atomic_set(&(skb->nf_bridge->use), 1);

	return skb->nf_bridge;
}

static void get_outdev(const struct sk_buff* skb, const struct net_device *out)
{
	struct neighbour *neigh;
	struct nf_bridge_info *nf_bridge;
	const unsigned char *dest;
	struct net_bridge_fdb_entry *fdb;

	nf_bridge = skb->nf_bridge;
	if ((nf_bridge && nf_bridge->physoutdev) ||
	    !out || out->hard_start_xmit != br_dev_xmit || !br_fdb_get_hook)
		return;

	if (!nf_bridge && !(nf_bridge = nf_bridge_alloc(skb)))
		return;
	nf_bridge->physoutdev = out; /* so that --physdev-is-out matches */

	neigh = skb->dst->neighbour;
	if (!neigh || neigh_event_send(neigh, NULL))
		return;
	dest = neigh->ha;
	if (!is_multicast_ether_addr(dest) &&
	    (fdb = br_fdb_get_hook(netdev_priv(out), dest)) != NULL) {
		nf_bridge->physoutdev = fdb->dst->dev;
		br_fdb_put_hook(fdb);
	}
}

static bool
physdev_mt(const struct sk_buff *skb, const struct net_device *in,
           const struct net_device *out, const struct xt_match *match,
           const void *matchinfo, int offset, unsigned int protoff,
           bool *hotdrop)
{
	int i;
	static const char nulldevname[IFNAMSIZ];
	const struct xt_physdev_info *info = matchinfo;
	bool ret;
	const char *indev, *outdev;
	const struct nf_bridge_info *nf_bridge;

	/* Not a bridged IP packet or no info available yet:
	 * LOCAL_OUT/mangle and LOCAL_OUT/nat don't know if
	 * the destination device will be a bridge. */
	if (!(nf_bridge = skb->nf_bridge)) {
		/* Return MATCH if the invert flags of the used options are on */
		if ((info->bitmask & XT_PHYSDEV_OP_BRIDGED) &&
		    !(info->invert & XT_PHYSDEV_OP_BRIDGED))
			return false;
		if ((info->bitmask & XT_PHYSDEV_OP_ISIN) &&
		    !(info->invert & XT_PHYSDEV_OP_ISIN))
			return false;
		if ((info->bitmask & XT_PHYSDEV_OP_IN) &&
		    !(info->invert & XT_PHYSDEV_OP_IN))
			return false;
		goto match_outdev;
	}

	/* This only makes sense in the FORWARD and POSTROUTING chains */
	if ((info->bitmask & XT_PHYSDEV_OP_BRIDGED) &&
	    (!!(nf_bridge->mask & BRNF_BRIDGED) ^
	    !(info->invert & XT_PHYSDEV_OP_BRIDGED)))
		return false;

	if (info->bitmask & XT_PHYSDEV_OP_ISIN &&
	    (!nf_bridge->physindev ^ !!(info->invert & XT_PHYSDEV_OP_ISIN)))
		return false;

	if (!(info->bitmask & XT_PHYSDEV_OP_IN))
		goto match_outdev;
	indev = nf_bridge->physindev ? nf_bridge->physindev->name : nulldevname;
	for (i = 0, ret = false; i < IFNAMSIZ/sizeof(unsigned int); i++) {
		ret |= (((const unsigned int *)indev)[i]
			^ ((const unsigned int *)info->physindev)[i])
			& ((const unsigned int *)info->in_mask)[i];
	}

	if (!ret ^ !(info->invert & XT_PHYSDEV_OP_IN))
		return false;

match_outdev:
	if (!(info->bitmask & (XT_PHYSDEV_OP_ISOUT | XT_PHYSDEV_OP_OUT)))
		return true;
	get_outdev(skb, out);
	if (!(nf_bridge = skb->nf_bridge)) {
		if ((info->bitmask & XT_PHYSDEV_OP_ISOUT) &&
		    !(info->invert & XT_PHYSDEV_OP_ISOUT))
			return false;
		if ((info->bitmask & XT_PHYSDEV_OP_OUT) &&
		    !(info->invert & XT_PHYSDEV_OP_OUT))
			return false;
		return true;
	}

	if (info->bitmask & XT_PHYSDEV_OP_ISOUT &&
	    (!nf_bridge->physoutdev ^ !!(info->invert & XT_PHYSDEV_OP_ISOUT)))
		return false;

	if (!(info->bitmask & XT_PHYSDEV_OP_OUT))
		return true;
	outdev = nf_bridge->physoutdev ?
		 nf_bridge->physoutdev->name : nulldevname;
	for (i = 0, ret = false; i < IFNAMSIZ/sizeof(unsigned int); i++) {
		ret |= (((const unsigned int *)outdev)[i]
			^ ((const unsigned int *)info->physoutdev)[i])
			& ((const unsigned int *)info->out_mask)[i];
	}

	return ret ^ !(info->invert & XT_PHYSDEV_OP_OUT);
}

static bool
physdev_mt_check(const char *tablename, const void *ip,
                 const struct xt_match *match, void *matchinfo,
                 unsigned int hook_mask)
{
	const struct xt_physdev_info *info = matchinfo;

	if (!(info->bitmask & XT_PHYSDEV_OP_MASK) ||
	    info->bitmask & ~XT_PHYSDEV_OP_MASK)
		return false;
	return true;
}

static struct xt_match physdev_mt_reg[] __read_mostly = {
	{
		.name		= "physdev",
		.family		= AF_INET,
		.checkentry	= physdev_mt_check,
		.match		= physdev_mt,
		.matchsize	= sizeof(struct xt_physdev_info),
		.me		= THIS_MODULE,
	},
	{
		.name		= "physdev",
		.family		= AF_INET6,
		.checkentry	= physdev_mt_check,
		.match		= physdev_mt,
		.matchsize	= sizeof(struct xt_physdev_info),
		.me		= THIS_MODULE,
	},
};

static int __init physdev_mt_init(void)
{
	return xt_register_matches(physdev_mt_reg, ARRAY_SIZE(physdev_mt_reg));
}

static void __exit physdev_mt_exit(void)
{
	xt_unregister_matches(physdev_mt_reg, ARRAY_SIZE(physdev_mt_reg));
}

module_init(physdev_mt_init);
module_exit(physdev_mt_exit);
