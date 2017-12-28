/*
  dm9000.c: Version 1.2 03/18/2003

        A Davicom DM9000 ISA NIC fast Ethernet driver for Linux.
	Copyright (C) 1997  Sten Wang

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.


  (C)Copyright 1997-1998 DAVICOM Semiconductor,Inc. All Rights Reserved.


V0.11	06/20/2001	REG_0A bit3=1, default enable BP with DA match
	06/22/2001 	Support DM9801 progrmming
	 	 	E3: R25 = ((R24 + NF) & 0x00ff) | 0xf000
		 	E4: R25 = ((R24 + NF) & 0x00ff) | 0xc200
		     		R17 = (R17 & 0xfff0) | NF + 3
		 	E5: R25 = ((R24 + NF - 3) & 0x00ff) | 0xc200
		     		R17 = (R17 & 0xfff0) | NF

v1.00               	modify by simon 2001.9.5
                        change for kernel 2.4.x

v1.1   11/09/2001      	fix force mode bug

v1.2   03/18/2003       Weilun Huang <weilun_huang@davicom.com.tw>:
			Fixed phy reset.
			Added tx/rx 32 bit mode.
			Cleaned up for kernel merge.

       22/01/2004	David Howells <dhowells@redhat.com>
			module stuff cleaned up
			I/O functions made inline
			I/O addresses made 32-bits
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/version.h>
#include <linux/spinlock.h>
#include <linux/crc32.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/pm.h>
#include <asm/uaccess.h>
#include <asm/delay.h>
#include <asm/dma.h>
#include <asm/io.h>
#ifdef CONFIG_DM9000_ARCH_CONFIG
#include <asm/dm9000.h>
#endif

MODULE_AUTHOR("Sten Wang, sten_wang@davicom.com.tw");
MODULE_DESCRIPTION("Davicom DM9000 ISA/uP Fast Ethernet Driver");


/* Board/System/Debug information/definition ---------------- */

#define DM9000_ID		0x90000A46

#define DM9000_REG00		0x00
#define DM9000_REG05		0x30	/* SKIP_CRC/SKIP_LONG */
#define DM9000_REG08		0x27
#define DM9000_REG09		0x38
#define DM9000_REG0A		0xff
#define DM9000_REGFF		0x83	/* IMR */

#define DM9000_PHY		0x40	/* PHY address 0x01 */
#define DM9000_PKT_MAX		1536	/* Received packet max size */
#define DM9000_PKT_RDY		0x01	/* Packet ready to receive */
#define DM9000_MIN_IO		0x300
#define DM9000_MAX_IO		0x370
#define DM9000_INT_MII		0x00
#define DM9000_EXT_MII		0x80

#define DM9000_VID_L		0x28
#define DM9000_VID_H		0x29
#define DM9000_PID_L		0x2A
#define DM9000_PID_H		0x2B

#define DM9801_NOISE_FLOOR	0x08
#define DM9802_NOISE_FLOOR	0x05

#define DMFE_SUCC       	0
#define MAX_PACKET_SIZE 	1514
#define DMFE_MAX_MULTICAST 	14

#define DM9000_RX_INTR		0x01
#define DM9000_TX_INTR		0x02
#define DM9000_OVERFLOW_INTR	0x04

#define DM9000_DWORD_MODE	1
#define DM9000_BYTE_MODE	2
#define DM9000_WORD_MODE	0

#define TRUE			1
#define FALSE			0

#define DMFE_TIMER_WUT  (jiffies+(HZ*2))	/* timer wakeup time : 2 second */
#define DMFE_TX_TIMEOUT (HZ*2)			/* tx packet time-out time 1.5 s" */

#if defined(DM9000_DEBUG)
#define DMFE_DBUG(dbug_now, msg, vaule)				\
do {								\
	if (dmfe_debug || dbug_now)				\
		printk(KERN_ERR "dmfe: %s %x\n", msg, vaule);	\
} while(0)
#else
#define DMFE_DBUG(dbug_now, msg, vaule)
#endif

enum DM9000_PHY_mode {
	DM9000_10MHD   = 0,
	DM9000_100MHD  = 1,
	DM9000_10MFD   = 4,
	DM9000_100MFD  = 5,
	DM9000_AUTO    = 8,
	DM9000_1M_HPNA = 0x10
};

enum DM9000_NIC_TYPE {
	FASTETHER_NIC = 0,
	HOMERUN_NIC   = 1,
	LONGRUN_NIC   = 2
};

static const char *nic_types[] = { "FastEther", "HomeRun", "LongRun" };

/* Structure/enum declaration ------------------------------- */
typedef struct board_info {

	u32 runt_length_counter;	/* counter: RX length < 64byte */
	u32 long_length_counter;	/* counter: RX length > 1514byte */
	u32 reset_counter;		/* counter: RESET */
	u32 reset_tx_timeout;		/* RESET caused by TX Timeout */
	u32 reset_rx_status;		/* RESET caused by RX Statsus wrong */

	unsigned long ioaddr;		/* Register I/O base address */
	unsigned long io_data;		/* Data I/O address */
	int irq;			/* IRQ */

	u16 tx_pkt_cnt;
	u16 queue_pkt_len;
	u16 queue_start_addr;
	u16 dbug_cnt;
	u8 reg0, reg5, reg8, reg9, rega;/* registers saved */
	u8 op_mode;			/* PHY operation mode */
	u8 io_mode;			/* 0:word, 2:byte */
	u8 phy_addr;
	u8 link_failed;			/* Ever link failed */
	u8 device_wait_reset;		/* device state */
	u8 nic_type;			/* NIC type */
	u8 prev_tx_pkt;                 /* which packet was sent last (0/1) */
	struct mii_if_info mii_if;
	struct timer_list timer;
	struct net_device_stats stats;
	uint16_t srom[64];
	spinlock_t lock;
} board_info_t;

/* Global variable declaration ----------------------------- */
static int dm9000_debug;
static struct net_device * dm9000_dev;
#ifdef CONFIG_PM
static struct pm_dev *dm9000_pm_dev;
#endif

/* For module input parameter */
static int debug      = 0;
static int mode       = DM9000_AUTO;
static int media_mode = DM9000_AUTO;
static u8 reg5 	      = DM9000_REG05;
static u8 reg8 	      = DM9000_REG08;
static u8 reg9 	      = DM9000_REG09;
static u8 rega 	      = DM9000_REG0A;
static u8 nfloor      = 0;
#ifndef CONFIG_DM9000_ARCH_CONFIG
static u8 irqline     = 3;
#endif

MODULE_PARM(debug, "i");
MODULE_PARM(mode, "i");
MODULE_PARM(reg5, "i");
MODULE_PARM(reg9, "i");
MODULE_PARM(rega, "i");
MODULE_PARM(nfloor, "i");

/* function declaration ------------------------------------- */
static int dm9000_open(struct net_device *);
static int dm9000_start_xmit(struct sk_buff *, struct net_device *);
static int dm9000_stop(struct net_device *);
static struct net_device_stats * dm9000_get_stats(struct net_device *);
static int dm9000_do_ioctl(struct net_device *, struct ifreq *, int);
static void dm9000_interrupt(int , void *, struct pt_regs *);
static void dm9000_timer(unsigned long);
static void dm9000_initialise(struct net_device *);
static unsigned long cal_CRC(unsigned char *, unsigned int, u8);
static void dm9000_packet_receive(unsigned long);
static void dm9000_hash_table(struct net_device *);

DECLARE_TASKLET(dm9000_rx_tasklet,dm9000_packet_receive,0);

/*
 *  Read a byte from I/O port
 */
static inline u8 ior(board_info_t *db, int reg)
{
	outb(reg, db->ioaddr);
	return inb(db->io_data);
}

/*
 * Write a byte to I/O port
 */
static inline void iow(board_info_t *db, int reg, u8 value)
{
	outb(reg, db->ioaddr);
	outb(value, db->io_data);
}

/* DM9000 network board routine ---------------------------- */


/*
 * Read a word datum from SROM
 */
static uint16_t read_srom_word(board_info_t *db, int offset)
{
	iow(db, 0xc, offset);
	iow(db, 0xb, 0x4);
	while (ior(db, 0xb) & 0x01)
		continue;
	udelay(200);
	iow(db, 0xb, 0x0);
	return ior(db, 0xd) + (ior(db, 0xe) << 8);
}

/*
 * Write a word datum to SROM
 */
static void write_srom_word(board_info_t *db, int offset, uint16_t datum)
{
	iow(db, 0xe, datum >> 8);
	ior(db, 0xe);
	iow(db, 0xd, datum);
	ior(db, 0xd);
	iow(db, 0xc, offset);
	ior(db, 0xc);
	iow(db, 0xb, 0x10);
	ior(db, 0xb);
	iow(db, 0xb, 0x12);
	ior(db, 0xb);
	while (ior(db, 0xb) & 0x01)
		continue;
	udelay(200);
	iow(db, 0xb, 0x0);
	ior(db, 0xb);
	while (ior(db, 0xb) & 0x01)
		continue;
	udelay(150);
}

/*
 * ask the NIC to reload its registers from the SROM
 */
static void reload_srom(board_info_t *db)
{
	iow(db, 0xb, 0x20);
	while (ior(db, 0xb) & 0x01)
		continue;
	udelay(200);
	iow(db, 0xb, 0x0);
	ior(db, 0xb);
}

/*
 * Read a word from phyxcer / MII
 */
static int dm9000_phy_read(struct net_device *dev, int phy_id, int loc)
{
	board_info_t *db = (board_info_t *) dev->priv;

	/* Fill the phyxcer register into REG_0C */
	iow(db, 0xc, DM9000_PHY | loc);

	iow(db, 0xb, 0xc); 	/* Issue phyxcer read command */
	udelay(100);		/* Wait read complete */
	iow(db, 0xb, 0x0); 	/* Clear phyxcer read command */

	/* The read data keeps on REG_0D & REG_0E */
	return (ior(db, 0xe) << 8) | ior(db, 0xd);
}

/*
 * Write a word to phyxcer / MII
 */
static void dm9000_phy_write(struct net_device *dev, int phy_id, int loc, int val)
{
	board_info_t *db = (board_info_t *) dev->priv;

	/* Fill the phyxcer register into REG_0C */
	iow(db, 0xc, DM9000_PHY | loc);

	/* Fill the written data into REG_0D & REG_0E */
	iow(db, 0xd, val & 0xff);
	iow(db, 0xe, (val >> 8) & 0xff);

	iow(db, 0xb, 0xa);		/* Issue phyxcer write command */
	udelay(500);			/* Wait write complete */
	iow(db, 0xb, 0x0);		/* Clear phyxcer write command */
}

/*
 * init the device's MII
 */
static void __devinit dm9000_init_mii(struct net_device *dev)
{
	board_info_t *db = (board_info_t *) dev->priv;
	unsigned ident, bmsr;

	db->mii_if.phy_id	= DM9000_PHY;
	db->mii_if.phy_id_mask	= 0xc0;
	db->mii_if.reg_num_mask	= 0x0;
	db->mii_if.dev		= dev;
	db->mii_if.mdio_read	= dm9000_phy_read;
	db->mii_if.mdio_write	= dm9000_phy_write;

	bmsr = dm9000_phy_read(dev, DM9000_PHY, MII_BMSR);
	ident = dm9000_phy_read(dev, DM9000_PHY, MII_PHYSID1) << 16;
	ident |= dm9000_phy_read(dev, DM9000_PHY, MII_PHYSID2);
	printk("  MII transceiver at index %d, status %x, ident %08x\n",
	       db->mii_if.phy_id, bmsr, ident);
}

/*
 * Init HomeRun DM9801
 */
static void program_dm9801(struct net_device *dev, u16 HPNA_rev)
{
	board_info_t *db = (board_info_t *) dev->priv;
	uint16_t reg16, reg17, reg24, reg25;

	if (!nfloor)
		nfloor = DM9801_NOISE_FLOOR;

	reg16 = dm9000_phy_read(dev, db->mii_if.phy_id, 16);
	reg17 = dm9000_phy_read(dev, db->mii_if.phy_id, 17);
	reg24 = dm9000_phy_read(dev, db->mii_if.phy_id, MII_LBRERROR);
	reg25 = dm9000_phy_read(dev, db->mii_if.phy_id, MII_PHYADDR);

	switch (HPNA_rev) {
	case 0xb900: /* DM9801 E3 */
		reg16 |= 0x1000;
		reg25 = ((reg24 + nfloor) & 0x00ff) | 0xf000;
		break;
	case 0xb901: /* DM9801 E4 */
		reg25 = ((reg24 + nfloor) & 0x00ff) | 0xc200;
		reg17 = (reg17 & 0xfff0) + nfloor + 3;
		break;
	case 0xb902: /* DM9801 E5 */
	case 0xb903: /* DM9801 E6 */
	default:
		reg16 |= 0x1000;
		reg25 = ( (reg24 + nfloor - 3) & 0x00ff) | 0xc200;
		reg17 = (reg17 & 0xfff0) + nfloor;
	}

	dm9000_phy_write(dev, db->mii_if.phy_id, 16, reg16);
	dm9000_phy_write(dev, db->mii_if.phy_id, 17, reg17);
	dm9000_phy_write(dev, db->mii_if.phy_id, MII_PHYADDR, reg25);
}

/*
 * Init LongRun DM9802
 */
static void program_dm9802(struct net_device *dev)
{
	board_info_t *db = (board_info_t *) dev->priv;
	uint16_t reg25;

	if (!nfloor)
		nfloor = DM9802_NOISE_FLOOR;

	reg25 = dm9000_phy_read(dev, db->mii_if.phy_id, MII_PHYADDR);
	reg25 = (reg25 & 0xff00) + nfloor;
	dm9000_phy_write(dev, db->mii_if.phy_id, MII_PHYADDR, reg25);
}

/*
 * Identify NIC type
 */
static void identify_nic(struct net_device *dev)
{
	board_info_t *db = (board_info_t *) dev->priv;
	uint16_t reg3;

	iow(db, 0, DM9000_EXT_MII);
	reg3 = dm9000_phy_read(dev, db->mii_if.phy_id, MII_PHYSID2);

	switch (reg3 & 0xfff0) {
	case 0xb900:
		if (dm9000_phy_read(dev, db->mii_if.phy_id, 31) == 0x4404) {
			db->nic_type = HOMERUN_NIC;
			program_dm9801(dev, reg3);
		}
		else {
			db->nic_type = LONGRUN_NIC;
			program_dm9802(dev);
		}
		break;

	default:
		db->nic_type = FASTETHER_NIC;
		break;

	}

	iow(db, 0, DM9000_INT_MII);
}

/*
 * probe a prospective DM9000 board to see if it exists
 */
static int __devinit dm9000_probe_one(struct net_device *dev, unsigned long iobase, int irq)
{
	struct board_info *db;    /* Point a board information structure */
	unsigned long flags;
	u32 id_val;
	int loop, xloop;

	outb(DM9000_VID_L, iobase);
	id_val = inb(iobase + 4);
	outb(DM9000_VID_H, iobase);
	id_val |= inb(iobase + 4) << 8;
	outb(DM9000_PID_L, iobase);
	id_val |= inb(iobase + 4) << 16;
	outb(DM9000_PID_H, iobase);
	id_val |= inb(iobase + 4) << 24;

	if (id_val != DM9000_ID)
		return -ENOENT; /* no board */

	/* Init network device */
	dev = init_etherdev(dev, 0);

	/* Allocated board information structure */
	db = (struct board_info *) kmalloc(sizeof(*db), GFP_KERNEL);
	memset(db, 0, sizeof(*db));
	dev->priv   = db;   /* link device and board info */
	dm9000_dev    = dev;
	db->ioaddr  = iobase;
	db->io_data = iobase + 4;

	/* driver system function */
	ether_setup(dev);

	dev->base_addr 		= iobase;
	dev->irq 		= irq;
	dev->open 		= &dm9000_open;
	dev->hard_start_xmit 	= &dm9000_start_xmit;
	dev->stop 		= &dm9000_stop;
	dev->get_stats 		= &dm9000_get_stats;
	dev->set_multicast_list = &dm9000_hash_table;
	dev->do_ioctl 		= &dm9000_do_ioctl;

	SET_MODULE_OWNER(dev);

	/* read SROM content */
	for (xloop = 0; xloop < 16; xloop++) {
		for (loop = 0; loop < 64; loop++)
			db->srom[loop] = read_srom_word(db, loop);
		if (db->srom[4] != 0 && db->srom[5] != 0)
			break;
	}

	/* program SROM for interrupt output active-low */
	db->srom[3] = (db->srom[3] & ~0x000c) | 0x0004; /* autoload control: enable WORD6[8:0] */
	db->srom[6] &= 0xfe00;				/* clear WORD6[8:0] */
	db->srom[6] |= 0x0006;				/* IOR#/IOW# are active low */
#ifdef DM9000_ARCH_IRQ_ACTLOW
	db->srom[6] = 0x0008; /* enable INT pin, set active low */
#endif

	local_irq_save(flags);
	write_srom_word(db, 6, db->srom[6]);
	write_srom_word(db, 3, db->srom[3]);
	local_irq_restore(flags);

	reload_srom(db);

	/* reread SROM content */
	for (xloop = 0; xloop < 16; xloop++) {
		for (loop = 0; loop < 64; loop++)
			db->srom[loop] = read_srom_word(db, loop);
		if (db->srom[4] != 0 && db->srom[5] != 0)
			break;
	}

	/* set MAC address */
	*(u16 *) &dev->dev_addr[0] = le16_to_cpu(db->srom[0]);
	*(u16 *) &dev->dev_addr[2] = le16_to_cpu(db->srom[1]);
	*(u16 *) &dev->dev_addr[4] = le16_to_cpu(db->srom[2]);

	/* Request IO from system */
	request_region(iobase, 2, dev->name);

	/* detect NIC type: FASTETHER, HOMERUN, LONGRUN */
	identify_nic(dev);

	printk("%s: DM9000 %s found at 0x%lx, IRQ %d, %02x:%02x:%02x:%02x:%02x:%02x\n",
	       dev->name, nic_types[db->nic_type], iobase, irq,
	       dev->dev_addr[0], dev->dev_addr[1], dev->dev_addr[2],
	       dev->dev_addr[3], dev->dev_addr[4], dev->dev_addr[5]);

	dm9000_init_mii(dev);

	if (db->srom[4] != 0xa46 || db->srom[5] != 0x9000)
		printk("  Unexpected ID in EEPROM: %04x:%04x not 0A46:9000\n",
		       db->srom[4], db->srom[5]);

	return 0;
}

/*
 * Search DM9000 board, allocate space and register it
 */
static int __devinit dm9000_probe(struct net_device *dev)
{
#ifndef CONFIG_DM9000_ARCH_CONFIG
	unsigned long iobase;
#endif

	DMFE_DBUG(0, "dm9000_probe()", 0);

#ifndef CONFIG_DM9000_ARCH_CONFIG
	/* Search All DM9000 NIC */
	for (iobase = DM9000_MIN_IO; iobase <= DM9000_MAX_IO; iobase += 0x10)
		if (dm9000_probe_one(dev, iobase, irqline) == 0)
			return 0;

#else
	if (dm9000_probe_one(dev, DM9000_ARCH_IOBASE, DM9000_ARCH_IRQ) == 0)
		return 0;

#endif

	return -ENODEV;
}

/*
 * Open the interface.
 * The interface is opened whenever "ifconfig" actives it.
 */
static int dm9000_open(struct net_device *dev)
{
	board_info_t * db = (board_info_t *)dev->priv;

	DMFE_DBUG(0, "dm9000_open", 0);

	if (request_irq(dev->irq, &dm9000_interrupt, SA_INTERRUPT | SA_SHIRQ, dev->name, dev))
		return -EAGAIN;

	/* Initialise DM910X board */
	dm9000_initialise(dev);

	/* Init driver variable */
	db->dbug_cnt 		= 0;
	db->runt_length_counter = 0;
	db->long_length_counter = 0;
	db->reset_counter 	= 0;

	/* set and active a timer process */
	init_timer(&db->timer);
	db->timer.expires 	= DMFE_TIMER_WUT * 2;
	db->timer.data 		= (unsigned long)dev;
	db->timer.function 	= &dm9000_timer;
	add_timer(&db->timer);

	netif_start_queue(dev);

	return 0;
}

/*
 * Set PHY operationg mode
 */
static void set_PHY_mode(struct net_device *dev)
{
	board_info_t *db = (board_info_t *) dev->priv;
	uint16_t phy_reg4;
	uint16_t phy_reg0;

	phy_reg4 = ADVERTISE_ALL | ADVERTISE_CSMA;
	phy_reg0 = BMCR_ANENABLE;

	if (!(db->op_mode & DM9000_AUTO)) {
		switch (db->op_mode) {
		case DM9000_10MHD:
			phy_reg4 = ADVERTISE_10HALF | ADVERTISE_CSMA;
			phy_reg0 = 0x0000;
			break;
		case DM9000_10MFD:
			phy_reg4 = ADVERTISE_10FULL | ADVERTISE_CSMA;
			phy_reg0 = BMCR_ANENABLE | BMCR_FULLDPLX;
			break;
		case DM9000_100MHD:
			phy_reg4 = ADVERTISE_100HALF | ADVERTISE_CSMA;
			phy_reg0 = BMCR_SPEED100;
			break;
		case DM9000_100MFD:
			phy_reg4 = ADVERTISE_100FULL | ADVERTISE_CSMA;
			phy_reg0 = BMCR_SPEED100 | BMCR_ANENABLE | BMCR_FULLDPLX;
			break;
		}

		dm9000_phy_write(dev, db->mii_if.phy_id, MII_ADVERTISE, phy_reg4);
		dm9000_phy_write(dev, db->mii_if.phy_id, MII_BMCR, phy_reg0);
	}

	iow(db, 0x1e, 0x01);			/* Let GPIO0 output */
	iow(db, 0x1f, 0x00);			/* Enable PHY */
}

/*
 * Initialise dm9000 board upon open
 */
static void dm9000_initialise(struct net_device *dev)
{
	board_info_t *db = (board_info_t *) dev->priv;

	DMFE_DBUG(0, "dm9000_initialise()", 0);

	/* RESET device */
	iow(db, 0, 1);
	udelay(100);			  /* delay 100us */

	/* I/O mode */
	db->io_mode = ior(db, 0xfe) >> 6; /* ISR bit7:6 keeps I/O mode */

	/* GPIO0 on pre-activate PHY */
	iow(db,0x1f,0x00); 		/*REG_1F bit0 activate phyxcer*/

	/* Set PHY */
	db->op_mode = media_mode;
	set_PHY_mode(dev);

	/* Init needed register value */
	db->reg0 = DM9000_REG00;
	if (db->nic_type != FASTETHER_NIC && db->op_mode & DM9000_1M_HPNA)
		db->reg0 |= DM9000_EXT_MII;

	/* User passed argument */
	db->reg5 = reg5;
	db->reg8 = reg8;
	db->reg9 = reg9;
	db->rega = rega;

	/* Program operating register */
	iow(db, 0x00, db->reg0);
	iow(db, 0x02, 0);		/* TX Polling clear */
	iow(db, 0x08, 0x3f);		/* Less 3Kb, 200us */
	iow(db, 0x09, db->reg9);	/* Flow Control : High/Low Water */
	iow(db, 0x0a, db->rega);	/* Flow Control */
	iow(db, 0x2f, 0);		/* Special Mode */
	iow(db, 0x01, 0x2c);		/* clear TX status */
	iow(db, 0xfe, 0x0f); 		/* Clear interrupt status */

	/* Set address filter table */
	dm9000_hash_table(dev);

	/* Activate DM9000 */
	iow(db, 0x05, db->reg5 | 1);	/* RX enable */
	iow(db, 0xff, DM9000_REGFF); 	/* Enable TX/RX interrupt mask */

	/* Init Driver variable */
	db->link_failed = 1;
	db->tx_pkt_cnt = 0;
	db->prev_tx_pkt = 1;
	db->queue_pkt_len = 0;
	dev->trans_start = 0;
	spin_lock_init(&db->lock);
}

/*
 * Stop the interface.
 * The interface is stopped when it is brought.
 */
static int dm9000_stop(struct net_device *dev)
{
	board_info_t *db = (board_info_t *)dev->priv;

	DMFE_DBUG(0, "dm9000_stop", 0);

	/* deleted timer */
	del_timer(&db->timer);

	netif_stop_queue(dev);

	/* free interrupt */
	free_irq(dev->irq, dev);

	/* RESET devie */
	dm9000_phy_write(dev, db->mii_if.phy_id, MII_BMCR, BMCR_RESET);
	iow(db, 0x1f, 0x01); 		/* Power-Down PHY */
	iow(db, 0xff, 0x80);		/* Disable all interrupt */
	iow(db, 0x05, 0x00);		/* Disable RX */

	/* Dump Statistic counter */
#if FALSE
	printk("\nRX FIFO OVERFLOW %lx\n", db->stats.rx_fifo_errors);
	printk("RX CRC %lx\n", db->stats.rx_crc_errors);
	printk("RX LEN Err %lx\n", db->stats.rx_length_errors);
	printk("RX LEN < 64byte %x\n", db->runt_length_counter);
	printk("RX LEN > 1514byte %x\n", db->long_length_counter);
	printk("RESET %x\n", db->reset_counter);
	printk("RESET: TX Timeout %x\n", db->reset_tx_timeout);
	printk("RESET: RX Status Wrong %x\n", db->reset_rx_status);
#endif

	return 0;
}

/*
 * Hardware start transmission.
 * Send a packet to media from the upper layer.
 */
static int dm9000_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	board_info_t *db = (board_info_t *)dev->priv;
	char * data_ptr;
	int i, tmplen;
	unsigned long flags;

	DMFE_DBUG(0, "dm9000_start_xmit", 0);

	spin_lock_irqsave(&db->lock, flags);

	if (db->tx_pkt_cnt > 1) {
		spin_unlock_irqrestore(&db->lock, flags);
		return 1;
	}

	netif_stop_queue(dev);

#if 0
	printk("Tx r=%02x%02x w=%02x%02x l=%x\n",
	       ior(db, 0x23), ior(db, 0x22),
	       ior(db, 0xfb), ior(db, 0xfa),
	       skb->len);
#endif

	/* Disable all interrupt */
	iow(db, 0xff, 0x80);

	/* Move data to DM9000 TX RAM */
	data_ptr = (char *)skb->data;
	outb(0xf8, db->ioaddr);

	if (db->io_mode == DM9000_BYTE_MODE) {
		/* Byte mode */
		for (i = 0; i < skb->len; i++)
			outb((data_ptr[i] & 0xff), db->io_data);
	} else if (db->io_mode == DM9000_WORD_MODE) {
		/* Word mode */
		tmplen = (skb->len + 1) / 2;
		for (i = 0; i < tmplen; i++)
         		outw(((u16 *)data_ptr)[i], db->io_data);
	} else {
		/* DWord mode */
		tmplen = (skb->len + 3) / 4;
		outsl(db->io_data, data_ptr, tmplen);
	}

	/* TX control: First packet immediately send, second packet queue */
	if (db->tx_pkt_cnt == 0) {

		/* First Packet */
		db->tx_pkt_cnt++;

		/* Set TX length to DM9000 */
		iow(db, 0xfc, skb->len & 0xff);
		iow(db, 0xfd, (skb->len >> 8) & 0xff);

		/* Issue TX polling command */
		iow(db, 0x2, 0x1);		/* Cleared after TX complete */

		dev->trans_start = jiffies;	/* saved the time stamp */

	} else {
		/* Second packet */
		db->tx_pkt_cnt++;
		db->queue_pkt_len = skb->len;
	}

	/* free this SKB */
	dev_kfree_skb(skb);

	/* Re-enable resource check */
	if (db->tx_pkt_cnt == 1)
              netif_wake_queue(dev);

	/* Re-enable interrupt*/
	iow(db,0xff,0x83);

	spin_unlock_irqrestore(&db->lock, flags);

	return 0;
}

/*
 * DM9102 insterrupt handler
 * receive the packet to upper layer, free the transmitted packet
 */
static void dm9000_tx_done(struct net_device *dev, board_info_t *db)
{
	int net_status = ior(db, 0x01);	/* Got TX status */
	int tx_status;

	if (!(net_status & 0xc)) {
		/*
		 * Sometimes dm9000_tx_done is called with neither status bit set.
		 * In that case, consult prev_tx_pkt to see which pkt completion
		 * bit should have been set.
		 */
		if (db->prev_tx_pkt = !db->prev_tx_pkt)
			net_status |= 8;
		else
			net_status |= 4;
	} else
		db->prev_tx_pkt = (net_status & 8) ? 1 : 0;

	if (net_status & 8)
		tx_status = ior(db, 4);
	else
		tx_status = ior(db, 3);

	/* One packet sent complete */
	db->tx_pkt_cnt--;
	dev->trans_start = 0;

	if (tx_status & 0x08) {
		db->stats.collisions++;
	}
	unsigned char txpar = ior(db, 0x22);
	if (txpar & 3) {
		/* TRPAL unaligned. Need to reset NIC. */
		printk(KERN_NOTICE "DM9000 NIC bug detected. Resetting NIC.\n");
		printk(KERN_NOTICE "TX status %02x\n", tx_status);

		db->stats.tx_errors++;
#if 0
		netif_stop_queue(dev);
		db->reset_counter++;
		db->device_wait_reset = 0;
		dev->trans_start = 0;
		dm9000_initialise(dev);
		netif_wake_queue(dev);
#else
		db->device_wait_reset = 1;
#endif
		return;
	}
		

	db->stats.tx_packets++;

	/* Queue packet check & send */
	if (db->tx_pkt_cnt > 0) {
		iow(db, 0xfc, db->queue_pkt_len & 0xff);
		iow(db, 0xfd, (db->queue_pkt_len >> 8) & 0xff);
		iow(db, 0x2, 0x1);
		dev->trans_start = jiffies;
	}

	netif_wake_queue(dev);
}

static void dm9000_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct net_device *dev = dev_id;
	board_info_t *db;
	int int_status;
	u8 reg_save;

	DMFE_DBUG(0, "dm9000_interrupt()", 0);

	if (!dev) {
		DMFE_DBUG(1, "dm9000_interrupt() without DEVICE arg", 0);
		return;
	}

	/* A real interrupt coming */
	db = (board_info_t *) dev->priv;
	spin_lock(&db->lock);

	/* Save previous register address */
	reg_save = inb(db->ioaddr);

	/* Disable all interrupt */
	iow(db, 0xff, 0x80);

	/* Got DM9000 interrupt status */
	int_status = ior(db, 0xfe);		/* Got ISR */
	iow(db, 0xfe, int_status);		/* Clear ISR status */

	/* Received the coming packet */
	if (int_status & DM9000_RX_INTR)
		tasklet_schedule(&dm9000_rx_tasklet);

	/* Transmit Interrupt check */
	if (int_status & DM9000_TX_INTR)
		dm9000_tx_done(dev, db);

	/* Re-enable interrupt mask */
	iow(db, 0xff, 0x83);

	/* Restore previous register address */
	outb(reg_save, db->ioaddr);

	spin_unlock(&db->lock);
}

/*
 * Get statistics from driver.
 */
static struct net_device_stats *dm9000_get_stats(struct net_device *dev)
{
	board_info_t *db = (board_info_t *) dev->priv;

	DMFE_DBUG(0, "dm9000_get_stats", 0);
	return &db->stats;
}

/*
 * deal with ethtool
 */
static int netdev_ethtool_ioctl(struct net_device *dev, void *useraddr)
{
	board_info_t *db = (board_info_t *) dev->priv;
	uint32_t ethcmd;

	if (copy_from_user(&ethcmd, useraddr, sizeof(ethcmd)))
		return -EFAULT;

        switch (ethcmd) {
        case ETHTOOL_GDRVINFO: {
		struct ethtool_drvinfo info = { ETHTOOL_GDRVINFO };
		strcpy(info.driver, "DM9000");
		strcpy(info.version, "1.2");
#ifndef DM9000_ARCH_BUS_INFO
		sprintf(info.bus_info, "isa.%lx", dev->ioaddr);
#else
		strcpy(info.bus_info, DM9000_ARCH_BUS_INFO);
#endif
		if (copy_to_user(useraddr, &info, sizeof(info)))
			return -EFAULT;
		return 0;
	}

	/* get settings */
	case ETHTOOL_GSET: {
		struct ethtool_cmd ecmd = { ETHTOOL_GSET };
		spin_lock_irq(&db->lock);
		mii_ethtool_gset(&db->mii_if, &ecmd);
		spin_unlock_irq(&db->lock);
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
		spin_lock_irq(&db->lock);
		r = mii_ethtool_sset(&db->mii_if, &ecmd);
		spin_unlock_irq(&db->lock);
		return r;
	}

	/* restart autonegotiation */
	case ETHTOOL_NWAY_RST: {
		return mii_nway_restart(&db->mii_if);
	}

	/* get link status */
	case ETHTOOL_GLINK: {
		struct ethtool_value edata = { ETHTOOL_GLINK };
		edata.data = mii_link_ok(&db->mii_if);
		if (copy_to_user(useraddr, &edata, sizeof(edata)))
			return -EFAULT;
		return 0;
	}

        }

	return -EOPNOTSUPP;
}

/*
 * Process the upper socket ioctl command
 */
static int dm9000_do_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	board_info_t *db = (board_info_t *) dev->priv;
	struct mii_ioctl_data *miodata = (struct mii_ioctl_data *) &rq->ifr_data;

	DMFE_DBUG(0, "dm9000_do_ioctl()", 0);

	switch (cmd) {
	case SIOCETHTOOL:
		return netdev_ethtool_ioctl(dev, (void *) rq->ifr_data);

	default:
		break;
	}

	return generic_mii_ioctl(&db->mii_if, miodata, cmd, NULL);
}

/*
 * A periodic timer routine
 * Dynamic media sense, allocated Rx buffer...
 */
static void dm9000_timer(unsigned long data)
{
	struct net_device *dev = (struct net_device *) data;
	board_info_t *db = (board_info_t *) dev->priv;
	u8 reg_save, tmp_reg;

	DMFE_DBUG(0, "dm9000_timer()", 0);

	/* Save previous register address */
	reg_save = inb(db->ioaddr);

	/* TX timeout check */
	if (dev->trans_start && jiffies - dev->trans_start > DMFE_TX_TIMEOUT) {
		db->device_wait_reset = 1;
		db->reset_tx_timeout++;
	}

	/* DM9000 dynamic RESET check and do */
	if (db->device_wait_reset) {
		netif_stop_queue(dev);
		db->reset_counter++;
		db->device_wait_reset = 0;
		dev->trans_start = 0;
		dm9000_initialise(dev);
		netif_wake_queue(dev);
	}

	/* Auto Sense Media mode policy:
		FastEthernet NIC: don't need to do anything.
		Media Force mode: don't need to do anything.
		HomeRun/LongRun NIC and AUTO_Mode:
			INT_MII not link, select EXT_MII
			EXT_MII not link, select INT_MII
	 */
	if (db->nic_type != FASTETHER_NIC && db->op_mode & DM9000_AUTO) {
		tmp_reg = ior(db, 0x01);	/* Got link status */
		if (!(tmp_reg & 0x40)) { 	/* not link */
			db->reg0 ^= 0x80;
			iow(db, 0x00, db->reg0);
		}
	}

	/* Restore previous register address */
	outb(reg_save, db->ioaddr);

	/* Set timer again */
	db->timer.expires = DMFE_TIMER_WUT;
	add_timer(&db->timer);
}

/*
 * Received a packet and pass to upper layer
 */
static void dm9000_packet_receive(unsigned long unused)
{
	struct net_device *dev = dm9000_dev;
	board_info_t *db = (board_info_t *) dev->priv;
	struct sk_buff *skb;
	u8 rxbyte, *rdptr;
	u16 i, RxStatus, RxLen, GoodPacket, tmplen;
	u32 tmpdata;

	/* Check packet ready or not */
	do {
		ior(db, 0xf0);			/* Dummy read */
		rxbyte = inb(db->io_data);	/* Got most updated data */

		/* Status check: this byte must be 0 or 1 */
		if (rxbyte > DM9000_PKT_RDY) {
			iow(db, 0x05, 0x00);	/* Stop Device */
			iow(db, 0xfe, 0x80);	/* Stop INT request */
			db->device_wait_reset = TRUE;
			db->reset_rx_status++;
		}

		/* packet ready to receive check */
		if (rxbyte == DM9000_PKT_RDY) {
			/* A packet ready now  & Get status/length */
			GoodPacket = TRUE;
			outb(0xf2, db->ioaddr);

			if (db->io_mode == DM9000_BYTE_MODE) {
				/* Byte mode */
				RxStatus = inb(db->io_data) + (inb(db->io_data) << 8);
				RxLen    = inb(db->io_data) + (inb(db->io_data) << 8);
			} else if (db->io_mode == DM9000_WORD_MODE) {
				/* Word mode */
				RxStatus = inw(db->io_data);
				RxLen    = inw(db->io_data);
			} else {
				/* DWord mode */
				tmpdata  = inl(db->io_data);
				RxStatus = tmpdata;
				RxLen	 = tmpdata >> 16;
			}

			/* Packet Status check */
			if (RxLen < 0x40) {
				GoodPacket = FALSE;
				db->runt_length_counter++;
			}

			if (RxLen > DM9000_PKT_MAX) {
				printk("<DM9000> RST: RX Len:%x\n", RxLen);
				db->device_wait_reset = TRUE;
				db->long_length_counter++;
			}

			if (RxStatus & 0xbf00) {
				GoodPacket = FALSE;
				if (RxStatus & 0x100)
					db->stats.rx_fifo_errors++;
				if (RxStatus & 0x200)
					db->stats.rx_crc_errors++;
				if (RxStatus & 0x8000)
					db->stats.rx_length_errors++;
			}

			/* Move data from DM9000 */
			if (!db->device_wait_reset) {
				if (GoodPacket && ((skb = dev_alloc_skb(RxLen + 4)) != NULL)) {
					skb->dev = dev;
					skb_reserve(skb, 2);
					rdptr = (u8 *) skb_put(skb, RxLen-4);

					/* Read received packet from RX SARM */
					if (db->io_mode == DM9000_BYTE_MODE) {
						/* Byte mode */
						for (i=0; i<RxLen; i++)
							rdptr[i]=inb(db->io_data);
					} else if (db->io_mode == DM9000_WORD_MODE) {
						/* Word mode */
						tmplen = (RxLen + 1) / 2;
						for (i = 0; i < tmplen; i++)
							((u16 *)rdptr)[i] = inw(db->io_data);
					} else {
						/* DWord mode */
						tmplen = (RxLen + 3) / 4;
						insl(db->io_data, rdptr, tmplen);
					}

					/* Pass to upper layer */
					skb->protocol = eth_type_trans(skb,dev);
					netif_rx(skb);
					db->stats.rx_packets++;

				} else {
					/* Without buffer or error packet */
					if (db->io_mode == DM9000_BYTE_MODE) {
						/* Byte mode */
						for (i = 0; i < RxLen; i++)
							inb(db->io_data);
					} else if (db->io_mode == DM9000_WORD_MODE) {
						/* Word mode */
						printk("0");
						tmplen = (RxLen + 1) / 2;
						for (i = 0; i < tmplen; i++)
							inw(db->io_data);
					} else {
						/* DWord mode */
						tmplen = (RxLen + 3) / 4;
						for (i = 0; i < tmplen; i++)
							inl(db->io_data);
					}
				}
			}
		}
	} while(rxbyte == DM9000_PKT_RDY && !db->device_wait_reset);
}

/*
 * Set DM9000 multicast address
 */
static void dm9000_hash_table(struct net_device *dev)
{
	board_info_t *db = (board_info_t *) dev->priv;
	struct dev_mc_list *mcptr = dev->mc_list;
	int mc_cnt = dev->mc_count;
	u32 hash_val;
	u16 i, oft, hash_table[4];

	DMFE_DBUG(0, "dm9000_hash_table()", 0);

	/* Set Node address */
	for (i = 0, oft = 0x10; i < 6; i++, oft++)
		iow(db, oft, dev->dev_addr[i]);

	/* Clear Hash Table */
	for (i = 0; i < 4; i++)
		hash_table[i] = 0x0;

	/* broadcast address */
	hash_table[3] = 0x8000;

	/* the multicast address in Hash Table : 64 bits */
	for (i = 0; i < mc_cnt; i++, mcptr = mcptr->next) {
		hash_val = cal_CRC((char *) mcptr->dmi_addr, 6, 0) & 0x3f;
		hash_table[hash_val / 16] |= (u16) 1 << (hash_val % 16);
	}

	/* Write the hash table to MAC MD table */
	for (i = 0, oft = 0x16; i < 4; i++) {
		iow(db, oft++, hash_table[i] & 0xff);
		iow(db, oft++, (hash_table[i] >> 8) & 0xff);
	}
}

/*
 * Calculate the CRC valude of the Rx packet
 * flag = 1 : return the reverse CRC (for the received packet CRC)
 *        0 : return the normal CRC (for Hash Table index)
 */
static unsigned long cal_CRC(unsigned char * Data, unsigned int Len, u8 flag)
{

	u32 crc = ether_crc_le(Len, Data);

	if (flag)
		return ~crc;

	return crc;

}

#ifdef CONFIG_PM
static int dm9000_pm_callback(struct pm_dev *pdev, pm_request_t rqst, void *data)
{
	struct net_device *dev = dm9000_dev;
	board_info_t *db = (board_info_t *) dev->priv;
	
	if (rqst == PM_SUSPEND) {
		/* deleted timer */
		del_timer(&db->timer);

		netif_stop_queue(dev);

		/* RESET devie */
		dm9000_phy_write(dev, db->mii_if.phy_id, MII_BMCR, BMCR_RESET);
		iow(db, 0x1f, 0x01); 		/* Power-Down PHY */
		iow(db, 0xff, 0x80);		/* Disable all interrupt */
		iow(db, 0x05, 0x00);		/* Disable RX */
	} else {
		/* Initialise DM910X board */
		dm9000_initialise(dev);

		/* Init driver variable */
		db->dbug_cnt 		= 0;
		db->runt_length_counter = 0;
		db->long_length_counter = 0;
		db->reset_counter 	= 0;

		/* set and active a timer process */
		init_timer(&db->timer);
		db->timer.expires 	= DMFE_TIMER_WUT * 2;
		db->timer.data 		= (unsigned long)dev;
		db->timer.function 	= &dm9000_timer;
		add_timer(&db->timer);

		netif_start_queue(dev);
	}
	return 0;
}
#endif
/*
 * search for DM9000 adapters and register any found
 */
static int __devinit dm9000_init(void)
{
	int ret;

	DMFE_DBUG(0, "dm9000_init() ", debug);

	if (debug)
		dm9000_debug = debug;   /* set debug flag */

	switch (mode) {
	case DM9000_10MHD:
	case DM9000_100MHD:
	case DM9000_10MFD:
	case DM9000_100MFD:
	case DM9000_1M_HPNA:
		media_mode = mode;
		break;
	default:
		media_mode = DM9000_AUTO;
	}

	nfloor = (nfloor > 15) ? 0 : nfloor;

	/* search for a board */
	ret = dm9000_probe(NULL);

#ifdef CONFIG_PM
	if (!ret)
		dm9000_pm_dev = pm_register(PM_UNKNOWN_DEV, 0, dm9000_pm_callback);
#endif
	return ret;
}

/*
 * unregister a registered DM9000 device and dispose of allocated resources
 */
static void __exit dm9000_cleanup(void)
{
	board_info_t *db;

	DMFE_DBUG(0, "dm9000_clean()", 0);

#ifdef CONFIG_PM
	if (dm9000_pm_dev) {
		pm_unregister(dm9000_pm_dev);
		dm9000_pm_dev = NULL;
	}
#endif
	unregister_netdev(dm9000_dev);
	db = (board_info_t *) dm9000_dev->priv;
	release_region(dm9000_dev->base_addr, 2);
	kfree(db);		/* free board information */
	kfree(dm9000_dev);	/* free device structure */

	DMFE_DBUG(0, "clean_module() exit", 0);
}

module_init(dm9000_init);
module_exit(dm9000_cleanup);
