/* ax88796.c: driver for the AX88796 NE2000 clone
 *
 * Copyright (C) 2004 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * Derived from ne2k-pci.c

	A Linux device driver for PCI NE2000 clones.

	Authors and other copyright holders:
	1992-2000 by Donald Becker, NE2000 core and various modifications.
	1995-1998 by Paul Gortmaker, core modifications and PCI support.
	Copyright 1993 assigned to the United States Government as represented
	by the Director, National Security Agency.

	This software may be used and distributed according to the terms of
	the GNU General Public License (GPL), incorporated herein by reference.
	Drivers based on or derived from this code fall under the GPL and must
	retain the authorship, copyright and license notice.  This file is not
	a complete program and may only be used when the entire operating
	system is licensed under the GPL.

	The author may be reached as becker@scyld.com, or C/O
	Scyld Computing Corporation
	410 Severn Ave., Suite 210
	Annapolis MD 21403

	Issues remaining:
	People are making PCI ne2000 clones! Oh the horror, the horror...
	Limited full-duplex support.
*/

#define DRV_NAME	"ax88796"
#define DRV_VERSION	"1.02"
#define DRV_RELDATE	"10/19/2000"


/* The user-configurable values.
   These may be modified when a driver module is loaded.*/

static int debug = 1;			/* 1 normal messages, 0 quiet .. 7 verbose. */

/* Force a non std. amount of memory.  Units are 256 byte pages. */
/* #define PACKETBUF_MEMSIZE	0x40 */


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/ethtool.h>
#include <linux/mii.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/delay.h>
#include <asm/ax88796.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include "8390.h"

#define AX88796_DODGY_REMOTEDMA 1	/* remote DMA on reception seems dodgy on this chip */

/* These identify the driver base version and may not be removed. */
static char version[] __devinitdata =
KERN_INFO DRV_NAME ".c:v" DRV_VERSION " " DRV_RELDATE " D. Becker/P. Gortmaker\n"
KERN_INFO "  http://www.scyld.com/network/ne2k-pci.html\n";

#ifdef __frv__

#define ax_inb(A)				\
({						\
	unsigned long a = (A);			\
	uint8_t x = inb(a);			\
	x;					\
})

#define ax_outb(V,A)				\
do {						\
	unsigned long a = (A);			\
	uint8_t x = (V);			\
	outb(x, a);				\
} while(0)

#else
#error not __frv__
#endif

#define PFX DRV_NAME ": "

MODULE_AUTHOR("Donald Becker / Paul Gortmaker / David Howells");
MODULE_DESCRIPTION("Ax88796 NE2000-clone driver");
MODULE_LICENSE("GPL");

MODULE_PARM(debug, "i");
MODULE_PARM_DESC(debug, "debug level (1-2)");

struct ax88796_private {
	struct mii_if_info	mii_if;
	spinlock_t		lock;
};

static struct net_device *ax88796_device;

enum {
	FORCE_FDX	= 0x20,		/* Full-Duplex user override. */
};


/* ---- No user-serviceable parts below ---- */

#define NE_BASE		(dev->base_addr)
#define NE_CMD		0x00
#define NE_DATAPORT	0x10	/* NatSemi-defined port window offset. */
#define NE_MEMR		0x14	/* EEPROM/MII port */
#define NE_RESET	0x1f	/* Issue a read to reset, a write to clear. */
#define NE_IO_EXTENT	0x20

#define NESM_START_PG	0x40	/* First page of TX buffer */
#define NESM_STOP_PG	0x80	/* Last page +1 of RX ring */

#define EEPROM_PHYADDR	0x00	/* offset of PHY addr in EEPROM */

#define MDIO_SHIFT_CLK		0x01	/* MII clock signal bit */
#define MDIO_ENB_IN		0x02	/* MII data direction bit (0 out, 1 in) */
#define MDIO_DATA_READ		0x04	/* MII data input signal bit */
#define MDIO_DATA_WRITE0	0x00	/* MII data output signal bit as 0 */
#define MDIO_DATA_WRITE1	0x08	/* MII data output signal bit as 1 */

static int ax88796_open(struct net_device *dev);
static int ax88796_close(struct net_device *dev);

static void ax88796_reset_8390(struct net_device *dev);
static void ax88796_get_8390_hdr(struct net_device *dev, struct e8390_pkt_hdr *hdr,
			  int ring_page);
static void ax88796_block_input(struct net_device *dev, int count,
			  struct sk_buff *skb, int ring_offset);
static void ax88796_block_output(struct net_device *dev, const int count,
		const unsigned char *buf, const int start_page);
static int netdev_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);


/*****************************************************************************/
/*
 * send a command to the EEPROM
 */
static inline void ax88796_eeprom_command(unsigned long memr, int32_t command, int nbits)
{
	uint8_t bit;

	command <<= 32 - nbits;

	outb(0x10, memr);	/* turn on EECS 20uS in advance */
	udelay(10);
	outb(0x90, memr);	/* clock through */
	udelay(10);

	do {
		bit = 0x10;			/* keep EECS on */
		if (command < 0)
			bit |= 0x20;		/* set EEI if there's a set bit */
		outb(bit, memr);
		udelay(10);
		outb(bit | 0x80, memr);		/* clock through */
		udelay(10);
		command <<= 1;
	} while (--nbits > 0);

} /* end ax88796_eeprom_command() */

/*****************************************************************************/
/*
 * read data from the EEPROM
 */
static void ax88796_eeprom_read(unsigned long memr, uint16_t addr, void *buffer, unsigned count)
{
	uint8_t x, y;
	int nbits;

	ax88796_eeprom_command(memr, 0x600 | (addr & 0x7f), 11); /* 1,10,X,A6-A0 */

	do {
		y = 0;
		for (nbits = 7; nbits >= 0; nbits--) {
			outb(0x10, memr);		/* waggle EECLK whilst keeping EECS high */
			udelay(10);
			outb(0x90, memr);
			udelay(10);
			x = (inb(memr) & 0x40) >> 6;
			y |= x << nbits;
		}

		*(uint8_t *) buffer = y;
		buffer++;
	} while (--count > 0);

	/* terminate the EEPROM command */
	outb(0x10, memr);
	udelay(20);
	outb(0x00, memr);
	udelay(20);

} /* end ax88796_eeprom_read() */

/*****************************************************************************/
/*
 * synchronise the MII protocol by writing 32 ones
 */
static void ax88796_mdio_sync(unsigned long addr)
{
	int bits;

	for (bits = 0; bits < 32; bits++) {
		outb(MDIO_DATA_WRITE1, addr);
		outb(MDIO_DATA_WRITE1 | MDIO_SHIFT_CLK, addr);
	}
} /* end ax88796_mdio_sync() */

/*****************************************************************************/
/*
 * read from an MII register
 */
static int ax88796_mdio_read(struct net_device *dev, int phy_id, int loc)
{
	unsigned long ioaddr = dev->base_addr + NE_MEMR;
	uint32_t cmd = (0xf6 << 10) | (phy_id << 5) | loc;
	int i, retval;

	ax88796_mdio_sync(ioaddr);

	for (i = 14; i >= 0; i--) {
		int dat = (cmd & (1 << i)) ? MDIO_DATA_WRITE1 : MDIO_DATA_WRITE0;
		outb(dat, ioaddr);
		outb(dat | MDIO_SHIFT_CLK, ioaddr);
	}

	retval = 0;

	for (i = 19; i > 0; i--) {
		outb(MDIO_ENB_IN, ioaddr);
		retval = (retval << 1) | ((inb(ioaddr) & MDIO_DATA_READ) != 0);
		outb(MDIO_ENB_IN | MDIO_SHIFT_CLK, ioaddr);
	}

	return (retval >> 1) & 0xffff;
} /* end ax88796_mdio_read() */

/*****************************************************************************/
/*
 * write to an MII register
 */
static void ax88796_mdio_write(struct net_device *dev, int phy_id, int loc, int val)
{
	unsigned long ioaddr = dev->base_addr + NE_MEMR;
	uint32_t cmd = (0x05 << 28) | (phy_id << 23) | (loc << 18) | (1 << 17) | val;
	int i;

	ax88796_mdio_sync(ioaddr);

	for (i = 31; i >= 0; i--) {
		int dat = (cmd & (1 << i)) ? MDIO_DATA_WRITE1 : MDIO_DATA_WRITE0;
		outb(dat, ioaddr);
		outb(dat | MDIO_SHIFT_CLK, ioaddr);
	}

	for (i = 1; i >= 0; i--) {
		outb(MDIO_ENB_IN, ioaddr);
		outb(MDIO_ENB_IN | MDIO_SHIFT_CLK, ioaddr);
	}
} /* end ax88796_mdio_write() */

/*****************************************************************************/
/*
 * init the device's MII
 */
static void __devinit ax88796_init_mii(struct net_device *dev)
{
	struct ax88796_private *priv = (struct ax88796_private *) ei_status.priv;
	unsigned index, ident, bmsr;

	priv->mii_if.phy_id		= 0xffffffff;
	priv->mii_if.phy_id_mask	= 0x1f;
	priv->mii_if.reg_num_mask	= 0x0;
	priv->mii_if.dev		= dev;
	priv->mii_if.mdio_read		= ax88796_mdio_read;
	priv->mii_if.mdio_write		= ax88796_mdio_write;

	for (index = 0; index < 32; index++) {
		bmsr = ax88796_mdio_read(dev, index, MII_BMSR);
		if (bmsr != 0 && bmsr != 0xffff)
			break;
	}

	if (index < 32) {
		priv->mii_if.phy_id = index;

		ident = ax88796_mdio_read(dev, index, MII_PHYSID1) << 16;
		ident |= ax88796_mdio_read(dev, index, MII_PHYSID2);
		printk("  MII transceiver at index %d, status %x, ident %08x\n",
		       priv->mii_if.phy_id, bmsr, ident);
	}
	else {
		printk(KERN_NOTICE "  No MII transceivers found!\n");
	}

} /* end ax88796_init_mii() */


/*****************************************************************************/
/*
 * Power management 
 */
#ifdef CONFIG_PM
#include <linux/pm.h>

static struct pm_dev *ax88796_pm_dev;

static int ax88796_pm_callback(struct pm_dev *pdev, pm_request_t rqst, 
			       void *data)
{
	struct net_device *dev = ax88796_device;
	
	if (rqst == PM_SUSPEND)
		ei_close(dev);
	else
		ei_open(dev);
	
	return 0;
}
#endif

/*****************************************************************************/
/*
 * initialise the interface
 * - need to read the MAC address from the EEPROM
 */
static int __devinit ax88796_init(void)
{
	struct net_device *dev;
	unsigned long reset_start_time;
	unsigned char phy_addr[6];
	unsigned long ioaddr = AX88796_IOADDR;
	int reg0, regd;
	int i;

/* when built into the kernel, we only print version if device is found */
#ifndef MODULE
	static int printed_version;
	if (!printed_version++)
		printk(version);
#endif

	if (request_region(ioaddr, NE_IO_EXTENT, DRV_NAME) == NULL) {
		printk (KERN_ERR PFX "I/O resource 0x%x @ 0x%lx busy\n",
			NE_IO_EXTENT, ioaddr);
		return -EBUSY;
	}

	reg0 = ax_inb(ioaddr);
	if (reg0 == 0xFF)
		goto err_out_free_res;

	/* Do a preliminary verification that we have a 8390. */
	ax_outb(E8390_NODMA + E8390_PAGE1 + E8390_STOP, ioaddr + E8390_CMD);
	regd = ax_inb(ioaddr + 0x0d);
	ax_outb(0xff, ioaddr + 0x0d);
	ax_outb(E8390_NODMA+E8390_PAGE0, ioaddr + E8390_CMD);
	ax_inb(ioaddr + EN0_COUNTER0); /* Clear the counter by reading. */
	if (ax_inb(ioaddr + EN0_COUNTER0) != 0) {
		ax_outb(reg0, ioaddr);
		ax_outb(regd, ioaddr + 0x0d);	/* Restore the old values. */
		goto err_out_free_res;
	}

	dev = alloc_etherdev(0);
	if (!dev) {
		printk (KERN_ERR PFX "cannot allocate ethernet device\n");
		goto err_out_free_res;
	}
	SET_MODULE_OWNER(dev);

	/* Reset card. Who knows what dain-bramaged state it was left in. */
	reset_start_time = jiffies;

	ax_outb(ax_inb(ioaddr + NE_RESET), ioaddr + NE_RESET);

	/* This looks like a horrible timing loop, but it should never take
	 * more than a few cycles.
	 */
	while ((ax_inb(ioaddr + EN0_ISR) & ENISR_RESET) == 0)
		/* Limit wait: '2' avoids jiffy roll-over. */
		if (jiffies - reset_start_time > 2) {
			printk(KERN_ERR PFX "Card failure (no reset ack).\n");
			goto err_out_free_netdev;
		}

	ax_outb(0xff, ioaddr + EN0_ISR); /* Ack all intr. */

	/* unlike other NE2000 clones, our EEPROM is not accessible through the card's memory
	 * window, so we have to read the EEPROM directly */
	ax88796_eeprom_read(ioaddr + NE_MEMR, EEPROM_PHYADDR, &phy_addr, 6);

	/* We always set the 8390 registers for word mode. */
	ax_outb(0x49, ioaddr + EN0_DCFG);

	/* Set up the rest of the parameters. */
	dev->irq	= AX88796_IRQ;
	dev->base_addr	= ioaddr;

	/* Allocate dev->priv and fill in 8390 specific dev fields. */
	if (ethdev_init(dev)) {
		printk (KERN_ERR "Ax88796: unable to get memory for 8390 data\n");
		goto err_out_free_netdev;
	}

	/* allocate my private data for MII and suchlike */
	ei_status.priv = (unsigned long) kmalloc(sizeof(struct ax88796_private), GFP_KERNEL);
	if (!ei_status.priv) {
		printk (KERN_ERR "Ax88796: unable to get memory for private data\n");
		goto err_out_free_8390;
	}

	memset((void *) ei_status.priv, 0, sizeof(struct ax88796_private));
	spin_lock_init(&((struct ax88796_private *) ei_status.priv)->lock);

	ei_status.name		= "Ax88796";
	ei_status.tx_start_page	= NESM_START_PG;
	ei_status.stop_page	= NESM_STOP_PG;
	ei_status.word16	= 1;
	ei_status.reg0		= AX88796_FULL_DUPLEX ? FORCE_FDX : 0;
	ei_status.rx_start_page = NESM_START_PG + TX_PAGES;

#ifdef PACKETBUF_MEMSIZE
	/* Allow the packet buffer size to be overridden by know-it-alls. */
	ei_status.stop_page	= ei_status.tx_start_page + PACKETBUF_MEMSIZE;
#endif

	ei_status.reset_8390	= &ax88796_reset_8390;
	ei_status.block_input	= &ax88796_block_input;
	ei_status.block_output	= &ax88796_block_output;
	ei_status.get_8390_hdr	= &ax88796_get_8390_hdr;

	dev->open	= &ax88796_open;
	dev->stop	= &ax88796_close;
	dev->do_ioctl	= &netdev_ioctl;
	NS8390_init(dev, 0);

	i = register_netdev(dev);
	if (i)
		goto err_out_free_priv;

	printk("Ax88796 found at %#lx, IRQ %d, ", ioaddr, dev->irq);
	for(i = 0; i < 6; i++) {
		printk("%2.2X%s", phy_addr[i], i == 5 ? ".\n" : ":");
		dev->dev_addr[i] = phy_addr[i];
	}

	ax88796_init_mii(dev);

	ax88796_device = dev;
#ifdef CONFIG_PM
	ax88796_pm_dev = pm_register(PM_UNKNOWN_DEV, 0, ax88796_pm_callback);
#endif
	return 0;

 err_out_free_priv:
	kfree((void *) ei_status.priv);
 err_out_free_8390:
	kfree(dev->priv);
 err_out_free_netdev:
	kfree(dev);
 err_out_free_res:
	release_region(ioaddr, NE_IO_EXTENT);
	return -ENODEV;
} /* end ax88796_init() */

/*****************************************************************************/
/*
 * open up the interface and activate it
 */
static int ax88796_open(struct net_device *dev)
{
	int ret = request_irq(dev->irq, ei_interrupt, SA_SHIRQ, dev->name, dev);
	if (ret)
		return ret;

	/* attempt to set full duplex if so requested */
	if (ei_status.reg0 & FORCE_FDX) {
		long ioaddr = dev->base_addr;
		ax_outb(ax_inb(ioaddr + EN0_TXCR) | 0x80, ioaddr + EN0_TXCR);
	}
	ei_open(dev);
	return 0;
} /* end ax88796_open() */

/*****************************************************************************/
/*
 * close down the interface
 */
static int ax88796_close(struct net_device *dev)
{
	ei_close(dev);
	free_irq(dev->irq, dev);
	return 0;
} /* end ax88796_close() */

/*****************************************************************************/
/*
 * hard reset the card
 * - this used to pause for the same period that a 8390 reset command required, but that
 *   shouldn't be necessary
 */
static void ax88796_reset_8390(struct net_device *dev)
{
	unsigned long reset_start_time = jiffies;

	if (debug > 1)
		printk("%s: Resetting the 8390 t=%ld...", dev->name, jiffies);

	ax_outb(ax_inb(NE_BASE + NE_RESET), NE_BASE + NE_RESET);

	ei_status.txing = 0;
	ei_status.dmaing = 0;

	/* This check _should_not_ be necessary, omit eventually. */
	while ((ax_inb(NE_BASE+EN0_ISR) & ENISR_RESET) == 0)
		if (jiffies - reset_start_time > 2) {
			printk("%s: ax88796_reset_8390() did not complete.\n", dev->name);
			break;
		}
	ax_outb(ENISR_RESET, NE_BASE + EN0_ISR);	/* Ack intr. */
} /* end ax88796_reset_8390() */

/*****************************************************************************/
/*
 * read the 8390 reception ring buffer header
 * - will occupy the four bytes at the beginning of the BOUNDARY+1 page
 */
static void ax88796_get_8390_hdr(struct net_device *dev, struct e8390_pkt_hdr *hdr, int ring_page)
{

	long nic_base = dev->base_addr;

	/* This *shouldn't* happen. If it does, it's the last thing you'll see */
	if (ei_status.dmaing) {
		printk("%s: DMAing conflict in ax88796_get_8390_hdr "
		       "[DMAstat:%d][irqlock:%d].\n",
		       dev->name, ei_status.dmaing, ei_status.irqlock);
		return;
	}

	ei_status.dmaing |= 0x01;
	ax_outb(E8390_NODMA + E8390_PAGE0 + E8390_START, nic_base + NE_CMD);
	ax_outb(0, nic_base + EN0_RSARLO);		/* On page boundary */
	ax_outb(ring_page, nic_base + EN0_RSARHI);
	ax_outb(sizeof(struct e8390_pkt_hdr), nic_base + EN0_RCNTLO);
	ax_outb(0, nic_base + EN0_RCNTHI);
	ax_outb(E8390_RREAD + E8390_START, nic_base + NE_CMD);

#ifdef AX88796_DODGY_REMOTEDMA
	udelay(1);
#endif

	insw(NE_BASE + NE_DATAPORT, hdr, sizeof(struct e8390_pkt_hdr)>>1);
	hdr->count = (hdr->count >> 8) | (hdr->count << 8);

	ax_outb(ENISR_RDC, nic_base + EN0_ISR);	/* Ack intr. */
	ei_status.dmaing &= ~0x01;

	uint8_t cmd, curp;
	cmd = inb(nic_base + NE_CMD);
	outb(cmd|E8390_PAGE1, nic_base + NE_CMD);
	curp = inb(nic_base + EN1_CURPAG);
	outb(cmd, nic_base + NE_CMD);
} /* end ax88796_get_8390_hdr() */

/* Block input and output, similar to the Crynwr packet driver.  If you
   are porting to a new ethercard, look at the packet driver source for hints.
   The NEx000 doesn't share the on-board packet memory -- you have to put
   the packet out through the "remote DMA" dataport using ax_outb. */

static void ax88796_block_input(struct net_device *dev, int count,
				struct sk_buff *skb, int ring_offset)
{
	long nic_base = dev->base_addr;
	char *buf = skb->data;

	/* This *shouldn't* happen. If it does, it's the last thing you'll see */
	if (ei_status.dmaing) {
		printk("%s: DMAing conflict in ax88796_block_input "
			   "[DMAstat:%d][irqlock:%d].\n",
			   dev->name, ei_status.dmaing, ei_status.irqlock);
		return;
	}
	ei_status.dmaing |= 0x01;
	ax_outb(E8390_NODMA+E8390_PAGE0+E8390_START, nic_base+ NE_CMD);
	ax_outb(count & 0xff, nic_base + EN0_RCNTLO);
	ax_outb(count >> 8, nic_base + EN0_RCNTHI);
	ax_outb(ring_offset & 0xff, nic_base + EN0_RSARLO);
	ax_outb(ring_offset >> 8, nic_base + EN0_RSARHI);
	ax_outb(E8390_RREAD+E8390_START, nic_base + NE_CMD);

#ifdef AX88796_DODGY_REMOTEDMA
	udelay(1);
#endif

	insw(NE_BASE + NE_DATAPORT,buf,count>>1);
	if (count & 0x01) {
		buf[count-1] = ax_inb(NE_BASE + NE_DATAPORT);
	}

	ax_outb(ENISR_RDC, nic_base + EN0_ISR);	/* Ack intr. */
	ei_status.dmaing &= ~0x01;
}

static void ax88796_block_output(struct net_device *dev, int count,
				  const unsigned char *buf, const int start_page)
{
	long nic_base = NE_BASE;
	unsigned long dma_start;

	/* On little-endian it's always safe to round the count up for
	   word writes. */
	if (count & 0x01)
		count++;

	/* This *shouldn't* happen. If it does, it's the last thing you'll see */
	if (ei_status.dmaing) {
		printk("%s: DMAing conflict in ax88796_block_output."
			   "[DMAstat:%d][irqlock:%d]\n",
			   dev->name, ei_status.dmaing, ei_status.irqlock);
		return;
	}
	ei_status.dmaing |= 0x01;
	/* We should already be in page 0, but to be safe... */
	ax_outb(E8390_PAGE0 + E8390_START + E8390_NODMA, nic_base + NE_CMD);

#ifdef NE8390_RW_BUGFIX
	/* Handle the read-before-write bug the same way as the
	   Crynwr packet driver -- the NatSemi method doesn't work.
	   Actually this doesn't always work either, but if you have
	   problems with your NEx000 this is better than nothing! */
	ax_outb(0x42, nic_base + EN0_RCNTLO);
	ax_outb(0x00, nic_base + EN0_RCNTHI);
	ax_outb(0x42, nic_base + EN0_RSARLO);
	ax_outb(0x00, nic_base + EN0_RSARHI);
	ax_outb(E8390_RREAD + E8390_START, nic_base + NE_CMD);
#endif
	ax_outb(ENISR_RDC, nic_base + EN0_ISR);

	/* Now the normal output. */
	ax_outb(count & 0xff, nic_base + EN0_RCNTLO);
	ax_outb(count >> 8,   nic_base + EN0_RCNTHI);
	ax_outb(0x00, nic_base + EN0_RSARLO);
	ax_outb(start_page, nic_base + EN0_RSARHI);
	ax_outb(E8390_RWRITE + E8390_START, nic_base + NE_CMD);
	outsw(NE_BASE + NE_DATAPORT, buf, count>>1);

	dma_start = jiffies;

	while ((ax_inb(nic_base + EN0_ISR) & ENISR_RDC) == 0)
		if (jiffies - dma_start > 2) {			/* Avoid clock roll-over. */
			printk(KERN_WARNING "%s: timeout waiting for Tx RDC.\n", dev->name);
			ax88796_reset_8390(dev);
			NS8390_init(dev, 1);
			break;
		}

	ax_outb(ENISR_RDC, nic_base + EN0_ISR);	/* Ack intr. */
	ei_status.dmaing &= ~0x01;
}

static int netdev_ethtool_ioctl(struct net_device *dev, void *useraddr)
{
	struct ei_device *ei = dev->priv;
	struct ax88796_private *priv = (struct ax88796_private *) ei->priv;
	u32 ethcmd;

	if (copy_from_user(&ethcmd, useraddr, sizeof(ethcmd)))
		return -EFAULT;

        switch (ethcmd) {
        case ETHTOOL_GDRVINFO: {
		struct ethtool_drvinfo info = { ETHTOOL_GDRVINFO };
		strcpy(info.driver, DRV_NAME);
		strcpy(info.version, DRV_VERSION);
		strcpy(info.bus_info, AX88796_BUS_INFO);
		if (copy_to_user(useraddr, &info, sizeof(info)))
			return -EFAULT;
		return 0;
	}

	/* get settings */
	case ETHTOOL_GSET: {
		struct ethtool_cmd ecmd = { ETHTOOL_GSET };
		spin_lock_irq(&priv->lock);
		mii_ethtool_gset(&priv->mii_if, &ecmd);
		spin_unlock_irq(&priv->lock);
		if (copy_to_user(useraddr, &ecmd, sizeof(ecmd)))
			return -EFAULT;
		return 0;
	}

	/* set settings */
	case ETHTOOL_SSET: {
		int r;
		struct ethtool_cmd ecmd;
		if (copy_from_user(&ecmd, useraddr, sizeof(ecmd)))
			return -EFAULT;
		spin_lock_irq(&priv->lock);
		r = mii_ethtool_sset(&priv->mii_if, &ecmd);
		spin_unlock_irq(&priv->lock);
		return r;
	}

	/* restart autonegotiation */
	case ETHTOOL_NWAY_RST: {
		return mii_nway_restart(&priv->mii_if);
	}

	/* get link status */
	case ETHTOOL_GLINK: {
		struct ethtool_value edata = { ETHTOOL_GLINK };
		edata.data = mii_link_ok(&priv->mii_if);
		if (copy_to_user(useraddr, &edata, sizeof(edata)))
			return -EFAULT;
		return 0;
	}

        }

	return -EOPNOTSUPP;
}

static int netdev_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct ax88796_private *priv = (struct ax88796_private *) ei_status.priv;
	struct mii_ioctl_data *miodata = (struct mii_ioctl_data *) &rq->ifr_data;
 
	switch (cmd) {
	case SIOCETHTOOL:
		return netdev_ethtool_ioctl(dev, (void *) rq->ifr_data);

	default:
		break;
	}

	return generic_mii_ioctl(&priv->mii_if, miodata, cmd, NULL);
}

static void __exit ax88796_cleanup(void)
{
	if (ax88796_device) {
		unregister_netdev(ax88796_device);
		release_region(ax88796_device->base_addr, NE_IO_EXTENT);
		kfree((void *) ((struct ei_device *)ax88796_device->priv)->priv);
		kfree(ax88796_device->priv);
		kfree(ax88796_device);
	}
#ifdef CONFIG_PM
	if (ax88796_pm_dev)
		pm_unregister(ax88796_pm_dev);
#endif
}

module_init(ax88796_init);
module_exit(ax88796_cleanup);
