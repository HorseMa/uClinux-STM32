/*
  dm9000.c: Version 0.11 06/20/2001

        A Davicom DM9000 ISA NIC fast Ethernet driver for Linux.
	Copyright (C) 1997  Sten Wang
	Copyright (C) 2003  Simtec Electronics <ben@simtec.co.uk>

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

  Author: Sten Wang, 886-3-5798797-8517, E-mail: sten_wang@davicom.com.tw
  Date:   10/28,1998

  BAST / Linux 2.4 modifications by Ben Dooks, (C) 2003 Simtec Electronics

  (C)Copyright 1997-1998 DAVICOM Semiconductor,Inc. All Rights Reserved.


V0.11	06/20/2001	REG_0A bit3=1, default enable BP with DA match
	06/22/2001 	Support DM9801 progrmming
	 	 	E3: R25 = ((R24 + NF) & 0x00ff) | 0xf000
		 	E4: R25 = ((R24 + NF) & 0x00ff) | 0xc200
		     		R17 = (R17 & 0xfff0) | NF + 3
		 	E5: R25 = ((R24 + NF - 3) & 0x00ff) | 0xc200
		     		R17 = (R17 & 0xfff0) | NF
*/
#if defined(MODVERSIONS)
#include <linux/modversions.h>
#endif

#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/delay.h>

#include <asm/dma.h>

/* Board/System/Debug information/definition ---------------- */

#define DM9000_ID	0x90000A46

#define DM9000_REG00	0x00
#define DM9000_REG05	0x30	/* SKIP_CRC/SKIP_LONG */
#define DM9000_REG08	0x27
#define DM9000_REG09	0x38
#define DM9000_REG0A	0x08
#define DM9000_REGFF	0x83	/* IMR */

#define DM9000_PHY	0x40	/* PHY address 0x01 */
#define DM9000_PKT_MAX	1536	/* Received packet max size */
#define DM9000_PKT_RDY	0x01	/* Packet ready to receive */
#define DM9000_MIN_IO	0x300
#define DM9000_MAX_IO	0x370
#define DM9000_INT_MII	0x00
#define DM9000_EXT_MII	0x80

#define DM9000_VID_L	0x28
#define DM9000_VID_H	0x29
#define DM9000_PID_L	0x2A
#define DM9000_PID_H	0x2B

#define DM9801_NOISE_FLOOR	0x08
#define DM9802_NOISE_FLOOR	0x05

#define DMFE_SUCC       0
#define MAX_PACKET_SIZE 1514
#define DMFE_MAX_MULTICAST 14

#define DMFE_TIMER_WUT  jiffies+(HZ*2)	/* timer wakeup time : 2 second */
#define DMFE_TX_TIMEOUT (HZ*2)	/* tx packet time-out time 1.5 s" */

//#define DM9000_DEBUG

#if defined(DM9000_DEBUG)
#define DMFE_DBUG(dbug_now, msg, vaule) if (dmfe_debug || dbug_now) printk(KERN_ERR "dmfe: %s %x\n", msg, vaule)
#else
#define DMFE_DBUG(dbug_now, msg, vaule)
#endif

#if LINUX_VERSION_CODE < 0x20300
#define DEVICE device
#else
#define DEVICE net_device
#endif

enum DM9000_PHY_mode {
	DM9000_10MHD = 0, DM9000_100MHD = 1, DM9000_10MFD = 4,
	DM9000_100MFD = 5, DM9000_AUTO = 8, DM9000_1M_HPNA =0x10 };

enum DM9000_NIC_TYPE {
	FASTETHER_NIC = 0, HOMERUN_NIC = 1, LONGRUN_NIC = 2 };

#if 1
#define  enet_statistics net_device_stats
#endif

/* Structure/enum declaration ------------------------------- */
typedef struct board_info {
	struct DEVICE *next_dev;	/* next device */

	u32 runt_length_counter;	/* counter: RX length < 64byte */
	u32 long_length_counter;	/* counter: RX length > 1514byte */
	u32 reset_counter;		/* counter: RESET */
	u32 reset_tx_timeout;		/* RESET caused by TX Timeout */
	u32 reset_rx_status;		/* RESET caused by RX Statsus wrong */

	void (*inblk)(unsigned long ioaddr, unsigned short *dp, u32 count);
	void (*outblk)(unsigned long ioaddr, unsigned short *dp, u32 count);

	u32 ioaddr;			/* Register I/O base address */
	u32 io_data;			/* Data I/O address */
	u16 irq;			/* IRQ */

	u16 tx_pkt_cnt;
	u16 queue_pkt_len;
	u16 queue_start_addr;
	u16 dbug_cnt;
	u8 reg0, reg5, reg8, reg9, rega;	/* registers saved */
	u8 op_mode;			/* PHY operation mode */
	u8 io_mode;			/* 0:word, 2:byte */
	u8 phy_addr;
	u8 link_failed;			/* Ever link failed */
	u8 device_wait_reset;		/* device state */
	u8 nic_type;			/* NIC type */
	struct timer_list timer;
	struct enet_statistics stats;	/* statistic counter */
	unsigned char srom[128];
} board_info_t;

/* Global variable declaration ----------------------------- */
static int dmfe_debug = 1;
static struct DEVICE * dmfe_root_dev = NULL;      /* First device */

/* For module input parameter */
static int debug = 0;
static int mode = DM9000_AUTO;
static int media_mode = DM9000_AUTO;
static u8 reg5 = DM9000_REG05;
static u8 reg8 = DM9000_REG08;
static u8 reg9 = DM9000_REG09;
static u8 rega = DM9000_REG0A;
static u8 nfloor = 0;

unsigned long CrcTable[256] = {
   0x00000000L, 0x77073096L, 0xEE0E612CL, 0x990951BAL,
   0x076DC419L, 0x706AF48FL, 0xE963A535L, 0x9E6495A3L,
   0x0EDB8832L, 0x79DCB8A4L, 0xE0D5E91EL, 0x97D2D988L,
   0x09B64C2BL, 0x7EB17CBDL, 0xE7B82D07L, 0x90BF1D91L,
   0x1DB71064L, 0x6AB020F2L, 0xF3B97148L, 0x84BE41DEL,
   0x1ADAD47DL, 0x6DDDE4EBL, 0xF4D4B551L, 0x83D385C7L,
   0x136C9856L, 0x646BA8C0L, 0xFD62F97AL, 0x8A65C9ECL,
   0x14015C4FL, 0x63066CD9L, 0xFA0F3D63L, 0x8D080DF5L,
   0x3B6E20C8L, 0x4C69105EL, 0xD56041E4L, 0xA2677172L,
   0x3C03E4D1L, 0x4B04D447L, 0xD20D85FDL, 0xA50AB56BL,
   0x35B5A8FAL, 0x42B2986CL, 0xDBBBC9D6L, 0xACBCF940L,
   0x32D86CE3L, 0x45DF5C75L, 0xDCD60DCFL, 0xABD13D59L,
   0x26D930ACL, 0x51DE003AL, 0xC8D75180L, 0xBFD06116L,
   0x21B4F4B5L, 0x56B3C423L, 0xCFBA9599L, 0xB8BDA50FL,
   0x2802B89EL, 0x5F058808L, 0xC60CD9B2L, 0xB10BE924L,
   0x2F6F7C87L, 0x58684C11L, 0xC1611DABL, 0xB6662D3DL,
   0x76DC4190L, 0x01DB7106L, 0x98D220BCL, 0xEFD5102AL,
   0x71B18589L, 0x06B6B51FL, 0x9FBFE4A5L, 0xE8B8D433L,
   0x7807C9A2L, 0x0F00F934L, 0x9609A88EL, 0xE10E9818L,
   0x7F6A0DBBL, 0x086D3D2DL, 0x91646C97L, 0xE6635C01L,
   0x6B6B51F4L, 0x1C6C6162L, 0x856530D8L, 0xF262004EL,
   0x6C0695EDL, 0x1B01A57BL, 0x8208F4C1L, 0xF50FC457L,
   0x65B0D9C6L, 0x12B7E950L, 0x8BBEB8EAL, 0xFCB9887CL,
   0x62DD1DDFL, 0x15DA2D49L, 0x8CD37CF3L, 0xFBD44C65L,
   0x4DB26158L, 0x3AB551CEL, 0xA3BC0074L, 0xD4BB30E2L,
   0x4ADFA541L, 0x3DD895D7L, 0xA4D1C46DL, 0xD3D6F4FBL,
   0x4369E96AL, 0x346ED9FCL, 0xAD678846L, 0xDA60B8D0L,
   0x44042D73L, 0x33031DE5L, 0xAA0A4C5FL, 0xDD0D7CC9L,
   0x5005713CL, 0x270241AAL, 0xBE0B1010L, 0xC90C2086L,
   0x5768B525L, 0x206F85B3L, 0xB966D409L, 0xCE61E49FL,
   0x5EDEF90EL, 0x29D9C998L, 0xB0D09822L, 0xC7D7A8B4L,
   0x59B33D17L, 0x2EB40D81L, 0xB7BD5C3BL, 0xC0BA6CADL,
   0xEDB88320L, 0x9ABFB3B6L, 0x03B6E20CL, 0x74B1D29AL,
   0xEAD54739L, 0x9DD277AFL, 0x04DB2615L, 0x73DC1683L,
   0xE3630B12L, 0x94643B84L, 0x0D6D6A3EL, 0x7A6A5AA8L,
   0xE40ECF0BL, 0x9309FF9DL, 0x0A00AE27L, 0x7D079EB1L,
   0xF00F9344L, 0x8708A3D2L, 0x1E01F268L, 0x6906C2FEL,
   0xF762575DL, 0x806567CBL, 0x196C3671L, 0x6E6B06E7L,
   0xFED41B76L, 0x89D32BE0L, 0x10DA7A5AL, 0x67DD4ACCL,
   0xF9B9DF6FL, 0x8EBEEFF9L, 0x17B7BE43L, 0x60B08ED5L,
   0xD6D6A3E8L, 0xA1D1937EL, 0x38D8C2C4L, 0x4FDFF252L,
   0xD1BB67F1L, 0xA6BC5767L, 0x3FB506DDL, 0x48B2364BL,
   0xD80D2BDAL, 0xAF0A1B4CL, 0x36034AF6L, 0x41047A60L,
   0xDF60EFC3L, 0xA867DF55L, 0x316E8EEFL, 0x4669BE79L,
   0xCB61B38CL, 0xBC66831AL, 0x256FD2A0L, 0x5268E236L,
   0xCC0C7795L, 0xBB0B4703L, 0x220216B9L, 0x5505262FL,
   0xC5BA3BBEL, 0xB2BD0B28L, 0x2BB45A92L, 0x5CB36A04L,
   0xC2D7FFA7L, 0xB5D0CF31L, 0x2CD99E8BL, 0x5BDEAE1DL,
   0x9B64C2B0L, 0xEC63F226L, 0x756AA39CL, 0x026D930AL,
   0x9C0906A9L, 0xEB0E363FL, 0x72076785L, 0x05005713L,
   0x95BF4A82L, 0xE2B87A14L, 0x7BB12BAEL, 0x0CB61B38L,
   0x92D28E9BL, 0xE5D5BE0DL, 0x7CDCEFB7L, 0x0BDBDF21L,
   0x86D3D2D4L, 0xF1D4E242L, 0x68DDB3F8L, 0x1FDA836EL,
   0x81BE16CDL, 0xF6B9265BL, 0x6FB077E1L, 0x18B74777L,
   0x88085AE6L, 0xFF0F6A70L, 0x66063BCAL, 0x11010B5CL,
   0x8F659EFFL, 0xF862AE69L, 0x616BFFD3L, 0x166CCF45L,
   0xA00AE278L, 0xD70DD2EEL, 0x4E048354L, 0x3903B3C2L,
   0xA7672661L, 0xD06016F7L, 0x4969474DL, 0x3E6E77DBL,
   0xAED16A4AL, 0xD9D65ADCL, 0x40DF0B66L, 0x37D83BF0L,
   0xA9BCAE53L, 0xDEBB9EC5L, 0x47B2CF7FL, 0x30B5FFE9L,
   0xBDBDF21CL, 0xCABAC28AL, 0x53B39330L, 0x24B4A3A6L,
   0xBAD03605L, 0xCDD70693L, 0x54DE5729L, 0x23D967BFL,
   0xB3667A2EL, 0xC4614AB8L, 0x5D681B02L, 0x2A6F2B94L,
   0xB40BBE37L, 0xC30C8EA1L, 0x5A05DF1BL, 0x2D02EF8DL
};

/* function declaration ------------------------------------- */
int dmfe_probe(struct DEVICE *);
static int dmfe_open(struct DEVICE *);
static int dmfe_start_xmit(struct sk_buff *, struct DEVICE *);
static int dmfe_stop(struct DEVICE *);
static struct enet_statistics * dmfe_get_stats(struct DEVICE *);
static int dmfe_do_ioctl(struct DEVICE *, struct ifreq *, int);
static void dmfe_interrupt(int , void *, struct pt_regs *);
static void dmfe_timer(unsigned long);
static void dmfe_init_dm9000(struct DEVICE *);
static unsigned long cal_CRC(unsigned char *, unsigned int, u8);
static u8 ior(board_info_t *, int);
static void iow(board_info_t *, int, u8);
static u16 phy_read(board_info_t *, int);
static void phy_write(board_info_t *, int, u16);
static u16 read_srom_word(board_info_t *, int);
static void dmfe_packet_receive(struct DEVICE *, board_info_t *);
static void dm9000_hash_table(struct DEVICE *);

/* DM9000 network baord routine ---------------------------- */

#include <asm/arch/irqs.h>
#include <asm/io.h>

/*
   Read a byte from I/O port
*/
static inline u8 ior(board_info_t *db, int reg)
{
	__raw_writeb(reg, db->ioaddr);
	return __raw_readb(db->io_data);
}

/*
   Write a byte to I/O port
*/
static inline void iow(board_info_t *db, int reg, u8 value)
{
	__raw_writeb(reg, db->ioaddr);
	__raw_writeb(value, db->io_data);
}

/* get/put blocks to chip */

/* these routines seem to allow an ftp-get at about 1.95MB/sec at 226MHz */

static void dm_bast_outblk(unsigned long addr, unsigned short *dp, u32 len)
{
  __raw_writesw(addr, dp, (len+1)/2);
}

static void dm_bast_inblk(unsigned long addr, unsigned short *dp, u32 len)
{
  __raw_readsw(addr, dp, (len+1)/2);
}

#if 0
/* don't use the fast IO routines atm */

extern void __raw_m8_readsl(void *, void *, int);
extern void __raw_m8_writesl(void *, void *, int);

#define is_aligned(ptr) ((((unsigned long)(ptr)) & 3) == 0)

static void dm_bast_outblk_f(unsigned long addr, unsigned short *dp, u32 len)
{
	if (len & 1)   /* round up packet length to half-word*/
		len++;

	if (!is_aligned(dp)) {
		__raw_writew(*dp, addr);
		dp++;
		len -= 2;
	}

	__raw_m8_writesl((void *)addr, dp, len >> 2);

	/* if we have 2 bytes left, then move them */
	if ((len & 3) > 1) {
		dp += (len / 2);
		__raw_writew(dp[-1], addr);
	}
}

static void dm_bast_inblk_f(unsigned long addr, unsigned short *dp, u32 len)
{
	if (len & 1)
		len++;

	if (!is_aligned(dp)) {
		*dp++ = __raw_readw(addr);
		len -= 2;
	}

	__raw_m8_readsl((void *)addr, dp, len >> 2);

	if (len & 2) {
		dp += len/2;
		dp[-1] = __raw_readw(addr);
	}
}
#else
#define dm_bast_inblk_f dm_bast_inblk
#define dm_bast_outblk_f dm_bast_outblk
#endif


#define OKI_DM9000_BASE (0xF0000000 + (7<<21))
#define IRQ_DM9000 (22)

/*
  Search DM9000 board, allocate space and register it
*/
int dmfe_bast_probe(struct DEVICE *dev)
{
	struct board_info *db;    /* Point a board information structure */
	u32 id_val;
	u32 addr, data;
	u16 i, dm9000_count = 0;
	u8  irqline;

	printk("EB67DIP DM9000 Driver, (c) 2003 Simtec Electronics\n");

	DMFE_DBUG(0, "dmfe_probe()",0);

	/* Search All DM9000 NIC */
	do {
		addr = OKI_DM9000_BASE;
		data = OKI_DM9000_BASE + (1<<18);

		__raw_writeb(DM9000_VID_L, addr);
		id_val = __raw_readb(data);
		__raw_writeb(DM9000_VID_H, addr);
		id_val |= __raw_readb(data) << 8;
		__raw_writeb(DM9000_PID_L, addr);
		id_val |= __raw_readb(data) << 16;
		__raw_writeb(DM9000_PID_H, addr);
		id_val |= __raw_readb(data) << 24;

		printk("DM9000: id=%08x\n", id_val);

		if (id_val == DM9000_ID) {
			dm9000_count++;

			DMFE_DBUG(0, "dmfe_probe(): found",0);

			/* Init network device */
			dev = init_etherdev(dev, 0);

			DMFE_DBUG(0, "dmfe_probe(): done init_etherdev",0);

			/* Allocated board information structure */
			irqline = IRQ_DM9000;

			DMFE_DBUG(0, "dmfe_probe: allopcating bloc...", 0);

			db = (void *)(kmalloc(sizeof(*db), GFP_KERNEL));
			DMFE_DBUG(0, "dmfe_probe(): db: ", db);

			memset(db, 0, sizeof(*db));
			dev->priv = db;	/* link device and board info */
			db->next_dev = dmfe_root_dev;
			dmfe_root_dev = dev;
			db->ioaddr = addr;
			db->io_data = data;

			db->outblk = dm_bast_outblk_f;
			db->inblk = dm_bast_inblk_f;

			/* driver system function */
			dev->base_addr = addr;
			dev->irq = irqline;
			dev->open = &dmfe_open;
			dev->hard_start_xmit = &dmfe_start_xmit;
			dev->stop = &dmfe_stop;
			dev->get_stats = &dmfe_get_stats;
			dev->set_multicast_list = &dm9000_hash_table;
			dev->do_ioctl = &dmfe_do_ioctl;

			DMFE_DBUG(0, "dmfe_probe(): reading SROM",0);

			/* Read SROM content */
			for (i=0; i<64; i++)
				((u16 *)db->srom)[i] = read_srom_word(db, i);

#if 0
			/* Set Node Address */
			for (i=0; i<6; i++)
				dev->dev_addr[i] = db->srom[i];
#else
			for (i = 0; i < 6; i++) {
			  __raw_writeb(i+0x10, addr);
			  dev->dev_addr[i] = __raw_readb(data);
			}
#endif
			DMFE_DBUG(0, "dmfe_probe(): requesting resources\n",0);

			/* Request IO from system */
			request_region(addr, 0x40, dev->name);
			request_region(data, 0x40, dev->name);

			/* Announce found device */
			printk("%s: bast-dm9000 found at %#lx, IRQ %d, ",
				dev->name, addr, dev->irq);

			for(i = 0; i < 6; i++) {
				printk("%2.2X%s", dev->dev_addr[i], i == 5 ? ".\n": ":");
			}

			dev = 0;             /* NULL device */

		} else {
			printk("dm9000: %08x: read ID %08x\n", addr, id_val);
		}
	} while(0);

	return dm9000_count ? 0:-ENODEV;
}

/*
  Open the interface.
  The interface is opened whenever "ifconfig" actives it.
*/
static int dmfe_open(struct DEVICE *dev)
{
	board_info_t * db = (board_info_t *)dev->priv;

	DMFE_DBUG(0, "dmfe_open", 0);

	if (request_irq(dev->irq, &dmfe_interrupt, SA_SHIRQ, dev->name, dev))
		return -EAGAIN;

	/* Initilize DM910X board */
	dmfe_init_dm9000(dev);

	/* Init driver variable */
	db->dbug_cnt = 0;
	db->runt_length_counter = 0;
	db->long_length_counter = 0;
	db->reset_counter = 0;

	/* Active System Interface */
#if 0
	dev->tbusy = 0;	/* Can transmit packet */
	dev->start = 1;	/* interface ready */
#endif
	MOD_INC_USE_COUNT;

	/* set and active a timer process */
	init_timer(&db->timer);
	db->timer.expires = DMFE_TIMER_WUT * 2;
	db->timer.data = (unsigned long)dev;
	db->timer.function = &dmfe_timer;
	add_timer(&db->timer);

	return 0;
}

/* Set PHY operationg mode
*/
static void set_PHY_mode(board_info_t *db)
{
	u16 phy_reg4 = 0x01e1, phy_reg0=0x1000;

	if ( !(db->op_mode & DM9000_AUTO) ) {
		switch(db->op_mode) {
		case DM9000_10MHD: phy_reg4 = 0x21; phy_reg0 = 0x0000; break;
		case DM9000_10MFD: phy_reg4 = 0x41; phy_reg0 = 0x0100; break;
		case DM9000_100MHD: phy_reg4 = 0x81; phy_reg0 = 0x2000; break;
		case DM9000_100MFD: phy_reg4 = 0x101; phy_reg0 = 0x2100; break;
		}
		phy_write(db, 4, phy_reg4);	/* Set PHY media mode */
		phy_write(db, 0, phy_reg0);	/*  Tmp */
	}

	iow(db, 0x1e, 0x01);		/* Let GPIO0 output */
	iow(db, 0x1f, 0x00);		/* Enable PHY */
}

/*
	Init HomeRun DM9801
*/
static void program_dm9801(board_info_t *db, u16 HPNA_rev)
{
	__u16 reg16, reg17, reg24, reg25;

	if ( !nfloor ) nfloor = DM9801_NOISE_FLOOR;

	reg16 = phy_read(db, 16);
	reg17 = phy_read(db, 17);
	reg24 = phy_read(db, 24);
	reg25 = phy_read(db, 25);

	switch(HPNA_rev) {
	case 0xb900: /* DM9801 E3 */
		reg16 |= 0x1000;
		reg25 = ( (reg24 + nfloor) & 0x00ff) | 0xf000;
		break;
	case 0xb901: /* DM9801 E4 */
		reg25 = ( (reg24 + nfloor) & 0x00ff) | 0xc200;
		reg17 = (reg17 & 0xfff0) + nfloor + 3;
		break;
	case 0xb902: /* DM9801 E5 */
	case 0xb903: /* DM9801 E6 */
	default:
		reg16 |= 0x1000;
		reg25 = ( (reg24 + nfloor - 3) & 0x00ff) | 0xc200;
		reg17 = (reg17 & 0xfff0) + nfloor;
		break;
	}

	phy_write(db, 16, reg16);
	phy_write(db, 17, reg17);
	phy_write(db, 25, reg25);
}

/*
	Init LongRun DM9802
*/
static void program_dm9802(board_info_t *db)
{
	__u16 reg25;

	if ( !nfloor ) nfloor = DM9802_NOISE_FLOOR;

	reg25 = phy_read(db, 25);
	reg25 = (reg25 & 0xff00) + nfloor;
	phy_write(db, 25, reg25);
}

/* Identify NIC type
*/
static void identify_nic(board_info_t *db)
{
	u16 phy_reg3;

	iow(db, 0, DM9000_EXT_MII);
	phy_reg3 = phy_read(db, 3);
	switch(phy_reg3 & 0xfff0) {
	case 0xb900:
		if (phy_read(db, 31) == 0x4404) {
			db->nic_type =  HOMERUN_NIC;
			program_dm9801(db, phy_reg3);
		} else {
			db->nic_type = LONGRUN_NIC;
			program_dm9802(db);
		}
		break;
	default: db->nic_type = FASTETHER_NIC; break;
	}
	iow(db, 0, DM9000_INT_MII);
}

/* Initilize dm9000 board
*/
static void dmfe_init_dm9000(struct DEVICE *dev)
{
	board_info_t *db = (board_info_t *)dev->priv;

	DMFE_DBUG(0, "dmfe_init_dm9000()", 0);

	/* RESET device */
	iow(db, 0, 1);
	udelay(100);	/* delay 100us */

	/* I/O mode */
	db->io_mode = ior(db, 0xfe) >> 6; 	/* ISR bit7:6 keeps I/O mode */

	/* NIC Type: FASTETHER, HOMERUN, LONGRUN */
	identify_nic(db);

	/* Set PHY */
	db->op_mode = media_mode;
	set_PHY_mode(db);

	/* Init needed register value */
	db->reg0 = DM9000_REG00;
	if ( (db->nic_type != FASTETHER_NIC) && (db->op_mode & DM9000_1M_HPNA) )
		db->reg0 |= DM9000_EXT_MII;

	/* User passed argument */
	db->reg5 = reg5;
	db->reg8 = reg8;
	db->reg9 = reg9;
	db->rega = rega;

	/* Program operating register */
	iow(db, 0x00, db->reg0);
	iow(db, 0x02, 0);	/* TX Polling clear */
	iow(db, 0x08, 0x3f);	/* Less 3Kb, 200us */
	iow(db, 0x09, db->reg9);/* Flow Control : High/Low Water */
	iow(db, 0x0a, db->rega);/* Flow Control */
	iow(db, 0x2f, 0);	/* Special Mode */
	iow(db, 0x01, 0x2c);	/* clear TX status */
	iow(db, 0xfe, 0x0f); 	/* Clear interrupt status */

	/* Set address filter table */
	dm9000_hash_table(dev);

	/* Activate DM9000 */
	iow(db, 0x05, db->reg5 | 1);	/* RX enable */
	iow(db, 0xff, DM9000_REGFF); 	/* Enable TX/RX interrupt mask */

	/* Init Driver variable */
	db->link_failed = 1;
	db->tx_pkt_cnt = 0;
	db->queue_pkt_len = 0;
	dev->trans_start = 0;
}

/*
  Hardware start transmission.
  Send a packet to media from the upper layer.
*/
static int dmfe_start_xmit(struct sk_buff *skb, struct DEVICE *dev)
{
	board_info_t *db = (board_info_t *)dev->priv;
	int i;

	DMFE_DBUG(0, "dmfe_start_xmit", 0);

	/* Resource flag check */
#if 0
	if ( test_and_set_bit(0, (void *)&dev->tbusy) || (db->tx_pkt_cnt > 1) )
		return 1;
#else
	if (db->tx_pkt_cnt > 1)
	        return 1;
#endif

	/* Disable all interrupt */
	iow(db, 0xff, 0x80);

	/* Move data to DM9000 TX RAM */
	__raw_writeb(0xf8, db->ioaddr);

	(db->outblk)(db->io_data, (unsigned short *)skb->data, skb->len);

	/* TX control: First packet immediately send, second packet queue */
	if (db->tx_pkt_cnt == 0) {
		/* First Packet */
		db->tx_pkt_cnt++;

		/* Set TX length to DM9000 */
		iow(db, 0xfc, skb->len & 0xff);
		iow(db, 0xfd, (skb->len >> 8) & 0xff);

		/* Issue TX polling command */
		iow(db, 0x2, 0x1);	/* Cleared after TX complete */

		dev->trans_start = jiffies;	/* saved the time stamp */
	} else {
		/* Second packet */
		db->tx_pkt_cnt++;
		db->queue_pkt_len = skb->len;
	}

	/* free this SKB */
	dev_kfree_skb(skb);

#if 0
	/* Re-enable resource check */
	if (db->tx_pkt_cnt == 1)
		clear_bit(0, (void*)&dev->tbusy);
#else
	/* we don't need to do anything with this for 2.4 */
#endif

	/* Re-enable interrupt mask */
	iow(db, 0xff, 0x83);

	return 0;
}

/*
  Stop the interface.
  The interface is stopped when it is brought.
*/
static int dmfe_stop(struct DEVICE *dev)
{
	board_info_t *db = (board_info_t *)dev->priv;

	DMFE_DBUG(0, "dmfe_stop", 0);

	/* deleted timer */
	del_timer(&db->timer);

	/* disable system */
#if 0
	dev->start = 0;	/* interface disable */
	dev->tbusy = 1;	/* can't transmit */
#else
	netif_stop_queue(dev);
#endif

	/* free interrupt */
	free_irq(dev->irq, dev);

	/* RESET devie */
	phy_write(db, 0x00, 0x8000);	/* PHY RESET */
	iow(db, 0x1f, 0x01);	/* Power-Down PHY */
	iow(db, 0xff, 0x80);	/* Disable all interrupt */
	iow(db, 0x05, 0x00);	/* Disable RX */

	MOD_DEC_USE_COUNT;

	/* Dump Statistic counter */
#if 1
	printk("\nRX FIFO OVERFLOW %lx\n", db->stats.rx_fifo_errors++);
	printk("RX CRC %lx\n", db->stats.rx_crc_errors++);
	printk("RX LEN Err %lx\n", db->stats.rx_length_errors++);
	printk("RX LEN<64byte %x\n", db->runt_length_counter);
	printk("RX LEN>1514byte %x\n", db->long_length_counter);
	printk("RESET %x\n", db->reset_counter);
	printk("RESET: TX Timeout %x\n", db->reset_tx_timeout);
	printk("RESET: RX Status Wrong %x\n", db->reset_rx_status);
#endif

	return 0;
}

/*
  DM9102 insterrupt handler
  receive the packet to upper layer, free the transmitted packet
*/
static void dmfe_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct DEVICE *dev = dev_id;

	board_info_t *db;
	int int_status, tx_status;
	u8 reg_save;

	DMFE_DBUG(0, "dmfe_interrupt()", 0);

	if (!dev) {
		DMFE_DBUG(1, "dmfe_interrupt() without DEVICE arg", 0);
		return;
	}

#if 0
	if (dev->interrupt) {
		DMFE_DBUG(1, "dmfe_interrupt() re-entry ", 0);
		return;
	}

	/* A real interrupt coming */
	dev->interrupt=1;              /* Lock interrupt */
#endif

	db = (board_info_t *)dev->priv;

	/* Save previous register address */
	reg_save = __raw_readb(db->ioaddr);

	/* Disable all interrupt */
	iow(db, 0xff, 0x80);

	/* Got DM9000 interrupt status */
	int_status = ior(db, 0xfe);		/* Got ISR */
	iow(db, 0xfe, int_status);		/* Clear ISR status */
	//printk("I%x ", int_status);

	/* Received the coming packet */
	if (int_status & 1)
		dmfe_packet_receive(dev, db);

	/* Trnasmit Interrupt check */
	if (int_status & 2) {
		tx_status = ior(db, 0x01);	/* Got TX status */
		if (tx_status & 0xc) {
			/* One packet sent complete */
			db->tx_pkt_cnt--;
			dev->trans_start = 0;
			db->stats.tx_packets++;

			/* Queue packet check & send */
			if (db->tx_pkt_cnt > 0) {
				iow(db, 0xfc, db->queue_pkt_len & 0xff);
				iow(db, 0xfd, (db->queue_pkt_len >> 8) & 0xff);
				iow(db, 0x2, 0x1);
				dev->trans_start = jiffies;
			}

#if 0
			dev->tbusy = 0;	/* Active upper layer, send again */
			mark_bh(NET_BH);
#else
			netif_wake_queue(dev);
#endif

		}
	}

	/* Re-enable interrupt mask */
	iow(db, 0xff, 0x83);

	/* Restore previous register address */
	__raw_writeb(reg_save, db->ioaddr);

#if 0
	dev->interrupt=0;              /* release interrupt lock */
#endif
}

/*
  Get statistics from driver.
*/
static struct enet_statistics * dmfe_get_stats(struct DEVICE *dev)
{
	board_info_t *db = (board_info_t *)dev->priv;

	DMFE_DBUG(0, "dmfe_get_stats", 0);
	return &db->stats;
}

/*
  Process the upper socket ioctl command
*/
static int dmfe_do_ioctl(struct DEVICE *dev, struct ifreq *ifr, int cmd)
{
	DMFE_DBUG(0, "dmfe_do_ioctl()", 0);
	return 0;
}

/*
  A periodic timer routine
  Dynamic media sense, allocated Rx buffer...
*/
static void dmfe_timer(unsigned long data)
{
	struct DEVICE *dev = (struct DEVICE *)data;
	board_info_t *db = (board_info_t *)dev->priv;
	u8 reg_save, tmp_reg;

	DMFE_DBUG(0, "dmfe_timer()", 0);

	/* Save previous register address */
	reg_save = __raw_readb(db->ioaddr);

	/* TX timeout check */
	if (dev->trans_start && ((jiffies - dev->trans_start) > DMFE_TX_TIMEOUT)) {
		db->device_wait_reset = 1;
		db->reset_tx_timeout++;
	}

	/* DM9000 dynamic RESET check and do */
	if (db->device_wait_reset) {
#if 0
		dev->tbusy = 1;
#else
		netif_stop_queue(dev);
#endif
		db->reset_counter++;
		db->device_wait_reset = 0;
		dev->trans_start = 0;
		dmfe_init_dm9000(dev);

#if 0
		dev->tbusy = 0;		/* Active upper layer, send again */
		mark_bh(NET_BH);
#else
		netif_start_queue(dev);
#endif
	}

	/* Auto Sense Media mode policy:
		FastEthernet NIC: don't need to do anything.
		Media Force mode: don't need to do anything.
		HomeRun/LongRun NIC and AUTO_Mode:
			INT_MII not link, select EXT_MII
			EXT_MII not link, select INT_MII
	 */
	if ( (db->nic_type != FASTETHER_NIC) && (db->op_mode & DM9000_AUTO) ) {
		tmp_reg = ior(db, 0x01);	/* Got link status */
		if ( !(tmp_reg & 0x40) ) { /* not link */
			db->reg0 ^= 0x80;
			iow(db, 0x00, db->reg0);
		}
	}

	/* Restore previous register address */
	__raw_writeb(reg_save, db->ioaddr);

	/* Set timer again */
	db->timer.expires = DMFE_TIMER_WUT;
	add_timer(&db->timer);
}

/*
  Received a packet and pass to upper layer
*/
static void dmfe_packet_receive(struct DEVICE *dev, board_info_t *db)
{
	struct sk_buff *skb;
	u8 rxbyte;
	u8 *rdptr;
	u16 i, RxStatus, RxLen, GoodPacket, tmplen;

	/* Check packet ready or not */
	do {
		rxbyte = ior(db, 0xf0);	/* Dummy read */
		rxbyte = ior(db, 0xf0);	/* Got most updated data */

		/* Status check: this byte must be 0 or 1 */
		if (rxbyte > DM9000_PKT_RDY) {
			iow(db, 0x05, 0x00);	/* Stop Device */
			iow(db, 0xfe, 0x80);	/* Stop INT request */
			db->device_wait_reset = 1;
			db->reset_rx_status++;
		}

		/* packet ready to receive check */
		if (rxbyte == DM9000_PKT_RDY) {
			/* A packet ready now  & Get status/length */
			GoodPacket = 1;
			if (db->io_mode == 2) {
				/* Byte mode */
				RxStatus = ior(db, 0xf2) + (ior(db, 0xf2) << 8);
				RxLen = ior(db, 0xf2) + (ior(db, 0xf2) << 8);
			} else {
				/* Word mode */
				__raw_writeb(0xf2, db->ioaddr);
				RxStatus = __raw_readw(db->io_data);
				RxLen = __raw_readw(db->io_data);
			}

			/* Packet Status check */
			if (RxLen < 0x40) {
				GoodPacket = 0;
				db->runt_length_counter++;
			}
			if (RxLen > DM9000_PKT_MAX) {
				printk("<DM9000> RST: RX Len:%x\n", RxLen);
				db->device_wait_reset = 1;
				db->long_length_counter++;
			}
			if (RxStatus & 0xbf00) {
				GoodPacket = 0;
				if (RxStatus & 0x100) db->stats.rx_fifo_errors++;
				if (RxStatus & 0x200) db->stats.rx_crc_errors++;
				if (RxStatus & 0x8000) db->stats.rx_length_errors++;
			}

			/* Move data from DM9000 */
			if (!db->device_wait_reset) {
				if ( GoodPacket && ((skb = dev_alloc_skb(RxLen + 4)) != NULL ) ) {
					skb->dev = dev;
					skb_reserve(skb, 2);
					rdptr = (u8 *) skb_put(skb, RxLen - 4);

					/* Read received packet from RX SARM */
					(db->inblk)(db->io_data, (unsigned short *)rdptr, RxLen);

					/* Pass to upper layer */
					skb->protocol = eth_type_trans(skb,dev);
					netif_rx(skb);
					db->stats.rx_packets++;
				} else {
					/* Without buffer or error packet */
					if (db->io_mode == 2) {
						/* Byte mode */
						for (i = 0; i < RxLen; i++)
							__raw_readb(db->io_data);
					} else {
						/* Word mode */
						tmplen = (RxLen + 1) / 2;
						for (i = 0; i < tmplen; i++)
							__raw_readw(db->io_data);
					}
				}
			}
		}
	}while( rxbyte  && !db->device_wait_reset);
}

/*
  Read a word data from SROM
*/
static u16 read_srom_word(board_info_t *db, int offset)
{
	iow(db, 0xc, offset);
	iow(db, 0xb, 0x4);
	udelay(200);
	iow(db, 0xb, 0x0);
	return (ior(db, 0xd) + (ior(db, 0xe) << 8) );
}

/*
  Set DM9000 multicast address
*/
static void dm9000_hash_table(struct DEVICE *dev)
{
	board_info_t *db = (board_info_t *)dev->priv;
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
		hash_val = cal_CRC((char *)mcptr->dmi_addr, 6, 0) & 0x3f;
		hash_table[hash_val / 16] |= (u16) 1 << (hash_val % 16);
	}

	/* Write the hash table to MAC MD table */
	for (i = 0, oft = 0x16; i < 4; i++) {
		iow(db, oft++, hash_table[i] & 0xff);
		iow(db, oft++, (hash_table[i] >> 8) & 0xff);
	}
}

/*
  Calculate the CRC valude of the Rx packet
  flag = 1 : return the reverse CRC (for the received packet CRC)
         0 : return the normal CRC (for Hash Table index)
*/
static unsigned long cal_CRC(unsigned char * Data, unsigned int Len, u8 flag)
{
	unsigned long Crc = 0xffffffff;

	while (Len--) {
		Crc = CrcTable[(Crc ^ *Data++) & 0xFF] ^ (Crc >> 8);
	}

	if (flag) return ~Crc;
	else return Crc;
}

/*
   Read a word from phyxcer
*/
static u16 phy_read(board_info_t *db, int reg)
{
	/* Fill the phyxcer register into REG_0C */
	iow(db, 0xc, DM9000_PHY | reg);

	iow(db, 0xb, 0xc); /* Issue phyxcer read command */
	udelay(100);	/* Wait read complete */
	iow(db, 0xb, 0x0); /* Clear phyxcer read command */

	/* The read data keeps on REG_0D & REG_0E */
	return ( ior(db, 0xe) << 8 ) | ior(db, 0xd);
}

/*
   Write a word to phyxcer
*/
static void phy_write(board_info_t *db, int reg, u16 value)
{
	/* Fill the phyxcer register into REG_0C */
	iow(db, 0xc, DM9000_PHY | reg);

	/* Fill the written data into REG_0D & REG_0E */
	iow(db, 0xd, (value & 0xff));
	iow(db, 0xe, ( (value >> 8) & 0xff));

	iow(db, 0xb, 0xa);		/* Issue phyxcer write command */
	udelay(500);		/* Wait write complete */
	iow(db, 0xb, 0x0);		/* Clear phyxcer write command */
}

#ifdef MODULE

    MODULE_AUTHOR("Sten Wang, sten_wang@davicom.com.tw");
    MODULE_DESCRIPTION("BAST: Davicom DM9000 Fast Ethernet Driver");
    MODULE_PARM(debug, "i");
    MODULE_PARM(mode, "i");
    MODULE_PARM(reg5, "i");
    MODULE_PARM(reg9, "i");
    MODULE_PARM(rega, "i");
    MODULE_PARM(nfloor, "i");

/* Description:
   when user used insmod to add module, system invoked init_module()
   to initilize and register.
*/
int init_module(void)
{
	DMFE_DBUG(0, "init_module() ", debug);

	debug = 1;  // force debug

	if (debug) dmfe_debug = debug;   /* set debug flag */

	switch(mode) {
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

	nfloor = (nfloor > 15) ? 0:nfloor;

	return dmfe_bast_probe(0);              /* search board and register */
}



/* Description:
   when user used rmmod to delete module, system invoked clean_module()
   to  un-register DEVICE.
*/
void cleanup_module(void)
{
	struct DEVICE *next_dev;
	board_info_t * db;

	DMFE_DBUG(0, "clean_module()", 0);

	while (dmfe_root_dev) {
		next_dev = ((board_info_t *)dmfe_root_dev->priv)->next_dev;
		unregister_netdev(dmfe_root_dev);
		db = (board_info_t *)dmfe_root_dev->priv;
		release_region(dmfe_root_dev->base_addr, 2);
		kfree(db);		/* free board information */
		kfree(dmfe_root_dev);	/* free device structure */
		dmfe_root_dev = next_dev;
	}

	DMFE_DBUG(0, "clean_module() exit", 0);
}

#else

static int __init
dmfe_bast_init(void)
{
  return dmfe_bast_probe(NULL);
}

module_init(dmfe_bast_init);

#endif  /* MODULE */
/*
 * Local variables:
 *  version-control: t
 *  kept-new-versions: 5
 *  c-indent-level: 8
 *  tab-width: 8
 * End:
 *
 */
