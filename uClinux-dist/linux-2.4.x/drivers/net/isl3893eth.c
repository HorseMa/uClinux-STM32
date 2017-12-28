/* isl3893.c: ISL3893 Direct Ethernet driver (bypassing MVC).
 *
 * Based on skeleton ethernet driver by Donald Becker.
 * 
 * Copyright 1993 United States Government as represented by the
 * Director, National Security Agency.
 *
 * Modified by Arlet Ottens 2001.
 * Modified by M. Thuys, Copyright (C) 2002 Intersil Americas Inc.
 *
 */

static const char *version = "ISL3893 direct ethernet\n";

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/in.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/module.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/bitops.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <linux/errno.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>

/****************************************************************************
 * Ethernet defines. Since this is a temporary driver, we put them here and
 * not in a nice header file. This way, the stuff can be removed by deleting
 * one file
 ****************************************************************************/

/* - CTRL */
#define	ETHCTRL				(0x0)
#define	ETHCTRLHBD			(0x10000000)
#define	ETHCTRLPS			(0x8000000)
#define	ETHCTRLDRO			(0x800000)
#define	ETHCTRLFDLBM			(0x200000)
#define	ETHCTRLFDM			(0x100000)
#define	ETHCTRLPM			(0x80000)
#define	ETHCTRLPR			(0x40000)
#define	ETHCTRLPB			(0x10000)
#define	ETHCTRLHOF			(0x8000)
#define	ETHCTRLIFE			(0x2000)
#define	ETHCTRLLCC			(0x1000)
#define	ETHCTRLDBP			(0x800)
#define	ETHCTRLDRTY			(0x400)
#define	ETHCTRLASTP			(0x100)
#define	ETHCTRLBOLMT			(0xc0)
#define	ETHCTRLBOLMTShift		(0x6)
#define	ETHCTRLDC			(0x20)
#define	ETHCTRLTE			(0x8)
#define	ETHCTRLRE			(0x4)
#define	ETHCTRLMask			(0x18bdbdec)
#define	ETHCTRLAccessType		(RW)
#define	ETHCTRLInitialValue		(0x40000)
#define	ETHCTRLTestMask			(0x18bdbdec)

/* - MAHR */
#define	ETHMAHR				(0x4)
#define	ETHMAHRMAH			(0xffff)
#define	ETHMAHRMAHShift			(0x0)
#define	ETHMAHRMask			(0xffff)
#define	ETHMAHRAccessType		(RW)
#define	ETHMAHRInitialValue		(0xffff)
#define	ETHMAHRTestMask			(0xffff)

/* - MALR */
#define	ETHMALR				(0x8)
#define	ETHMALRMAL			(0x0)
#define	ETHMALRMALShift			(0x0)
#define	ETHMALRMask			(0x0)
#define	ETHMALRAccessType		(RW)
#define	ETHMALRInitialValue		(0xffffffff)
#define	ETHMALRTestMask			(0x0)

/* - MHTHR */
#define	ETHMHTHR			(0xc)
#define	ETHMHTHRHTH			(0x0)
#define	ETHMHTHRHTHShift		(0x0)
#define	ETHMHTHRMask			(0x0)
#define	ETHMHTHRAccessType		(RW)
#define	ETHMHTHRInitialValue		(0x0)
#define	ETHMHTHRTestMask		(0x0)

/* - MHTLR */
#define	ETHMHTLR			(0x10)
#define	ETHMHTLRHTL			(0x0)
#define	ETHMHTLRHTLShift		(0x0)
#define	ETHMHTLRMask			(0x0)
#define	ETHMHTLRAccessType		(RW)
#define	ETHMHTLRInitialValue		(0x0)
#define	ETHMHTLRTestMask		(0x0)

/* - MIIAR */
#define	ETHMIIAR			(0x14)
#define	ETHMIIARTestMask		(0xffc2)
#define	ETHMIIARPHYADR			(0xf800)
#define	ETHMIIARPHYADRShift		(0xb)
#define	ETHMIIARMIIADR			(0x7c0)
#define	ETHMIIARMIIADRShift		(0x6)
#define	ETHMIIARMIIWR			(0x2)
#define	ETHMIIARMIIBUSY			(0x1)
#define	ETHMIIARMask			(0xffc3)
#define	ETHMIIARAccessType		(RW)
#define	ETHMIIARInitialValue		(0x0)

/* - MIIDR */
#define	ETHMIIDR			(0x18)
#define	ETHMIIDRPHYDATA			(0xffff)
#define	ETHMIIDRPHYDATAShift		(0x0)
#define	ETHMIIDRMask			(0xffff)
#define	ETHMIIDRAccessType		(RW)
#define	ETHMIIDRInitialValue		(0x0)
#define	ETHMIIDRTestMask		(0xffff)

/* - FCR */
#define	ETHFCR				(0x1c)
#define	ETHFCRTestMask			(0xffff0006)
#define	ETHFCRPTIME			(0xfff80000)
#define	ETHFCRPTIMEShift		(0x13)
#define	ETHFCRPCP			(0x4)
#define	ETHFCRFCE			(0x2)
#define	ETHFCRFCB			(0x1)
#define	ETHFCRMask			(0xfff80007)
#define	ETHFCRAccessType		(RW)
#define	ETHFCRInitialValue		(0x0)

/* - VLAN1R */
#define	ETHVLAN1R			(0x20)
#define	ETHVLAN1RVLAN1			(0xffff)
#define	ETHVLAN1RVLAN1Shift		(0x0)
#define	ETHVLAN1RMask			(0xffff)
#define	ETHVLAN1RAccessType		(RW)
#define	ETHVLAN1RInitialValue		(0x0)
#define	ETHVLAN1RTestMask		(0xffff)

/* - VLAN2R */
#define	ETHVLAN2R			(0x24)
#define	ETHVLAN2RVLAN2			(0xffff)
#define	ETHVLAN2RVLAN2Shift		(0x0)
#define	ETHVLAN2RMask			(0xffff)
#define	ETHVLAN2RAccessType		(RW)
#define	ETHVLAN2RInitialValue		(0x0)
#define	ETHVLAN2RTestMask		(0xffff)

/* - INTEN */
#define	ETHINTEN			(0x28)
#define	ETHINTENRXENDEN			(0x4)
#define	ETHINTENRXSTEN			(0x2)
#define	ETHINTENTXENDEN			(0x1)
#define	ETHINTENMask			(0x7)
#define	ETHINTENAccessType		(RW)
#define	ETHINTENInitialValue		(0x0)
#define	ETHINTENTestMask		(0x7)

/* - INTST */
#define	ETHINTST			(0x2c)
#define	ETHINTSTRXEND			(0x4)
#define	ETHINTSTRXSTART			(0x2)
#define	ETHINTSTTXEND			(0x1)
#define	ETHINTSTMask			(0x7)
#define	ETHINTSTAccessType		(RO)
#define	ETHINTSTInitialValue		(0x0)
#define	ETHINTSTTestMask		(0x7)

/* - TXSTAT */
#define	ETHTXSTAT			(0x30)
#define	ETHTXSTATTXRETRY		(0x10000000)
#define	ETHTXSTATHBF			(0x800)
#define	ETHTXSTATCOLCNT			(0x400)
#define	ETHTXSTATLCOLOB			(0x200)
#define	ETHTXSTATDEF			(0x100)
#define	ETHTXSTATURUN			(0x80)
#define	ETHTXSTATEXCOL			(0x40)
#define	ETHTXSTATTXLCOL			(0x20)
#define	ETHTXSTATEXDEF			(0x10)
#define	ETHTXSTATLCAR			(0x8)
#define	ETHTXSTATNCAR			(0x4)
#define	ETHTXSTATTXABORT		(0x1)
#define	ETHTXSTATMask			(0x10000ffd)
#define	ETHTXSTATAccessType		(RO)
#define	ETHTXSTATInitialValue		(0x0)
#define	ETHTXSTATTestMask		(0x10000ffd)

/* - TXBCR */
#define	ETHTXBCR			(0x34)
#define	ETHTXBCRTXBC			(0xffff)
#define	ETHTXBCRTXBCShift		(0x0)
#define	ETHTXBCRMask			(0xffff)
#define	ETHTXBCRAccessType		(RW)
#define	ETHTXBCRInitialValue		(0x5ee)
#define	ETHTXBCRTestMask		(0xffff)

/* - TXADDR */
#define	ETHTXADDR			(0x38)
#define	ETHTXADDRTXBA			(0x1fffffff)
#define	ETHTXADDRTXBAShift		(0x0)
#define	ETHTXADDRMask			(0x1fffffff)
#define	ETHTXADDRAccessType		(RW)
#define	ETHTXADDRInitialValue		(0x0)
#define	ETHTXADDRTestMask		(0x1fffffff)

/* - TXCTRL */
#define	ETHTXCTRL			(0x3c)
#define	ETHTXCTRLTXFLUSH		(0x1)
#define	ETHTXCTRLMask			(0x1)
#define	ETHTXCTRLAccessType		(WO)
#define	ETHTXCTRLInitialValue		(0x0)
#define	ETHTXCTRLTestMask		(0x1)

/* - RXSTAT */
#define	ETHRXSTAT			(0x40)
#define	ETHRXSTATMF			(0x80000000)
#define	ETHRXSTATPF			(0x40000000)
#define	ETHRXSTATBCP			(0x10000000)
#define	ETHRXSTATMCP			(0x8000000)
#define	ETHRXSTATUCP			(0x4000000)
#define	ETHRXSTATCP			(0x2000000)
#define	ETHRXSTATLE			(0x1000000)
#define	ETHRXSTATRXVLAN2		(0x800000)
#define	ETHRXSTATRXVLAN1		(0x400000)
#define	ETHRXSTATCRCERR			(0x200000)
#define	ETHRXSTATDRIB			(0x100000)
#define	ETHRXSTATMIIERR			(0x80000)
#define	ETHRXSTATPKTYPE			(0x40000)
#define	ETHRXSTATRXLCOL			(0x20000)
#define	ETHRXSTATPTL			(0x10000)
#define	ETHRXSTATRUNT			(0x8000)
#define	ETHRXSTATWDTO			(0x4000)
#define	ETHRXSTATPKTLEN			(0x3fff)
#define	ETHRXSTATPKTLENShift		(0x0)
#define	ETHRXSTATMask			(0xdfffffff)
#define	ETHRXSTATAccessType		(RO)
#define	ETHRXSTATInitialValue		(0x0)
#define	ETHRXSTATTestMask		(0xdfffffff)

/* - RXBCR */
#define	ETHRXBCR			(0x44)
#define	ETHRXBCRRXBC			(0xffff)
#define	ETHRXBCRRXBCShift		(0x0)
#define	ETHRXBCRMask			(0xffff)
#define	ETHRXBCRAccessType		(RW)
#define	ETHRXBCRInitialValue		(0x5ee)
#define	ETHRXBCRAccessType		(RW)
#define	ETHRXBCRTestMask		(0xffff)

/* - RXADDR */
#define	ETHRXADDR			(0x48)
#define	ETHRXADDRRXBA			(0x1fffffff)
#define	ETHRXADDRRXBAShift		(0x0)
#define	ETHRXADDRMask			(0x1fffffff)
#define	ETHRXADDRAccessType		(RW)
#define	ETHRXADDRInitialValue		(0x0)
#define	ETHRXADDRTestMask		(0x1fffffff)

/* - RXCTRL */
#define	ETHRXCTRL			(0x4c)
#define	ETHRXCTRLRXFLUSH		(0x1)
#define	ETHRXCTRLMask			(0x1)
#define	ETHRXCTRLAccessType		(WO)
#define	ETHRXCTRLInitialValue		(0x0)
#define	ETHRXCTRLTestMask		(0x1)

/* C struct view */

typedef struct s_ETH {
 unsigned CTRL; /* @0 */
 unsigned MAHR; /* @4 */
 unsigned MALR; /* @8 */
 unsigned MHTHR; /* @12 */
 unsigned MHTLR; /* @16 */
 unsigned MIIAR; /* @20 */
 unsigned MIIDR; /* @24 */
 unsigned FCR; /* @28 */
 unsigned VLAN1R; /* @32 */
 unsigned VLAN2R; /* @36 */
 unsigned INTEN; /* @40 */
 unsigned INTST; /* @44 */
 unsigned TXSTAT; /* @48 */
 unsigned TXBCR; /* @52 */
 unsigned TXADDR; /* @56 */
 unsigned TXCTRL; /* @60 */
 unsigned RXSTAT; /* @64 */
 unsigned RXBCR; /* @68 */
 unsigned RXADDR; /* @72 */
 unsigned RXCTRL; /* @76 */
} s_ETH;

#define uETH1 ((volatile struct s_ETH *) 0xc0000980)
#define uETH2 ((volatile struct s_ETH *) 0xc0000a00)

/****************************************************************************/

/*
 * The name of the card. Is used for messages and in the requests for
 * io regions, irqs and dma channels
 */
static const char* cardname = "ethernet";

/* First, a few definitions that the brave might change. */

/* use 0 for production, 1 for verification, >2 for debug */
#ifndef NET_DEBUG
#define NET_DEBUG 2
#endif
static unsigned int net_debug = NET_DEBUG;

/* macros to simplify debug checking */
#if 1
#define BUGMSG(args...) do{printk(## args);}while(0)
#else
#define BUGMSG(args...) 
#endif

#define PKT_BUF_SZ	1536	/* maximum packet size */

/*
 * mask of all rx errors
 */
#define ETHRXSTATERROR ( ETHRXSTATCRCERR | ETHRXSTATDRIB | \
 		         ETHRXSTATMIIERR | ETHRXSTATRUNT )

/* Note, code depends on these being power of 2 */
#define ETH_RXQ_SIZE	8
#define ETH_TXQ_SIZE	8

/* Information that need to be kept for each board. */
struct net_local {
	struct net_device_stats	    stats;		/* tx/rx statistics */
	long open_time;					/* Useless example local info. */
	volatile struct s_ETH *uEth;			/* Pointer to Ethernet device */
	struct sk_buff *rx_frames[ETH_RXQ_SIZE];	/* frame RX ring */
	int rxq_ptr;
	struct sk_buff *tx_frames[ETH_TXQ_SIZE];	/* frame TX ring */
	int txq_p;					/* TX produce pointer */
	int txq_c;					/* TX consume pointer */
};

/* Index to functions, as function prototypes. */
extern int isl3893_probe(struct net_device *dev);

static int isl3893_probe1(struct net_device *dev, int ioaddr);
static int net_open(struct net_device *dev);
static int net_send_packet(struct sk_buff *skb, struct net_device *dev);
static void net_interrupt(int irq, void *dev_id, struct pt_regs *regs);
static int net_close(struct net_device *dev);
static struct net_device_stats *net_get_stats(struct net_device *dev);

static void chipset_init(struct net_device *dev, int startp);


/*
 * Check for a network adaptor of this type, and return '0' iff one exists.
 * If dev->base_addr == 0, probe all likely locations.
 * If dev->base_addr == 1, always return failure.
 * If dev->base_addr == 2, allocate space for the device and return success
 * (detachable devices only).
 */
int
isl3893_ether_probe(struct net_device *dev)
{
	/* For now only detect eth0 */
	if(dev->base_addr != 0)
		return -ENODEV;

	return isl3893_probe1( dev, 0 );
}


/*
 * This is the real probe routine. 
 */
static int isl3893_probe1(struct net_device *dev, int ioaddr)
{
	static unsigned version_printed = 0;
	u_char ether_address[] = "\x00\x10\x91\x00\xed\xcb";
	int i;

	/* Allocate a new 'dev' if needed. */
	if (dev == NULL) {
		/*
		 * Don't allocate the private data here, it is done later
		 * This makes it easier to free the memory when this driver
		 * is used as a module.
		 */
		dev = init_etherdev(0, 0);
		if (dev == NULL)
			return -ENOMEM;
	}

	if (net_debug  &&  version_printed++ == 0)
		printk(KERN_DEBUG "%s", version);

	printk(KERN_INFO "%s: %s found, ", dev->name, cardname );

	/* Fill in the 'dev' fields. */
	if (dev->base_addr == 0)
		dev->base_addr = ioaddr;

	/* Retrieve and print the ethernet address. */
	for (i = 0; i < 6; i++)
		printk(" %2.2x", dev->dev_addr[i] = ether_address[i] );

	/* Initialize the device structure. */
	if (dev->priv == NULL) {
		dev->priv = kmalloc(sizeof(struct net_local), GFP_KERNEL);
		if (dev->priv == NULL)
			return -ENOMEM;
	}
	memset(dev->priv, 0, sizeof(struct net_local));

	dev->open		= net_open;
	dev->stop		= net_close;
	dev->get_stats		= net_get_stats;
	dev->hard_start_xmit 	= net_send_packet;

	ether_setup(dev);

	SET_MODULE_OWNER(dev);

	return 0;
}

static void chipset_init( struct net_device *dev, int startp )
{
	struct net_local *lp = (struct net_local *)dev->priv;
	int i;

	/* RX enable, TX enable, Promiscuous mode */
	lp->uEth->CTRL = ETHCTRLRE | ETHCTRLTE | ETHCTRLPR;

	/* Fill the RX FIFO */
	lp->uEth->RXBCR = PKT_BUF_SZ;
	for( i = 0; i < ETH_RXQ_SIZE; i++ ) {
		lp->uEth->RXADDR = (int) lp->rx_frames[i]->data;
	}
	/* Enable RX end, TX end and RX start interrupts */
	lp->uEth->INTEN = ETHINTENRXENDEN | ETHINTENRXSTEN | ETHINTENTXENDEN;
}

/*
 * Open/initialize the board. This is called (in the current kernel)
 * sometime after booting when the 'ifconfig' program is run.
 *
 * This routine should set everything up anew at each open, even
 * registers that "should" only need to be set once at boot, so that
 * there is non-reboot way to recover if something goes wrong.
 */
static int
net_open(struct net_device *dev)
{
	struct net_local *lp = (struct net_local *)dev->priv;
	struct sk_buff *skb;
	int irqval;
	int i;

	MOD_INC_USE_COUNT;

	/* For now we always do Eth1 */
	lp->uEth = uETH1;

	BUGMSG("%s:Opening Ethernet interface ", dev->name);
	if(lp->uEth == uETH1) {
		BUGMSG("1\n");
		dev->irq = IRQ_ETH1;
	} else {
		BUGMSG("2\n");
		dev->irq = IRQ_ETH2;
	}
	irqval = request_irq(dev->irq, &net_interrupt, SA_INTERRUPT, cardname, NULL);

	if (irqval) {
		printk("%s: unable to get IRQ %d (irqval=%d).\n",
			   dev->name, dev->irq, irqval);
		return -EAGAIN;
	}

	/* Allocate Ethernet RX buffers */
	for( i = 0; i < ETH_RXQ_SIZE; i++ ) {
		skb = dev_alloc_skb( PKT_BUF_SZ );
		if (skb == NULL) {
			printk("%s: Could not allocate RX Ring buffer\n", dev->name);
			while(i != 0)
				dev_kfree_skb(lp->rx_frames[--i]);

			return -ENOMEM;
		}
		skb->dev = dev;
		skb->protocol = eth_type_trans(skb, dev);
		skb->ip_summed = CHECKSUM_UNNECESSARY; /* don't check it */
		/* word align IP header */
		skb_reserve( skb, 2 );
    		lp->rx_frames[i] = skb;
	}
	lp->rxq_ptr = 0;
	lp->txq_p = 0;
	lp->txq_c = 0;

	BUGMSG("%s: Resetting hardware\n", dev->name);
	/* Reset the hardware here. */
	chipset_init( dev, 1 );
	lp->open_time = jiffies;

	BUGMSG("%s: netif_start_queue\n", dev->name);
	netif_start_queue(dev);
	return 0;
}

static int
net_send_packet(struct sk_buff *skb, struct net_device *dev)
{
	struct net_local *lp = (struct net_local *)dev->priv;
	short length = ETH_ZLEN < skb->len ? skb->len : ETH_ZLEN;

	BUGMSG("%s: net_send_packet p %d, c %d\n", dev->name, lp->txq_p, lp->txq_c);

	if((lp->txq_p - lp->txq_c) > (ETH_TXQ_SIZE - 1)) {
		printk("TX Queue full, dropping packet! (p %d, c %d)\n", lp->txq_p, lp->txq_c);
		dev_kfree_skb(skb);
		return 0;
	}

	/* Put packet in the Ethernet queue */
	lp->uEth->TXBCR = length;
	lp->uEth->TXADDR = (int)skb->data;

	lp->tx_frames[lp->txq_p++ & (ETH_TXQ_SIZE - 1)] = skb;
	dev->trans_start = jiffies;

	BUGMSG("%s: net_send_packet finished\n", dev->name);
	return 0;
}

/*
 * The typical workload of the driver:
 *   Handle the network interface interrupts.
 */
static void
net_interrupt(int irq, void *dev_id, struct pt_regs * regs)
{
	struct net_device *dev;
	struct net_local *lp;
	int status = 0;

	BUGMSG("net_interrupt..");
	if (dev_id == NULL) {
		printk(KERN_WARNING "%s: irq %d for unknown device.\n", cardname, irq);
		return;
	}

	dev = (struct net_device *) dev_id;
	lp = (struct net_local *) dev->priv;

	status = lp->uEth->INTST;

	/* RX Start: No frame available during frame RX */
	if(status & ETHINTSTRXSTART) {
		printk("PANIC: Ethernet RX Queue is empty: this should NEVER Occur!!!\n");
		printk("rxq_ptr = %d\n", lp->rxq_ptr);
	}

	/* RX End: a frame is received successfully */
	if(status & ETHINTSTRXEND) {
		BUGMSG("RX end..");
		do {
			unsigned int rx_packet_stat = lp->uEth->RXSTAT;

			if(rx_packet_stat & ETHRXSTATERROR) {
				printk("RX Packet error 0x%.8x\n", rx_packet_stat);
				lp->stats.rx_dropped++;
				/* We reuse the frame */
				lp->uEth->RXADDR = (int)lp->rx_frames[lp->rxq_ptr & (ETH_RXQ_SIZE -1)];
			} else {
				unsigned int length = (rx_packet_stat & 0x3fff);
				struct sk_buff *skb;
				struct sk_buff *newskb;

				BUGMSG("rxq %d, skblen %d..", lp->rxq_ptr, length);
				/* The received skb */
				skb = lp->rx_frames[lp->rxq_ptr & (ETH_RXQ_SIZE -1)];
				skb_put(skb, length);
				lp->stats.rx_packets++;
				lp->stats.rx_bytes += length;

				/* Alloc new skb for rx_frame ringbuffer */
				newskb = dev_alloc_skb( PKT_BUF_SZ );
				if (newskb == NULL) {
					/* We assume that we can consume and produce the RX ring
					 * buffer at the same time. In this case, we cannot
					 * produce so we will eventually crash ...
					 */
					printk("Cannot allocate skb. This is very bad.... We will CRASH!\n");
				} else {
					newskb->dev = dev;
					newskb->protocol = eth_type_trans(skb, dev);
					newskb->ip_summed = CHECKSUM_UNNECESSARY; /* don't check it */
					/* word align IP header */
					skb_reserve( newskb, 2 );
					lp->rx_frames[lp->rxq_ptr & (ETH_RXQ_SIZE -1)] = newskb;
					/* Put in Ethernet RX queue */
					lp->uEth->RXADDR = (int) newskb->data;
				}
				lp->rxq_ptr++;
				netif_rx(skb);
			}
		} while(lp->uEth->INTST & ETHINTSTRXEND);
	}

	if(status & ETHINTSTTXEND) {
		BUGMSG("TX end..");
		do {
			unsigned int tx_packet_stat = lp->uEth->TXSTAT;
			struct sk_buff *skb;

			BUGMSG("*%d %d", lp->txq_p, lp->txq_c);
			if(tx_packet_stat)
				printk("TX Packet error 0x%.8x\n", tx_packet_stat);

			skb = lp->tx_frames[lp->txq_c & (ETH_TXQ_SIZE -1)];
			/* paranoia check should be removed */
			if(skb == NULL) {
				printk("ERROR: tx frame NULL\n");
				break;
			}
			lp->tx_frames[lp->txq_c & (ETH_TXQ_SIZE -1)] = NULL;
			lp->txq_c++;
			lp->stats.tx_packets++;
			dev_kfree_skb(skb);
		} while(lp->uEth->INTST & ETHINTSTTXEND);
	}
	BUGMSG("\n");
	return;
}

/* The inverse routine to net_open(). */
static int
net_close(struct net_device *dev)
{
	BUGMSG("%s: net_close\n", dev->name);

	netif_stop_queue(dev); /* can't transmit any more */

	MOD_DEC_USE_COUNT;
	return 0;
}

/*
 * Get the current statistics.
 * This may be called with the card open or closed.
 */
static struct net_device_stats *
net_get_stats(struct net_device *dev)
{
	struct net_local *lp = (struct net_local *)dev->priv;

	BUGMSG("%s: net_get_stats\n", dev->name);
	return &lp->stats;
}
