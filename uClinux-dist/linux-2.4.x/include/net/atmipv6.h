/* $USAGI: atmipv6.h,v 1.4 2003/05/28 23:27:31 yoshfuji Exp $
 *
 * RFC2492 IPv6 over ATM
 *
 * Copyright (C)2001 USAGI/WIDE Project
 *
 * Written by Kazuyoshi SERIZAWA, based on atmarp.h - RFC1577 ATM ARP
 * Written 1995-1999 by Werner Almesberger, EPFL LRC/ICA
 */
 
#ifndef _ATMIPV6_H
#define _ATMIPV6_H

#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/atm.h>
#include <linux/atmdev.h>
#include <linux/atmarp.h>
#include <linux/spinlock.h>
#include <net/neighbour.h>


#define CLIP6_VCC(vcc) ((struct clip6_vcc *) ((vcc)->user_back))
//#define NEIGH2ENTRY(neigh) ((struct atmarp_entry *) (neigh)->primary_key)


struct clip6_vcc {
	struct atm_vcc	*vcc;		/* VCC descriptor */
	struct atmarp_entry *entry;	/* ATMARP table entry, NULL if IP addr.
					   isn't known yet */
	int		xoff;		/* 1 if send buffer is full */
	unsigned char	encap;		/* 0: NULL, 1: LLC/SNAP */
	unsigned long	last_use;	/* last send or receive operation */
	unsigned long	idle_timeout;	/* keep open idle for so many jiffies*/
	void (*old_push)(struct atm_vcc *vcc,struct sk_buff *skb);
					/* keep old push fn for chaining */
	void (*old_pop)(struct atm_vcc *vcc,struct sk_buff *skb);
					/* keep old pop fn for chaining */
	struct clip6_vcc	*next;		/* next VCC */
        struct net_device *dev;   	/* network interface. */
};

#if 0
struct atmarp_entry {
	u32		ip;		/* IP address */
	struct clip_vcc	*vccs;		/* active VCCs; NULL if resolution is
					   pending */
	unsigned long	expires;	/* entry expiration time */
	struct neighbour *neigh;	/* neighbour back-pointer */
};
#endif /* if 0 */

//TODO:
#define PRIV(dev) ((struct clip6_priv *) ((struct net_device *) (dev)+1))
#define PRIV6(dev) ((struct clip6_priv *) ((struct net_device *) (dev)+1))


struct clip6_priv {
	int number;			/* for convenience ... */
	spinlock_t xoff_lock;		/* ensures that pop is atomic (SMP) */
	struct net_device_stats stats;
	struct net_device *next;	/* next CLIP6 interface */
  	struct clip6_vcc	*vccs;		/* active VCCs; NULL if resolution is
					   pending */
//	struct atm_vcc vccc;		/* vcc ToDo: del*/
	struct atm_vcc *vcc;		/* vcc */
};


//extern struct atm_vcc *atmarpd; /* ugly */
//extern struct neigh_table clip6_tbl;

int clip6_create(int number);
int clip6_mkip(struct atm_vcc *vcc,int timeout);
int clip6_encap(struct atm_vcc *vcc,int mode);

int clip6_set_vcc_netif(struct socket *sock,int number);

void clip6_push(struct atm_vcc *vcc,struct sk_buff *skb);

void atm_clip6_init(void);

#endif
