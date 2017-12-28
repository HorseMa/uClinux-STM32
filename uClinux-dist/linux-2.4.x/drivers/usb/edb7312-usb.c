/*
 * EDB7312 support for PDIUSBD12 / mass-storage.
 *
 *	(c) Copyright 2003 Petko Manolov <petkan@nucleusys.com>
 *	(c) Copyright 2003 Vladimir Ivanov <vladitx@nucleusys.com>
 *
 * TODO:
 * 	 - more SCSI commands for various OS needs
 *       - make this endian-independent with respect to USB structs,
 * as it is currently LE
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <asm/arch/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/delay.h>
#include <asm/semaphore.h>
#include <asm/signal.h>
#include <asm/system.h>
#include <asm/types.h>

#include <linux/completion.h>
#include <linux/config.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/sched.h>

#include <linux/blkdev.h>
#include <linux/fs.h>
#include <linux/kdev_t.h>
#include <linux/locks.h>

/*
 * XXX: Here's a good thing to think about
 *
 *   According to USB 1.1, "Set Address" device request is the only one which
 * should be processed differently and must be completed _after_ successfull
 * status state ... do you really believe specs? We usually don't, but it
 * takes some time to find exactly what we won't believe.
 */

/* ------------------------------------------------------------------------- */

/*
 * debug mode
 */

#undef DBG
#define ID "usbd12"

/*
 * hardware description
 */

#define IRQ_D12		IRQ_EINT1

#define REG_D12_DAT	(EDB7312_VIRT_PDIUSBD12 + 0)
#define REG_D12_CMD	(EDB7312_VIRT_PDIUSBD12 + 1)

/* 
 * XXX:
 *
 * It seems that different EDB7312 revisions have the SUSPEND signal of
 * PDIUSBD12 routed to different locations. This is specific to P7 revision.
 */

#define GPIO_D12_SUSP	((__raw_readb(EP7312_VIRT_CS3 + 0x0004) & 0x04) ? 1 : 0)

#define __delay()	{			\
	__raw_readb(EP7312_VIRT_CS3 + 0x0004);	\
	__raw_readb(EP7312_VIRT_CS3 + 0x0004);	\
}

/*
 * atomic operations
 */

#define d12_lock(p)	disable_irq((p)->irq)
#define	d12_unlock(p)	enable_irq((p)->irq)

/*
 * PDIUSBD12 commands and data
 */

/* --- "Select endpoint" --- */

#define CMD_SELECT_ENDP		0x00

#define D_SELECT_ENDP_FULL	0x01
#define D_SELECT_ENDP_STALL	0x02

/* --- "Set endpoint status" --- */

#define CMD_SET_ENDP_STATUS	0x40

/* --- "Read last transaction status" --- */

#define CMD_READ_LTRNS		0x40

#define D_READ_LTRNS_SUCCESS	0x01
#define D_READ_LTRNS_ERR_M	0x1E
#define D_READ_LTRNS_SETUP	0x20
#define D_READ_LTRNS_DATA1	0x40
#define D_READ_LTRNS_PREV	0x80

#define D_READ_LTRNS_ERR_NO		(0 << 1)
#define D_READ_LTRNS_ERR_PID_ENC	(1 << 1)
#define D_READ_LTRNS_ERR_PID_UNK	(2 << 1)
#define D_READ_LTRNS_ERR_UNEXP_PK	(3 << 1)
#define D_READ_LTRNS_ERR_TOKCRC		(4 << 1)
#define D_READ_LTRNS_ERR_DATACRC	(5 << 1)
#define D_READ_LTRNS_ERR_TIMEOUT	(6 << 1)
#define D_READ_LTRNS_ERR_NEVER		(7 << 1)
#define D_READ_LTRNS_ERR_UNEXP_EOP	(8 << 1)
#define D_READ_LTRNS_ERR_NAK		(9 << 1)
#define D_READ_LTRNS_ERR_STALL		(10 << 1)
#define D_READ_LTRNS_ERR_OVF		(11 << 1)
#define D_READ_LTRNS_ERR_BITSTUFF	(13 << 1)
#define D_READ_LTRNS_ERR_WRONG_DPID	(15 << 1)

/* --- "Read endpoint status" --- */

#define CMD_READ_ENDP_STATUS	0x80

#define D_READ_ENDP_STAT_BUF0	0x20
#define D_READ_ENDP_STAT_BUF1	0x40

/* --- "Set address / enable" --- */

#define CMD_SET_ADDR_EN		0xD0

/* --- "Set endpoint enable" --- */

#define CMD_SET_ENDP_EN		0xD8

/* --- "Read buffer" --- */

#define CMD_READ_BUFFER		0xF0

/* --- "Write buffer" --- */

#define CMD_WRITE_BUFFER	0xF0

/* --- "Acknowledge setup" --- */

#define CMD_ACKNOWLEDGE_SETUP	0xF1

/* --- "Clear buffer" --- */

#define CMD_CLEAR_BUFFER	0xF2

/* --- "Set mode" --- */

#define CMD_MODE		0xF3

#define	D_MODE0_NOLAZYCLK	0x02
#define	D_MODE0_CLKRUNNING	0x04
#define D_MODE0_INTMODE		0x08
#define D_MODE0_SOFTCONNECT	0x10
#define D_MODE0_EP_NONISO	(0 << 6)

#define D_MODE1_CLK4M		0x0B
#define D_MODE1_SETTOONE	0x40
#define D_MODE1_SOFONLY		0x80

/*
 * default CLKOUT mode is:
 *	- lazy clock during standby
 *	- no clock running during standby
 *	- 4 MHz
 */

#define D_MODE0_DEFAULT		D_MODE0_EP_NONISO
#define D_MODE1_DEFAULT		(D_MODE1_SETTOONE | D_MODE1_CLK4M)

/* --- "Read interrupt register" --- */

#define CMD_READ_INT		0xF4

#define D_READ_INT_EP0_O	0x0001
#define D_READ_INT_EP0_I	0x0002
#define D_READ_INT_EP1_O	0x0004
#define D_READ_INT_EP1_I	0x0008
#define D_READ_INT_EP2_O	0x0010
#define D_READ_INT_EP2_I	0x0020
#define D_READ_INT_BUSRESET	0x0040
#define D_READ_INT_SUSPENDCHG	0x0080
#define D_READ_INT_DMAEOT	0x0100

/* --- "Read current frame number" --- */

#define CMD_READ_FRAMENUM	0xF5

/* --- "Validate buffer" --- */

#define CMD_VALIDATE_BUFFER	0xFA

/* --- "Set DMA" --- */

#define CMD_DMA			0xFB

#define D_DMA_NODMA		0x00
#define D_DMA_INTMODE		0x20
#define D_DMA_EPX4IE		0x40
#define D_DMA_EPX5IE		0x80

#define D_DMA_DEFAULT		D_DMA_NODMA

/*
 * general constants
 */

#define TIME_OUT	(HZ * 5)

#define EP_CTRL		0
#define EP_FREE		1
#define EP_BULK		2

#define EPX_CTRL_O	0
#define EPX_CTRL_I	1
#define EPX_FREE_O	2
#define EPX_FREE_I	3
#define EPX_BULK_O	4
#define EPX_BULK_I	5

#define EP_CTRL_SIZE	16
#define EP_FREE_SIZE	16
#define EP_BULK_SIZE	64

/* --- */

#define CONFIG_VAL	1

#define DEF_LANG	0x0409

/* ------------------------------------------------------------------------- */

/* USB 1.1 */

#define REQTYPE_DIR_M		0x80
#define REQTYPE_DIR_OUT		(0 << 7)
#define REQTYPE_DIR_IN		(1 << 7)
#define REQTYPE_TYPE_M		0x60
#define REQTYPE_TYPE_STANDARD	(0 << 5)
#define REQTYPE_TYPE_CLASS	(1 << 5)
#define REQTYPE_TYPE_VENDOR	(2 << 5)
#define REQTYPE_RECP_M		0x1F
#define REQTYPE_RECP_DEVICE	(0 << 0)
#define REQTYPE_RECP_INTERFACE	(1 << 0)
#define REQTYPE_RECP_ENDPOINT	(2 << 0)

struct devreq {
	__u8 bmRequestType;
	__u8 bRequest;
	__u16 wValue;
	__u16 wIndex;
	__u16 wLength;
} __attribute__ ((packed));

#define DESC_TYPE_DEVICE	1
#define DESC_TYPE_CONFIGURATION	2
#define DESC_TYPE_STRING	3
#define DESC_TYPE_INTERFACE	4
#define DESC_TYPE_ENDPOINT	5

#define CLASS_MASS_STORAGE	0x08
#define SUBCLASS_SCSI		0x06
#define PROTOCOL_BULK		0x50

#define STATUS_DEVICE_SELF_POWERED	0x01
#define STATUS_DEVICE_REMOTE_WAKEUP	0x02
#define STATUS_ENDPOINT_HALT		0x01

#define FEATURE_ENDPOINT_HALT		0
#define FEATURE_DEVICE_REMOTE_WAKEUP	1

/* mass-storage, bulk-only */

#define CBW_SIGNATURE		0x43425355

#define CBW_FLAGS_DIR_M		0x80
#define CBW_FLAGS_DIR_OUT	(0 << 7)
#define CBW_FLAGS_DIR_IN	(1 << 7)

struct cbw {
	__u32 dCBWSignature;
	__u32 dCBWTag;
	__u32 dCBWDataTransferLength;
	__u8 bmCBWFlags;
	__u8 bCBWLUN;
	__u8 bCBWCBLength;
	__u8 CBWCB[16];
} __attribute__ ((packed));

#define CSW_SIGNATURE		0x53425355

#define CSW_STAT_OK		0
#define CSW_STAT_FAILED		1
#define CSW_STAT_PHASE_ERROR	2

struct csw {
	__u32 dCSWSignature;
	__u32 dCSWTag;
	__u32 dCSWDataResidue;
	__u8 bCSWStatus;
} __attribute__ ((packed));

/* ------------------------------------------------------------------------- */

/* SCSI SPC-2 */

#define SCSI_SKEY_NO_SENSE		0
#define SCSI_SKEY_NOT_READY		2
#define SCSI_SKEY_MEDIUM_ERROR		3
#define SCSI_SKEY_ILLEGAL_REQUEST	5

#define SCSI_ASC_NO				0x0000
#define SCSI_ASC_LOGICAL_UNIT_NOT_READY		0x0400
#define SCSI_ASC_WRITE_ERROR			0x0C02
#define SCSI_ASC_UNRECOVERED_READ_ERROR		0x1100
#define SCSI_ASC_INVALID_CMD			0x2000
#define SCSI_ASC_LBA_OUT_OF_RANGE		0x2100
#define SCSI_ASC_INVALID_FIELD_IN_CDB		0x2400
#define SCSI_ASC_LOGICAL_UNIT_NOT_SUPPORTED	0x2500

#define MAX_SCSI_DATA_SENSE		18
#define MAX_SCSI_DATA_INQUIRY		36
#define MAX_SCSI_DATA_CAPACITY10	8
#define MAX_SCSI_DATA_CAPACITY16	12

/* ------------------------------------------------------------------------- */

/* device I/O request */

struct dioreq {
	int cmd;				/* device i/o command */
	int lun;
	__u32 lba;
	__u32 blks;
	__u32 nblks;
	void *buf;
};

/* device context */

#define FSM_DEV_POWERED		0
#define FSM_DEV_DEFAULT		1
#define FSM_DEV_ADDRESS		2
#define FSM_DEV_CONFIGURED	3

#define FSM_CTL_IDLE		0
#define FSM_CTL_DATA_IN		1
#define FSM_CTL_STATUS_OUT	2
#define FSM_CTL_STATUS_IN	3

#define FSM_BULK_IDLE		0
#define FSM_BULK_DATA_OUT	1
#define FSM_BULK_DATA_OUT_WAIT	2
#define FSM_BULK_DATA_IN	3
#define FSM_BULK_DATA_IN_WAIT	4
#define FSM_BULK_STALL_TO_CSW	5

#define MAX_DBUF		256
#define MAX_CHUNKBUF		65536

#define ATD_STAT_READY		(1 << 0)
#define ATD_STAT_RO		(1 << 1)

#define DIO_CMD_EXIT		1
#define DIO_CMD_READ		2
#define DIO_CMD_WRITE		3
#define DIO_CMD_VERIFY		4

struct d12 {
	unsigned int reg_cmd;
	unsigned int reg_dat;
	unsigned int irq;

	const void *desc_dev;
	int desc_dev_len;

	const void *desc_cfg;
	int desc_cfg_len;

	int desc_str_max;
	const void * const *desc_str;
	const int *desc_str_len;

	/* --- */

	int max_lun;

	const void *scsi_str_vendor[16];
	const void *scsi_str_product[16];
	const void *scsi_str_revision[16];

	kdev_t atd_dev[16];			/* attached device */
	int atd_blk[16];			/* block size */
	int atd_chunkblk[16];			/* blocks per chunk */
	__u32 atd_lba_capacity[16];		/* capacity in blocks */
	int atd_stat[16];			/* status */

	struct block_device *bdev[16];

	/* --- */

	int suspend;				/* 1- in suspend mode */

	int feat_wakeup;			/* remote wakeup */

	int fsm_dev;				/* device state */

	int fsm_ctl;				/* control endpoint state */
	int fsm_ctl_lastpkt;			/* last packet sent */

	struct devreq drq;			/* device requests */
	unsigned char dbuf[MAX_DBUF];		/* data buffer */
	int dlen, dptr;

	int fsm_bulk;				/* bulk endpoint state */

	unsigned char *bbuf;			/* bulk buffer chunk */
	int blen, bptr;
	void (*bhd)(struct d12 *, int, int);	/* chunk handler */
	int bxferlen, bxferptr;			/* total transfer length */

	struct cbw cbw;				/* CBW request */
	struct csw csw;				/* CSW answer */
	int cswdelay;				/* postpone CSW sending */
	int cswearly;				/* early CSW sending on error */

	unsigned char scsi_sense[MAX_SCSI_DATA_SENSE];	/* should this be per LUN ? */

	__u32 scsi_lba;
	__u32 scsi_blks;

	struct completion cpl_dio_sync;		/* enter / exit sync */
	struct semaphore wait_dio_cmd;
	volatile struct dioreq dio;

	unsigned char tmp[256];			/* template */
	unsigned char chunkbuf[MAX_CHUNKBUF];
};

struct d12 d12;

/* ------------------------------------------------------------------------- */

static __u16 get_u16be(const void *p)
{
	const unsigned char *b;
	__u16 r;

	b = (const unsigned char *)p;

	r = (b[0] << 8) |
	    (b[1] << 0);

	return r;
}

#if 0
static void put_16be(void *p, __u16 d)
{
	unsigned char *b;

	b = (unsigned char *)p;

	b[0] = (d >> 8) & 0xFF;
	b[1] = (d >> 0) & 0xFF;
}
#endif

static __u32 get_u32be(const void *p)
{
	const unsigned char *b;
	__u32 r;

	b = (const unsigned char *)p;

	r = (b[0] << 24) |
	    (b[1] << 16) |
	    (b[2] <<  8) |
	    (b[3] <<  0);

	return r;
}

static void put_32be(void *p, __u32 d)
{
	unsigned char *b;

	b = (unsigned char *)p;

	b[0] = (d >> 24) & 0xFF;
	b[1] = (d >> 16) & 0xFF;
	b[2] = (d >>  8) & 0xFF;
	b[3] = (d >>  0) & 0xFF;
}

static __u64 get_u64be(const void *p)
{
	const unsigned char *b;
	__u64 r;

	b = (const unsigned char *)p;

	r = get_u32be(b + 0);
	r <<= 32;
	r |= get_u32be(b + 4);

	return r;
}

static void put_64be(void *p, __u64 d)
{
	unsigned char *b;

	b = (unsigned char *)p;

	put_32be(b + 0, d >> 32);
	put_32be(b + 4, d >>  0);
}

/* ------------------------------------------------------------------------- */

/*
 * low-level PDIUSBD12
 */

static inline void d12_outc(struct d12 *p, int cmd)
{
	__delay();
	__raw_writeb(cmd, p->reg_cmd);
}

static inline void d12_outd(struct d12 *p, int dat)
{
	__delay();
	__raw_writeb(dat, p->reg_dat);
}

static inline int d12_ind(struct d12 *p)
{
	__delay();
	return __raw_readb(p->reg_dat);
}

/* --- */

static inline void d12_out1(struct d12 *p, int cmd, int d0)
{
	d12_outc(p, cmd);
	d12_outd(p, d0);
}

static inline void d12_out2(struct d12 *p, int cmd, int d0, int d1)
{
	d12_outc(p, cmd);
	d12_outd(p, d0);
	d12_outd(p, d1);
}

static inline int d12_in1(struct d12 *p, int cmd)
{
	d12_outc(p, cmd);
	return d12_ind(p);
}

static inline int d12_in2(struct d12 *p, int cmd)
{
	int d0, d1;

	d12_outc(p, cmd);
	d0 = d12_ind(p);
	d1 = d12_ind(p);
	return (d1 << 8) | d0;
}

/* --- */

/* clear buffer */

static void d12_buf_clear(struct d12 *p)
{
	d12_outc(p, CMD_CLEAR_BUFFER);
}

/* validate buffer */

static void d12_buf_validate(struct d12 *p)
{
	d12_outc(p, CMD_VALIDATE_BUFFER);
}

/* get frame number */

#if 0
static int d12_frame(struct d12 *p)
{
	return d12_in2(p, CMD_READ_FRAMENUM) & 0x7FF;
}
#endif

/* read last transaction status and ack interrupt */

static int d12_read_last_trans(struct d12 *p, int epx)
{
	return d12_in1(p, CMD_READ_LTRNS + epx);
}

/* set data pointer to start of internal buffer */

static int d12_select_endpoint(struct d12 *p, int epx)
{
	return d12_in1(p, CMD_SELECT_ENDP + epx);
}

/* set USB address (0x00 .. 0x7F) and enable */

static void d12_set_address_enable(struct d12 *p, int adr, int en)
{
	d12_out1(p, CMD_SET_ADDR_EN, adr | (en ? 0x80 : 0x00));
}

/* enable generic/isochronous endpoints */

static void d12_set_endpoint_enable(struct d12 *p, int en)
{
	d12_out1(p, CMD_SET_ENDP_EN, en ? 0x01 : 0x00);
}

/* get endpoint status */

static int d12_status_get(struct d12 *p, int epx)
{
	return d12_in1(p, CMD_READ_ENDP_STATUS + epx);
}

/* unconditionally initialize / stall endpoint */

static void d12_status_set(struct d12 *p, int epx, int stall)
{
	d12_out1(p, CMD_SET_ENDP_STATUS + epx, stall);
}

/* --- */

/* stall / unstall */

static void d12_stall_ctrl(struct d12 *p, int stall)
{
	d12_status_set(p, EPX_CTRL_O, stall);
	d12_status_set(p, EPX_CTRL_I, stall);
}

static void d12_stall_free(struct d12 *p, int stall)
{
	d12_status_set(p, EPX_FREE_O, stall);
	d12_status_set(p, EPX_FREE_I, stall);
}

static void d12_stall_bulk(struct d12 *p, int stall)
{
	d12_status_set(p, EPX_BULK_O, stall);
	d12_status_set(p, EPX_BULK_I, stall);
}

/* connect / disconnect */

static void d12_connect(struct d12 *p, int on)
{
	d12_out2(p, CMD_MODE, D_MODE0_DEFAULT | (on ? D_MODE0_SOFTCONNECT : 0), D_MODE1_DEFAULT);
}

/* after bus-reset */

static void d12_reset(struct d12 *p)
{
	d12_set_address_enable(p, 0, 1);
	d12_set_endpoint_enable(p, 0);

	d12_stall_ctrl(p, 1);
	d12_stall_free(p, 1);
	d12_stall_bulk(p, 1);
}

/* initialize */

static void d12_init(struct d12 *p)
{
	d12_connect(p, 0);
	d12_out1(p, CMD_DMA, D_DMA_DEFAULT | D_DMA_EPX4IE | D_DMA_EPX5IE);

	d12_reset(p);

	d12_connect(p, 1);
}

/* acknowledge setup on endpoint #0 */

static void d12_acknowledge_setup(struct d12 *p)
{
	d12_select_endpoint(p, EPX_CTRL_O);
	d12_outc(p, CMD_ACKNOWLEDGE_SETUP);
	d12_select_endpoint(p, EPX_CTRL_I);
	d12_outc(p, CMD_ACKNOWLEDGE_SETUP);
}

/* read buffer */

static int d12_buf_read(struct d12 *p, int epx, void *buf, int max)
{
	int i, l, len, stat;
	unsigned char *b;

	b = (unsigned char *)buf;

	if (epx == EPX_BULK_O) {
		stat = d12_status_get(p, epx);
		i = len = 0;
		if (stat & D_READ_ENDP_STAT_BUF0)
			i++;
		if (stat & D_READ_ENDP_STAT_BUF1)
			i++;

		while (i-- && (max - len)) {
			d12_select_endpoint(p, epx);

			d12_in1(p, CMD_READ_BUFFER);
			l = d12_ind(p);

			if (len + l > max)
				l = max - len;
			len += l;

			while (l--)
				*b++ = d12_ind(p);
			d12_buf_clear(p);
		}
	} else {
		d12_select_endpoint(p, epx);

		d12_in1(p, CMD_READ_BUFFER);
		l = d12_ind(p);

		if (l > max)
			l = max;
		len = l;

		while (l--)
			*b++ = d12_ind(p);
		d12_buf_clear(p);
	}

	return len;
}

/* write buffer */

static int d12_buf_write(struct d12 *p, int epx, const void *buf, int len)
{
	int i, l, l2, stat;
	const unsigned char *b;

	b = (const unsigned char *)buf;

	if (epx == EPX_BULK_I) {
		stat = d12_status_get(p, epx);
		i = 2;
		if (stat & D_READ_ENDP_STAT_BUF0)
			i--;
		if (stat & D_READ_ENDP_STAT_BUF1)
			i--;

		l2 = 0;
		while (i-- && len) {
			d12_select_endpoint(p, epx);

			l = len;
			if (l > EP_BULK_SIZE)
				l = EP_BULK_SIZE;
			d12_out2(p, CMD_WRITE_BUFFER, 0, l);

			l2 += l;
			len -= l;

			while (l--)
				d12_outd(p, *b++);
			d12_buf_validate(p);
		}
		len = l2;
	} else {
		d12_select_endpoint(p, epx);

		d12_out2(p, CMD_WRITE_BUFFER, 0, len);

		l = len;

		while (l--)
			d12_outd(p, *b++);
		d12_buf_validate(p);
	}

	return len;
}

/* ------------------------------------------------------------------------- */

/*
 * USB 1.1 chapter 9 broken into tiny little pieces plus a little mass-storage
 */

static void ep_ctrl_ack(struct d12 *p)
{
	d12_buf_write(p, EPX_CTRL_I, 0, 0);
	p->fsm_ctl = FSM_CTL_STATUS_IN;
}

static void ep_ctrl_write(struct d12 *p, const void *buf, int len)
{
	struct devreq *drq;

	drq = &p->drq;

	if (len > drq->wLength)			/* 9.3.5: return <= */
		len = drq->wLength;

	if (len > MAX_DBUF)
		len = MAX_DBUF;

	memcpy(p->dbuf, buf, len);
	p->dlen = len;
	p->dptr = 0;

	if (len > EP_CTRL_SIZE)
		len = EP_CTRL_SIZE;

	d12_buf_write(p, EPX_CTRL_I, p->dbuf, len);
	p->dptr += len;

	p->fsm_ctl = (p->dptr == p->dlen) ? FSM_CTL_STATUS_OUT : FSM_CTL_DATA_IN;
	p->fsm_ctl_lastpkt = len;
}

/* --- */

static int std_reserved(struct d12 *p)
{
#ifdef DBG
	printk("%s: unsupported reserved request\n", ID);
#endif
	return 1;
}

static int std_get_status(struct d12 *p)
{
	struct devreq *drq;
	int ep, epx;
	unsigned char buf[2];

	drq = &p->drq;

	if ((drq->bmRequestType & (REQTYPE_DIR_M | REQTYPE_TYPE_M)) != (REQTYPE_DIR_IN | REQTYPE_TYPE_STANDARD))
		goto err;

	if (drq->wValue || (drq->wLength != 2))
		goto err;

	if (p->fsm_dev == FSM_DEV_DEFAULT)
		goto err;

	buf[0] = buf[1] = 0;

	switch (drq->bmRequestType & REQTYPE_RECP_M) {
	case REQTYPE_RECP_DEVICE:
		if (drq->wIndex)
			goto err;

		buf[0] = STATUS_DEVICE_SELF_POWERED | (p->feat_wakeup ? STATUS_DEVICE_REMOTE_WAKEUP : 0);
#ifdef DBG
		printk("%s: get_status device --> 0x%02x 0x%02x\n", ID, buf[0], buf[1]);
#endif
		break;

	case REQTYPE_RECP_INTERFACE:
		if (p->fsm_dev == FSM_DEV_ADDRESS)
			goto err;

		if (drq->wIndex & 0xFF)		/* only one interface */
			goto err;		
#ifdef DBG
		printk("%s: get_status interface --> 0x%02x 0x%02x\n", ID, buf[0], buf[1]);
#endif
		break;

	case REQTYPE_RECP_ENDPOINT:
		ep = drq->wIndex & 0x0F;
		if ((ep != EP_CTRL) && (ep != EP_BULK))
			goto err;

		if (p->fsm_dev == FSM_DEV_ADDRESS)
			if (ep != EP_CTRL)
				goto err;

		epx = (ep << 1) | ((drq->wIndex & 0x80) ? 1 : 0);
		buf[0] = (d12_select_endpoint(p, epx) & D_SELECT_ENDP_STALL) ? STATUS_ENDPOINT_HALT : 0;
#ifdef DBG
		printk("%s: get_status endpoint --> 0x%02x 0x%02x\n", ID, buf[0], buf[1]);
#endif
		break;

	default:
		goto err;
	}

	ep_ctrl_write(p, buf, drq->wLength);

	return 0;

err:
#ifdef DBG
	printk("%s: bad get_status request\n", ID);
#endif
	return 1;
}

static int feature(struct d12 *p, int on)
{
	struct devreq *drq;
	int ep, epx;

	drq = &p->drq;

	if ((drq->bmRequestType & (REQTYPE_DIR_M | REQTYPE_TYPE_M)) != (REQTYPE_DIR_OUT | REQTYPE_TYPE_STANDARD))
		goto err;

	if (drq->wLength)
		goto err;

	if (p->fsm_dev == FSM_DEV_DEFAULT)
		goto err;

	switch (drq->bmRequestType & REQTYPE_RECP_M) {
	case REQTYPE_RECP_DEVICE:
		if (drq->wIndex)
			goto err;

		if (drq->wValue == FEATURE_DEVICE_REMOTE_WAKEUP) {
			p->feat_wakeup = on;
#ifdef DBG
			printk("%s: %s_feature device_remote_wakeup\n", ID, on ? "set" : "clear");
#endif
		} else
			goto err;
		break;

	case REQTYPE_RECP_INTERFACE:
		goto err;

	case REQTYPE_RECP_ENDPOINT:
		ep = drq->wIndex & 0x0F;
		if ((ep != EP_CTRL) && (ep != EP_BULK))
			goto err;

		if (p->fsm_dev == FSM_DEV_ADDRESS)
			if (ep != EP_CTRL)
				goto err;

		epx = (ep << 1) | ((drq->wIndex & 0x80) ? 1 : 0);

		if (drq->wValue == FEATURE_ENDPOINT_HALT) {
			d12_status_set(p, epx, on);

			if ((epx == EPX_BULK_I) && !on && (p->fsm_bulk == FSM_BULK_STALL_TO_CSW)) {
				d12_buf_write(p, EPX_BULK_I, &p->csw, sizeof(struct csw));
				p->fsm_bulk = FSM_BULK_IDLE;
			}
#ifdef DBG
			printk("%s: %s_feature endpoint_halt on epx #%d\n", ID, on ? "set" : "clear", epx);
#endif
		} else
			goto err;
		break;

	default:
		goto err;
	}

	ep_ctrl_ack(p);

	return 0;

err:
#ifdef DBG
	printk("%s: bad %s_feature request\n", ID, on ? "set" : "clear");
#endif
	return 1;
}

static int std_clear_feature(struct d12 *p)
{
	return feature(p, 0);
}

static int std_set_feature(struct d12 *p)
{
	return feature(p, 1);
}

static int std_set_address(struct d12 *p)
{
	struct devreq *drq;

	drq = &p->drq;

	if (drq->bmRequestType != (REQTYPE_DIR_OUT | REQTYPE_TYPE_STANDARD | REQTYPE_RECP_DEVICE))
		goto err;

	if ((drq->wValue > 127) || drq->wIndex || drq->wLength)
		goto err;

	switch (p->fsm_dev) {
	case FSM_DEV_DEFAULT:
		if (drq->wValue)
			p->fsm_dev = FSM_DEV_ADDRESS;
		break;

	case FSM_DEV_ADDRESS:
		if (!drq->wValue)
			p->fsm_dev = FSM_DEV_DEFAULT;
		break;

	case FSM_DEV_CONFIGURED:
		goto err;
	}

	d12_set_address_enable(p, drq->wValue, 1);
#ifdef DBG
	printk("%s: set_address %d\n", ID, drq->wValue);
#endif

	ep_ctrl_ack(p);

	return 0;

err:
#ifdef DBG
	printk("%s: bad set_address request\n", ID);
#endif
	return 1;
}

static int std_get_descriptor(struct d12 *p)
{
	struct devreq *drq;
	int type, ndx;
	const void *buf;
	int len;

	drq = &p->drq;

	if (drq->bmRequestType != (REQTYPE_DIR_IN | REQTYPE_TYPE_STANDARD | REQTYPE_RECP_DEVICE))
		goto err;

	type = (drq->wValue >> 8) & 0xFF;
	ndx = drq->wValue & 0xFF;

	switch (type) {
	case DESC_TYPE_DEVICE:
		if (ndx || drq->wIndex)
			goto err;
		buf = p->desc_dev;
		len = p->desc_dev_len;
#ifdef DBG
		printk("%s: get_descriptor device\n", ID);
#endif
		break;

	case DESC_TYPE_CONFIGURATION:
		if (ndx || drq->wIndex)		/* only one configuration */
			goto err;
		buf = p->desc_cfg;
		len = p->desc_cfg_len;
#ifdef DBG
		printk("%s: get_descriptor configuration %d\n", ID, ndx);
#endif
		break;

	case DESC_TYPE_STRING:
		if (!ndx) {
			if (drq->wIndex)
				goto err;
		} else {
			if ((ndx >= p->desc_str_max) || (drq->wIndex != DEF_LANG))
				goto err;
		}

		buf = p->desc_str[ndx];
		len = p->desc_str_len[ndx];
#ifdef DBG
		printk("%s: get_descriptor string %d\n", ID, ndx);
#endif
		break;

	default:
		goto err;
	}

	ep_ctrl_write(p, buf, len);

	return 0;

err:
#ifdef DBG
	printk("%s: bad get_descriptor request\n", ID);
#endif
	return 1;
}

static int std_set_descriptor(struct d12 *p)
{
#ifdef DBG
	printk("%s: unsupported set_descriptor request\n", ID);
#endif
	return 1;
}

static int std_get_configuration(struct d12 *p)
{
	struct devreq *drq;
	unsigned char buf[1];

	drq = &p->drq;

	if (drq->bmRequestType != (REQTYPE_DIR_IN | REQTYPE_TYPE_STANDARD | REQTYPE_RECP_DEVICE))
		goto err;

	if (drq->wValue || drq->wIndex || (drq->wLength != 1))
		goto err;

	switch (p->fsm_dev) {
	case FSM_DEV_DEFAULT:
		goto err;

	case FSM_DEV_ADDRESS:
		buf[0] = 0;
		break;

	case FSM_DEV_CONFIGURED:
		buf[0] = CONFIG_VAL;
		break;
	}

	ep_ctrl_write(p, buf, drq->wLength);
#ifdef DBG
	printk("%s: get_configuration --> %d\n", ID, buf[0]);
#endif

	return 0;

err:
#ifdef DBG
	printk("%s: bad get_configuration request\n", ID);
#endif
	return 1;
}

static int std_set_configuration(struct d12 *p)
{
	struct devreq *drq;
	int cfg;

	drq = &p->drq;

	if (drq->bmRequestType != (REQTYPE_DIR_OUT | REQTYPE_TYPE_STANDARD | REQTYPE_RECP_DEVICE))
		goto err;

	if ((drq->wValue & 0xFF00) || drq->wIndex || drq->wLength)
		goto err;

	cfg = drq->wValue & 0xFF;

	switch (p->fsm_dev) {
	case FSM_DEV_DEFAULT:
		goto err;

	case FSM_DEV_ADDRESS:
		if (cfg == CONFIG_VAL) {
			p->fsm_dev = FSM_DEV_CONFIGURED;
			p->fsm_bulk = FSM_BULK_IDLE;
			d12_set_endpoint_enable(p, 1);
			d12_stall_bulk(p, 0);
		} else if (cfg)
			goto err;
		break;

	case FSM_DEV_CONFIGURED:
		if (!cfg) {
			p->fsm_dev = FSM_DEV_ADDRESS;
			p->fsm_bulk = FSM_BULK_IDLE;
			d12_stall_bulk(p, 1);
			d12_set_endpoint_enable(p, 0);
		} else if (cfg != CONFIG_VAL)
			goto err;
		break;
	}

	ep_ctrl_ack(p);
#ifdef DBG
	printk("%s: set_configuration %d\n", ID, cfg);
#endif

	return 0;

err:
#ifdef DBG
	printk("%s: bad set_configuration request\n", ID);
#endif
	return 1;
}

static int std_get_interface(struct d12 *p)
{
	struct devreq *drq;
	unsigned char buf[1];

	drq = &p->drq;

	if (drq->bmRequestType != (REQTYPE_DIR_IN | REQTYPE_TYPE_STANDARD | REQTYPE_RECP_INTERFACE))
		goto err;

	if (drq->wValue || (drq->wLength != 1))
		goto err;

	switch (p->fsm_dev) {
	case FSM_DEV_DEFAULT:
	case FSM_DEV_ADDRESS:
		goto err;

	case FSM_DEV_CONFIGURED:
		if (!(drq->wIndex & 0xFF))	/* only one interface / alternative setting */
			buf[0] = 0;
		else
			goto err;
		break;
	}

	ep_ctrl_write(p, buf, drq->wLength);
#ifdef DBG
	printk("%s: get_interface --> %d\n", ID, buf[0]);
#endif

	return 0;

err:
#ifdef DBG
	printk("%s: bad get_interface request\n", ID);
#endif
	return 1;
}

static int std_set_interface(struct d12 *p)
{
	struct devreq *drq;

	drq = &p->drq;

	if (drq->bmRequestType != (REQTYPE_DIR_OUT | REQTYPE_TYPE_STANDARD | REQTYPE_RECP_INTERFACE))
		goto err;

	if (drq->wLength)
		goto err;

	switch (p->fsm_dev) {
	case FSM_DEV_DEFAULT:
	case FSM_DEV_ADDRESS:
		goto err;

	case FSM_DEV_CONFIGURED:
		if ((drq->wIndex & 0xFF) || drq->wValue)	/* only one interface / alternative setting */
			goto err;

		d12_stall_bulk(p, 0);

		break;
	}

	ep_ctrl_ack(p);
#ifdef DBG
	printk("%s: set_interface %d\n", ID, drq->wIndex & 0xFF);
#endif

	return 0;

err:
#ifdef DBG
	printk("%s: bad set_interface request\n", ID);
#endif
	return 1;
}

static int std_synch_frame(struct d12 *p)
{
#ifdef DBG
	printk("%s: unsupported synch_frame request\n", ID);
#endif
	return 1;
}

/* --- */

static int cls_mass_storage_reset(struct d12 *p)
{
	struct devreq *drq;

	drq = &p->drq;

	if (drq->bmRequestType != (REQTYPE_DIR_OUT | REQTYPE_TYPE_CLASS | REQTYPE_RECP_INTERFACE))
		goto err;

	if (drq->wValue || drq->wLength)
		goto err;

	if (drq->wIndex)			/* only one interface */
		goto err;

	p->fsm_bulk = FSM_BULK_IDLE;

	ep_ctrl_ack(p);
#ifdef DBG
	printk("%s: set_interface %d\n", ID, drq->wIndex & 0xFF);
#endif

	return 0;

err:
#ifdef DBG
	printk("%s: bad (class) mass_storage_reset request\n", ID);
#endif
	return 1;
}

static int cls_get_max_lun(struct d12 *p)
{
	struct devreq *drq;
	unsigned char buf[1];

	drq = &p->drq;

	if (drq->bmRequestType != (REQTYPE_DIR_IN | REQTYPE_TYPE_CLASS | REQTYPE_RECP_INTERFACE))
		goto err;

	if (drq->wValue || (drq->wLength != 1))
		goto err;

	if (drq->wIndex)			/* only one interface */
		goto err;

	buf[0] = p->max_lun - 1;

	ep_ctrl_write(p, buf, drq->wLength);
#ifdef DBG
	printk("%s: (class) get_max_lun --> %d\n", ID, buf[0]);
#endif

	return 0;

err:
#ifdef DBG
	printk("%s: bad (class) get_max_lun request\n", ID);
#endif
	return 1;
}

/* --- */

#define MAX_REQ_STANDARD 13

static int (* const T_STD_SETUP[MAX_REQ_STANDARD])(struct d12 *) = {
	std_get_status,
	std_clear_feature,
	std_reserved,
	std_set_feature,
	std_reserved,
	std_set_address,
	std_get_descriptor,
	std_set_descriptor,
	std_get_configuration,
	std_set_configuration,
	std_get_interface,
	std_set_interface,
	std_synch_frame
};

#define MAX_REQ_CLASS 2

static int (* const T_CLS_SETUP[MAX_REQ_CLASS])(struct d12 *) = {
	cls_mass_storage_reset,
	cls_get_max_lun
};

/* ------------------------------------------------------------------------- */

/*
 * bulk and quazi-SCSI
 */

#define BULK_NONE		0
#define BULK_OUT		0
#define BULK_IN			1

#define SCHD_ERR_PHASE		1
#define SCHD_ERR_INVALID_CDB	2
#define SCHD_ERR_INVALID_LBA	3

static void ep_bulk_write(struct d12 *p, const void *buf, int len)
{
	if (p->bxferptr + len > p->bxferlen)
		len = p->bxferlen - p->bxferptr;

	if (len > MAX_CHUNKBUF)
		len = MAX_CHUNKBUF;

	p->bbuf = (unsigned char *)buf;
	p->blen = len;
	p->bptr = 0;

	len = d12_buf_write(p, EPX_BULK_I, p->bbuf, len);
	p->bptr += len;
	p->bxferptr += len;

	p->fsm_bulk = FSM_BULK_DATA_IN;
}

static void ep_bulk_read(struct d12 *p, void *buf, int len)
{
	if (p->bxferptr + len > p->bxferlen)
		len = p->bxferlen - p->bxferptr;

	if (len > MAX_CHUNKBUF)
		len = MAX_CHUNKBUF;

	p->bbuf = (unsigned char *)buf;
	p->blen = len;
	p->bptr = 0;

	if (p->fsm_bulk == FSM_BULK_DATA_OUT_WAIT) {
		len = d12_buf_read(p, EPX_BULK_O, p->bbuf, len);
		p->bptr += len;
		p->bxferptr += len;
	}

	p->fsm_bulk = FSM_BULK_DATA_OUT;
}

/* --- */

static int bulk_xfer_chk(struct d12 *p, int dir, int len)
{
	struct cbw *cbw;
	struct csw *csw;
	int h, hi;

	cbw = &p->cbw;
	csw = &p->csw;

	h = cbw->dCBWDataTransferLength;
	hi = cbw->bmCBWFlags & CBW_FLAGS_DIR_IN;

	p->bxferlen = p->bxferptr = 0;

	if (dir == BULK_IN) {			/* Di */
		if (!h)				/* Hn < Di */
			goto phaserr;
		else if (hi) {			/* Hi ? Di */
			if (h >= len)		/* Hi >= Di */
				p->bxferlen = len;
			else 			/* Hi < Di */
				goto phaserr;
		} else				/* Ho <> Di */
			goto phaserr;
	} else if (len) {			/* Do */
		if (!h)				/* Hn < Do */
			goto phaserr;
		else if (hi)			/* Hi <> Do */
			goto phaserr;
		else {				/* Ho ? Do */
			if (h >= len)		/* Ho >= Do */
				p->bxferlen = len;
			else			/* Ho < Do */
				goto phaserr;
		}
	}

	return 0;

phaserr:
	csw->bCSWStatus = CSW_STAT_PHASE_ERROR;
	csw->dCSWDataResidue = h;
	return SCHD_ERR_PHASE;
}

static int bulk_xfer(struct d12 *p, int dir, const void *buf, int len)
{
	int r;

	r = bulk_xfer_chk(p, dir, len);
	if (!r && len) {
		if (dir == BULK_IN)
			ep_bulk_write(p, buf, len);
		else
			ep_bulk_read(p, (void *)buf, len);
	}

	return r;
}

static void csw_write(struct d12 *p)
{
	struct cbw *cbw;
	struct csw *csw;

	cbw = &p->cbw;
	csw = &p->csw;

	csw->dCSWDataResidue = cbw->dCBWDataTransferLength - p->bxferptr;
	if (csw->dCSWDataResidue) {
		if (cbw->bmCBWFlags & CBW_FLAGS_DIR_IN) {
			d12_status_set(p, EPX_BULK_I, 1);
			p->fsm_bulk = FSM_BULK_STALL_TO_CSW;
			return;
		} else
			d12_status_set(p, EPX_BULK_O, 1);
	}
	d12_buf_write(p, EPX_BULK_I, &p->csw, sizeof(struct csw));
	p->fsm_bulk = FSM_BULK_IDLE;
}

/* --- */

static int lba_chk(struct d12 *p, int lun, __u32 lba, __u32 blks)
{
	if (lba >= p->atd_lba_capacity[lun])
		return 1;

	if (lba + blks > p->atd_lba_capacity[lun])
		return 1;

	p->scsi_lba = lba;
	p->scsi_blks = blks;

	return 0;
}

static void scsi_sense_set(struct d12 *p, int skey, int asc)
{
	unsigned char *b;

	b = p->scsi_sense;

	memset (b, 0, MAX_SCSI_DATA_SENSE);

	b[0] = 0x70;				/* current error */
	b[2] = skey;
	b[7] = MAX_SCSI_DATA_SENSE - 8;
	b[12] = (asc >> 8) & 0xFF;
	b[13] = (asc >> 0) & 0xFF;
}

static void scsi_sense_lba_set(struct d12 *p, __u32 lba, int asc)
{
	unsigned char *b;

	p->csw.bCSWStatus = CSW_STAT_FAILED;
	scsi_sense_set(p, SCSI_SKEY_MEDIUM_ERROR, asc);

	b = p->scsi_sense;

	b[0] |= 0x80;				/* valid */
	put_32be(b + 3, lba);			/* information */
}

static int scsi_cmd_request_sense(struct d12 *p, int lun)
{
	int r;

	r = bulk_xfer(p, BULK_IN, p->scsi_sense, MAX_SCSI_DATA_SENSE);
	scsi_sense_set(p, SCSI_SKEY_NO_SENSE, SCSI_ASC_NO);

	return r;
}

static int scsi_cmd_test_unit_ready(struct d12 *p, int lun)
{
	if (p->atd_stat[lun] & ATD_STAT_READY) {
		scsi_sense_set(p, SCSI_SKEY_NO_SENSE, SCSI_ASC_NO);
	} else {
		p->csw.bCSWStatus = CSW_STAT_FAILED;
		scsi_sense_set(p, SCSI_SKEY_NOT_READY, SCSI_ASC_LOGICAL_UNIT_NOT_READY);
	}

	return bulk_xfer(p, BULK_NONE, 0, 0);
}

static int scsi_cmd_inquiry(struct d12 *p, int lun)
{
	unsigned char *b, *t;

	b = p->cbw.CBWCB;
	t = p->tmp;

	memset(t, 0, MAX_SCSI_DATA_INQUIRY);

	if ((b[1] & 0x03) || b[2])		/* no CmdDT & EVPD support */
		goto err;
	else {					/* standard */
//		t[0] = 0x00;			/* peripheral connected, direct-access device */
		t[1] = 0x80;			/* RMB = 1 */
		t[2] = 0x02;
		t[3] = 0x02;
		t[4] = MAX_SCSI_DATA_INQUIRY - 5;
		memcpy(t + 8, p->scsi_str_vendor[lun], 8);
		memcpy(t + 16, p->scsi_str_product[lun], 16);
		memcpy(t + 32, p->scsi_str_revision[lun], 4);
	}

	scsi_sense_set(p, SCSI_SKEY_NO_SENSE, SCSI_ASC_NO);

	return bulk_xfer(p, BULK_IN, t, MAX_SCSI_DATA_INQUIRY);

err:	return SCHD_ERR_INVALID_CDB;
}

static int scsi_cmd_read_capacity10(struct d12 *p, int lun)
{
	unsigned char *b, *t;

	b = p->cbw.CBWCB;
	t = p->tmp;

	memset(t, 0, MAX_SCSI_DATA_CAPACITY10);

	if ((b[1] & 0x01) || (b[8] & 0x01))	/* RELADR & PMI */
		goto err;

	put_32be(t + 0, p->atd_lba_capacity[lun] - 1);	/* last LBA */
	put_32be(t + 4, p->atd_blk[lun]);		/* block size */

	scsi_sense_set(p, SCSI_SKEY_NO_SENSE, SCSI_ASC_NO);

	return bulk_xfer(p, BULK_IN, t, MAX_SCSI_DATA_CAPACITY10);

err:	return SCHD_ERR_INVALID_CDB;
}

static int scsi_cmd_read_capacity16(struct d12 *p, int lun)
{
	unsigned char *b, *t;

	b = p->cbw.CBWCB;
	t = p->tmp;

	memset(t, 0, MAX_SCSI_DATA_CAPACITY16);

	if (((b[1] & 0x1F) != 0x10) || (b[14] & 0x03))	/* RELADR & PMI */
		goto err;

	put_64be(t + 0, p->atd_lba_capacity[lun] - 1);	/* last LBA */
	put_32be(t + 8, p->atd_blk[lun]);		/* block size */

	scsi_sense_set(p, SCSI_SKEY_NO_SENSE, SCSI_ASC_NO);

	return bulk_xfer(p, BULK_IN, t, MAX_SCSI_DATA_CAPACITY16);

err:	return SCHD_ERR_INVALID_CDB;
}

static void bhd_read(struct d12 *p, int lun, int len)
{
	volatile struct dioreq *dio;
	int blks;

	dio = &p->dio;

	blks = (p->scsi_blks > p->atd_chunkblk[lun]) ? p->atd_chunkblk[lun] : p->scsi_blks;

	if (blks) {
		dio->cmd = DIO_CMD_READ;
		dio->lun = lun;
		dio->lba = p->scsi_lba;
		dio->blks = blks;
		dio->buf = p->chunkbuf;
		up(&p->wait_dio_cmd);

		p->scsi_lba += blks;
		p->scsi_blks -= blks;
	}
}

static int do_read(struct d12 *p, int lun, __u32 lba, __u32 blks)
{
	volatile struct dioreq *dio;
	int r;

	if (lba_chk(p, lun, lba, blks))
		goto lbaerr;

	scsi_sense_set(p, SCSI_SKEY_NO_SENSE, SCSI_ASC_NO);

	dio = &p->dio;

	r = bulk_xfer_chk(p, BULK_IN, blks * p->atd_blk[lun]);
	if (!r && blks) {
		p->bhd = bhd_read;

		if (blks > p->atd_chunkblk[lun])
			blks = p->atd_chunkblk[lun];
		dio->cmd = DIO_CMD_READ;
		dio->lun = lun;
		dio->lba = lba;
		dio->blks = blks;
		dio->buf = p->chunkbuf;
		up(&p->wait_dio_cmd);

		p->scsi_lba += blks;
		p->scsi_blks -= blks;
	}

	return r;

lbaerr:	return SCHD_ERR_INVALID_LBA;
}

static int scsi_cmd_read6(struct d12 *p, int lun)
{
	unsigned char *b;
	__u32 lba, blks;

	b = p->cbw.CBWCB;

	lba = get_u32be(b + 0) & 0x001FFFFF;
	blks = b[4];
	if (!blks)
		blks = 256;

	return do_read(p, lun, lba, blks);
}

static int scsi_cmd_read10(struct d12 *p, int lun)
{
	unsigned char *b;
	__u32 lba, blks;

	b = p->cbw.CBWCB;

	if (b[1] & 0x01)			/* RELADR */
		goto err;

	lba = get_u32be(b + 2);
	blks = get_u16be(b + 7);

	return do_read(p, lun, lba, blks);

err:	return SCHD_ERR_INVALID_CDB;
}

static int scsi_cmd_read12(struct d12 *p, int lun)
{
	unsigned char *b;
	__u32 lba, blks;

	b = p->cbw.CBWCB;

	if (b[1] & 0x01)			/* RELADR */
		goto err;

	lba = get_u32be(b + 2);
	blks = get_u32be(b + 6);

	return do_read(p, lun, lba, blks);

err:	return SCHD_ERR_INVALID_CDB;
}

static int scsi_cmd_read16(struct d12 *p, int lun)
{
	unsigned char *b;
	__u32 lba, blks;

	b = p->cbw.CBWCB;

	if (b[1] & 0x01)			/* RELADR */
		goto err;

	lba = get_u64be(b + 2);
	blks = get_u32be(b + 10);

	return do_read(p, lun, lba, blks);

err:	return SCHD_ERR_INVALID_CDB;
}

static void bhd_write(struct d12 *p, int lun, int len)
{
	volatile struct dioreq *dio;
	int blks;

	dio = &p->dio;

	blks = len / p->atd_blk[lun];

	if (blks) {
		p->scsi_lba += blks;
		p->scsi_blks -= blks;

		dio->cmd = DIO_CMD_WRITE;
		dio->lun = lun;
		dio->lba = p->scsi_lba - blks;
		dio->blks = blks;
		dio->nblks = p->scsi_blks;
		dio->buf = p->chunkbuf;
		up(&p->wait_dio_cmd);
	}
}

static int do_write(struct d12 *p, int lun, __u32 lba, __u32 blks)
{
	int r;

	if (lba_chk(p, lun, lba, blks))
		goto lbaerr;

	scsi_sense_set(p, SCSI_SKEY_NO_SENSE, SCSI_ASC_NO);

	r = bulk_xfer_chk(p, BULK_OUT, blks * p->atd_blk[lun]);
	if (!r && blks) {
		p->bhd = bhd_write;

		ep_bulk_read(p, p->chunkbuf, blks * p->atd_blk[lun]);
	}

	return r;

lbaerr:	return SCHD_ERR_INVALID_LBA;
}

static int scsi_cmd_write6(struct d12 *p, int lun)
{
	unsigned char *b;
	__u32 lba, blks;

	b = p->cbw.CBWCB;

	lba = get_u32be(b + 0) & 0x001FFFFF;
	blks = b[4];
	if (!blks)
		blks = 256;

	return do_write(p, lun, lba, blks);
}

static int scsi_cmd_write10(struct d12 *p, int lun)
{
	unsigned char *b;
	__u32 lba, blks;

	b = p->cbw.CBWCB;

	if (b[1] & 0x01)			/* RELADR */
		goto err;

	lba = get_u32be(b + 2);
	blks = get_u16be(b + 7);

	return do_write(p, lun, lba, blks);

err:	return SCHD_ERR_INVALID_CDB;
}

static int scsi_cmd_write12(struct d12 *p, int lun)
{
	unsigned char *b;
	__u32 lba, blks;

	b = p->cbw.CBWCB;

	if (b[1] & 0x01)			/* RELADR */
		goto err;

	lba = get_u32be(b + 2);
	blks = get_u32be(b + 6);

	return do_write(p, lun, lba, blks);

err:	return SCHD_ERR_INVALID_CDB;
}

static int scsi_cmd_write16(struct d12 *p, int lun)
{
	unsigned char *b;
	__u32 lba, blks;

	b = p->cbw.CBWCB;

	if (b[1] & 0x01)			/* RELADR */
		goto err;

	lba = get_u64be(b + 2);
	blks = get_u32be(b + 10);

	return do_write(p, lun, lba, blks);

err:	return SCHD_ERR_INVALID_CDB;
}

static int do_verify(struct d12 *p, int lun, __u32 lba, __u32 blks)
{
	volatile struct dioreq *dio;
	int r;

	if (lba_chk(p, lun, lba, blks))
		goto lbaerr;

	scsi_sense_set(p, SCSI_SKEY_NO_SENSE, SCSI_ASC_NO);

	dio = &p->dio;

	r = bulk_xfer_chk(p, BULK_NONE, 0);
	if (!r && blks) {
		p->cswdelay = 1;

		dio->cmd = DIO_CMD_VERIFY;
		dio->lun = lun;
		dio->lba = lba;
		dio->blks = blks;
		up(&p->wait_dio_cmd);
	}

	return r;

lbaerr:	return SCHD_ERR_INVALID_LBA;
}

static int scsi_cmd_verify10(struct d12 *p, int lun)
{
	unsigned char *b;
	__u32 lba, blks;

	b = p->cbw.CBWCB;

	if (b[1] & 0x07)			/* BLKVFY, BYTCHK & RELADR */
		goto err;

	lba = get_u32be(b + 2);
	blks = get_u16be(b + 7);

	return do_verify(p, lun, lba, blks);

err:	return SCHD_ERR_INVALID_CDB;
}

static int scsi_cmd_verify12(struct d12 *p, int lun)
{
	unsigned char *b;
	__u32 lba, blks;

	b = p->cbw.CBWCB;

	if (b[1] & 0x07)			/* BLKVFY, BYTCHK & RELADR */
		goto err;

	lba = get_u32be(b + 2);
	blks = get_u32be(b + 6);

	return do_verify(p, lun, lba, blks);

err:	return SCHD_ERR_INVALID_CDB;
}

static int scsi_cmd_verify16(struct d12 *p, int lun)
{
	unsigned char *b;
	__u32 lba, blks;

	b = p->cbw.CBWCB;

	if (b[1] & 0x07)			/* BLKVFY, BYTCHK & RELADR */
		goto err;

	lba = get_u64be(b + 2);
	blks = get_u32be(b + 10);

	return do_verify(p, lun, lba, blks);

err:	return SCHD_ERR_INVALID_CDB;
}

/* --- */

struct scsi_cmd_hd {
	int len;
	int (*hd)(struct d12 *, int);
};

static const struct scsi_cmd_hd T_SCSI_HD[256] = {
	{6,	scsi_cmd_test_unit_ready},	/* 0x00 */
	{0,	0},				/* 0x01 */
	{0,	0},				/* 0x02 */
	{0,	scsi_cmd_request_sense},	/* 0x03 */
	{0,	0},				/* 0x04 */
	{0,	0},				/* 0x05 */
	{0,	0},				/* 0x06 */
	{0,	0},				/* 0x07 */
	{6,	scsi_cmd_read6},		/* 0x08 */
	{0,	0},				/* 0x09 */
	{6,	scsi_cmd_write6},		/* 0x0A */
	{0,	0},				/* 0x0B */
	{0,	0},				/* 0x0C */
	{0,	0},				/* 0x0D */
	{0,	0},				/* 0x0E */
	{0,	0},				/* 0x0F */
	{0,	0},				/* 0x10 */
	{0,	0},				/* 0x11 */
	{6,	scsi_cmd_inquiry},		/* 0x12 */
	{0,	0},				/* 0x13 */
	{0,	0},				/* 0x14 */
	{0,	0},				/* 0x15 */
	{0,	0},				/* 0x16 */
	{0,	0},				/* 0x17 */
	{0,	0},				/* 0x18 */
	{0,	0},				/* 0x19 */
	{0,	0},				/* 0x1A */
	{0,	0},				/* 0x1B */
	{0,	0},				/* 0x1C */
	{0,	0},				/* 0x1D */
	{0,	0},				/* 0x1E */
	{0,	0},				/* 0x1F */
	{0,	0},				/* 0x20 */
	{0,	0},				/* 0x21 */
	{0,	0},				/* 0x22 */
	{0,	0},				/* 0x23 */
	{0,	0},				/* 0x24 */
	{10,	scsi_cmd_read_capacity10},	/* 0x25 */
	{0,	0},				/* 0x26 */
	{0,	0},				/* 0x27 */
	{10,	scsi_cmd_read10},		/* 0x28 */
	{0,	0},				/* 0x29 */
	{10,	scsi_cmd_write10},		/* 0x2A */
	{0,	0},				/* 0x2B */
	{0,	0},				/* 0x2C */
	{0,	0},				/* 0x2D */
	{0,	0},				/* 0x2E */
	{10,	scsi_cmd_verify10},		/* 0x2F */
	{0,	0},				/* 0x30 */
	{0,	0},				/* 0x31 */
	{0,	0},				/* 0x32 */
	{0,	0},				/* 0x33 */
	{0,	0},				/* 0x34 */
	{0,	0},				/* 0x35 */
	{0,	0},				/* 0x36 */
	{0,	0},				/* 0x37 */
	{0,	0},				/* 0x38 */
	{0,	0},				/* 0x39 */
	{0,	0},				/* 0x3A */
	{0,	0},				/* 0x3B */
	{0,	0},				/* 0x3C */
	{0,	0},				/* 0x3D */
	{0,	0},				/* 0x3E */
	{0,	0},				/* 0x3F */
	{0,	0},				/* 0x40 */
	{0,	0},				/* 0x41 */
	{0,	0},				/* 0x42 */
	{0,	0},				/* 0x43 */
	{0,	0},				/* 0x44 */
	{0,	0},				/* 0x45 */
	{0,	0},				/* 0x46 */
	{0,	0},				/* 0x47 */
	{0,	0},				/* 0x48 */
	{0,	0},				/* 0x49 */
	{0,	0},				/* 0x4A */
	{0,	0},				/* 0x4B */
	{0,	0},				/* 0x4C */
	{0,	0},				/* 0x4D */
	{0,	0},				/* 0x4E */
	{0,	0},				/* 0x4F */
	{0,	0},				/* 0x50 */
	{0,	0},				/* 0x51 */
	{0,	0},				/* 0x52 */
	{0,	0},				/* 0x53 */
	{0,	0},				/* 0x54 */
	{0,	0},				/* 0x55 */
	{0,	0},				/* 0x56 */
	{0,	0},				/* 0x57 */
	{0,	0},				/* 0x58 */
	{0,	0},				/* 0x59 */
	{0,	0},				/* 0x5A */
	{0,	0},				/* 0x5B */
	{0,	0},				/* 0x5C */
	{0,	0},				/* 0x5D */
	{0,	0},				/* 0x5E */
	{0,	0},				/* 0x5F */
	{0,	0},				/* 0x60 */
	{0,	0},				/* 0x61 */
	{0,	0},				/* 0x62 */
	{0,	0},				/* 0x63 */
	{0,	0},				/* 0x64 */
	{0,	0},				/* 0x65 */
	{0,	0},				/* 0x66 */
	{0,	0},				/* 0x67 */
	{0,	0},				/* 0x68 */
	{0,	0},				/* 0x69 */
	{0,	0},				/* 0x6A */
	{0,	0},				/* 0x6B */
	{0,	0},				/* 0x6C */
	{0,	0},				/* 0x6D */
	{0,	0},				/* 0x6E */
	{0,	0},				/* 0x6F */
	{0,	0},				/* 0x70 */
	{0,	0},				/* 0x71 */
	{0,	0},				/* 0x72 */
	{0,	0},				/* 0x73 */
	{0,	0},				/* 0x74 */
	{0,	0},				/* 0x75 */
	{0,	0},				/* 0x76 */
	{0,	0},				/* 0x77 */
	{0,	0},				/* 0x78 */
	{0,	0},				/* 0x79 */
	{0,	0},				/* 0x7A */
	{0,	0},				/* 0x7B */
	{0,	0},				/* 0x7C */
	{0,	0},				/* 0x7D */
	{0,	0},				/* 0x7E */
	{0,	0},				/* 0x7F */
	{0,	0},				/* 0x80 */
	{0,	0},				/* 0x81 */
	{0,	0},				/* 0x82 */
	{0,	0},				/* 0x83 */
	{0,	0},				/* 0x84 */
	{0,	0},				/* 0x85 */
	{0,	0},				/* 0x86 */
	{0,	0},				/* 0x87 */
	{16,	scsi_cmd_read16},		/* 0x88 */
	{0,	0},				/* 0x89 */
	{16,	scsi_cmd_write16},		/* 0x8A */
	{0,	0},				/* 0x8B */
	{0,	0},				/* 0x8C */
	{0,	0},				/* 0x8D */
	{0,	0},				/* 0x8E */
	{16,	scsi_cmd_verify16},		/* 0x8F */
	{0,	0},				/* 0x90 */
	{0,	0},				/* 0x91 */
	{0,	0},				/* 0x92 */
	{0,	0},				/* 0x93 */
	{0,	0},				/* 0x94 */
	{0,	0},				/* 0x95 */
	{0,	0},				/* 0x96 */
	{0,	0},				/* 0x97 */
	{0,	0},				/* 0x98 */
	{0,	0},				/* 0x99 */
	{0,	0},				/* 0x9A */
	{0,	0},				/* 0x9B */
	{0,	0},				/* 0x9C */
	{0,	0},				/* 0x9D */
	{16,	scsi_cmd_read_capacity16},	/* 0x9E */
	{0,	0},				/* 0x9F */
	{0,	0},				/* 0xA0 */
	{0,	0},				/* 0xA1 */
	{0,	0},				/* 0xA2 */
	{0,	0},				/* 0xA3 */
	{0,	0},				/* 0xA4 */
	{0,	0},				/* 0xA5 */
	{0,	0},				/* 0xA6 */
	{0,	0},				/* 0xA7 */
	{12,	scsi_cmd_read12},		/* 0xA8 */
	{0,	0},				/* 0xA9 */
	{12,	scsi_cmd_write12},		/* 0xAA */
	{0,	0},				/* 0xAB */
	{0,	0},				/* 0xAC */
	{0,	0},				/* 0xAD */
	{0,	0},				/* 0xAE */
	{12,	scsi_cmd_verify12},		/* 0xAF */
	{0,	0},				/* 0xB0 */
	{0,	0},				/* 0xB1 */
	{0,	0},				/* 0xB2 */
	{0,	0},				/* 0xB3 */
	{0,	0},				/* 0xB4 */
	{0,	0},				/* 0xB5 */
	{0,	0},				/* 0xB6 */
	{0,	0},				/* 0xB7 */
	{0,	0},				/* 0xB8 */
	{0,	0},				/* 0xB9 */
	{0,	0},				/* 0xBA */
	{0,	0},				/* 0xBB */
	{0,	0},				/* 0xBC */
	{0,	0},				/* 0xBD */
	{0,	0},				/* 0xBE */
	{0,	0},				/* 0xBF */
	{0,	0},				/* 0xC0 */
	{0,	0},				/* 0xC1 */
	{0,	0},				/* 0xC2 */
	{0,	0},				/* 0xC3 */
	{0,	0},				/* 0xC4 */
	{0,	0},				/* 0xC5 */
	{0,	0},				/* 0xC6 */
	{0,	0},				/* 0xC7 */
	{0,	0},				/* 0xC8 */
	{0,	0},				/* 0xC9 */
	{0,	0},				/* 0xCA */
	{0,	0},				/* 0xCB */
	{0,	0},				/* 0xCC */
	{0,	0},				/* 0xCD */
	{0,	0},				/* 0xCE */
	{0,	0},				/* 0xCF */
	{0,	0},				/* 0xD0 */
	{0,	0},				/* 0xD1 */
	{0,	0},				/* 0xD2 */
	{0,	0},				/* 0xD3 */
	{0,	0},				/* 0xD4 */
	{0,	0},				/* 0xD5 */
	{0,	0},				/* 0xD6 */
	{0,	0},				/* 0xD7 */
	{0,	0},				/* 0xD8 */
	{0,	0},				/* 0xD9 */
	{0,	0},				/* 0xDA */
	{0,	0},				/* 0xDB */
	{0,	0},				/* 0xDC */
	{0,	0},				/* 0xDD */
	{0,	0},				/* 0xDE */
	{0,	0},				/* 0xDF */
	{0,	0},				/* 0xE0 */
	{0,	0},				/* 0xE1 */
	{0,	0},				/* 0xE2 */
	{0,	0},				/* 0xE3 */
	{0,	0},				/* 0xE4 */
	{0,	0},				/* 0xE5 */
	{0,	0},				/* 0xE6 */
	{0,	0},				/* 0xE7 */
	{0,	0},				/* 0xE8 */
	{0,	0},				/* 0xE9 */
	{0,	0},				/* 0xEA */
	{0,	0},				/* 0xEB */
	{0,	0},				/* 0xEC */
	{0,	0},				/* 0xED */
	{0,	0},				/* 0xEE */
	{0,	0},				/* 0xEF */
	{0,	0},				/* 0xF0 */
	{0,	0},				/* 0xF1 */
	{0,	0},				/* 0xF2 */
	{0,	0},				/* 0xF3 */
	{0,	0},				/* 0xF4 */
	{0,	0},				/* 0xF5 */
	{0,	0},				/* 0xF6 */
	{0,	0},				/* 0xF7 */
	{0,	0},				/* 0xF8 */
	{0,	0},				/* 0xF9 */
	{0,	0},				/* 0xFA */
	{0,	0},				/* 0xFB */
	{0,	0},				/* 0xFC */
	{0,	0},				/* 0xFD */
	{0,	0},				/* 0xFE */
	{0,	0}				/* 0xFF */
};

/* ------------------------------------------------------------------------- */

/*
 * interrupt handler and supporting functions
 */

static void ep0_rx(struct d12 *p)
{
	struct devreq *drq;
	int stat, len, req;

	stat = d12_read_last_trans(p, EPX_CTRL_O);

	if (!(stat & D_READ_LTRNS_SUCCESS))
		return;

	if (p->fsm_dev < FSM_DEV_DEFAULT)
		goto err;

	drq = &p->drq;

	if (stat & D_READ_LTRNS_SETUP) {	/* new transfer */
		len = d12_buf_read(p, EPX_CTRL_O, drq, sizeof(struct devreq));
		if (len != sizeof(struct devreq))
			goto err;

		d12_acknowledge_setup(p);
		d12_select_endpoint(p, EPX_CTRL_O);
		d12_buf_clear(p);

		req = drq->bRequest;

		switch (drq->bmRequestType & REQTYPE_TYPE_M) {
		case REQTYPE_TYPE_STANDARD:
			if (req >= MAX_REQ_STANDARD)
				goto err;
			if ((T_STD_SETUP[req])(p))
				goto err;
			break;

		case REQTYPE_TYPE_CLASS:
			if (p->fsm_dev != FSM_DEV_CONFIGURED)
				goto err;

			req = 255 - req;
			if (req >= MAX_REQ_CLASS)
				goto err;
			if ((T_CLS_SETUP[req])(p))
				goto err;
			break;

		default:
			goto err;
		}
	} else {				/* host -> d12 data */
		switch (p->fsm_ctl) {
		case FSM_CTL_STATUS_OUT:
			d12_stall_ctrl(p, 1);
			p->fsm_ctl = FSM_CTL_IDLE;
			break;

		default:
			goto err;
		}
	}

	return;

err:	d12_stall_ctrl(p, 1);			/* stall EP0 */
	p->fsm_ctl = FSM_CTL_IDLE;
}

static void ep0_tx(struct d12 *p)
{
	struct devreq *drq;
	int stat, len;

	stat = d12_read_last_trans(p, EPX_CTRL_I);

	if (!(stat & D_READ_LTRNS_SUCCESS))
		return;

	if (p->fsm_dev < FSM_DEV_DEFAULT)
		goto err;

	drq = &p->drq;

	switch (p->fsm_ctl) {
	case FSM_CTL_DATA_IN:
		len = p->dlen - p->dptr;
		if (len > EP_CTRL_SIZE)
			len = EP_CTRL_SIZE;

		d12_buf_write(p, EPX_CTRL_I, p->dbuf + p->dptr, len);
		p->dptr += len;

		if (p->dptr == p->dlen)
			p->fsm_ctl = FSM_CTL_STATUS_OUT;
		p->fsm_ctl_lastpkt = len;
		break;

	case FSM_CTL_STATUS_OUT:
		if (p->fsm_ctl_lastpkt == EP_CTRL_SIZE) {
			d12_buf_write(p, EPX_CTRL_I, 0, 0);
			p->fsm_ctl_lastpkt = 0;
		}
		break;

	case FSM_CTL_STATUS_IN:
		d12_stall_ctrl(p, 1);
		p->fsm_ctl = FSM_CTL_IDLE;
		break;

	default:
		goto err;
	}

	return;

err:	d12_stall_ctrl(p, 1);			/* stall EP0 */
	p->fsm_ctl = FSM_CTL_IDLE;
}

static void ep1_rx(struct d12 *p)		/* free endpoint */
{
	d12_read_last_trans(p, EPX_FREE_O);
	d12_stall_free(p, 1);			/* stall EP1 */
}

static void ep1_tx(struct d12 *p)		/* free endpoint */
{
	d12_read_last_trans(p, EPX_FREE_I);
	d12_stall_free(p, 1);			/* stall EP1 */
}

static void ep2_rx(struct d12 *p)
{
	struct cbw *cbw;
	struct csw *csw;
	int stat, len, lun, clen, cmd, r;

	stat = d12_read_last_trans(p, EPX_BULK_O);

	if (!(stat & D_READ_LTRNS_SUCCESS))
		return;

	if (p->fsm_dev != FSM_DEV_CONFIGURED)
		goto err;

	cbw = &p->cbw;
	csw = &p->csw;

	switch (p->fsm_bulk) {
	case FSM_BULK_IDLE:
		len = d12_buf_read(p, EPX_BULK_O, cbw, sizeof(struct cbw));
		if (len != sizeof(struct cbw))
			goto err;

		lun = cbw->bCBWLUN;
		clen = cbw->bCBWCBLength;
		cmd = cbw->CBWCB[0];

		if ((cbw->dCBWSignature != CBW_SIGNATURE) || !clen || (clen > sizeof(cbw->CBWCB)))
			goto err;

		p->bhd = 0;
		p->bxferlen = p->bxferptr = 0;
		p->cswdelay = p->cswearly = 0;

		csw->dCSWSignature = CSW_SIGNATURE;
		csw->dCSWTag = cbw->dCBWTag;
		csw->dCSWDataResidue = 0;
		csw->bCSWStatus = CSW_STAT_OK;

		if (lun >= p->max_lun) {
			csw->bCSWStatus = CSW_STAT_FAILED;
			scsi_sense_set(p, SCSI_SKEY_ILLEGAL_REQUEST, SCSI_ASC_LOGICAL_UNIT_NOT_SUPPORTED);
			bulk_xfer(p, BULK_NONE, 0, 0);
		} else if ((!T_SCSI_HD[cmd].len || (T_SCSI_HD[cmd].len == clen)) && T_SCSI_HD[cmd].hd) {
			r = T_SCSI_HD[cmd].hd(p, lun);
			if (r && (r != SCHD_ERR_PHASE)) {
				csw->bCSWStatus = CSW_STAT_FAILED;
				if (r == SCHD_ERR_INVALID_CDB)
					scsi_sense_set(p, SCSI_SKEY_ILLEGAL_REQUEST, SCSI_ASC_INVALID_FIELD_IN_CDB);
				else if (r == SCHD_ERR_INVALID_LBA)
					scsi_sense_set(p, SCSI_SKEY_ILLEGAL_REQUEST, SCSI_ASC_LBA_OUT_OF_RANGE);
				bulk_xfer(p, BULK_NONE, 0, 0);
			}
		} else {
			csw->bCSWStatus = CSW_STAT_FAILED;
			scsi_sense_set(p, SCSI_SKEY_ILLEGAL_REQUEST, SCSI_ASC_INVALID_CMD);
			bulk_xfer(p, BULK_NONE, 0, 0);
		}

		if (!p->bxferlen && !p->cswdelay)
			csw_write(p);
		break;

	case FSM_BULK_DATA_OUT:
		len = p->blen - p->bptr;
		len = d12_buf_read(p, EPX_BULK_O, p->bbuf + p->bptr, len);
		p->bptr += len;
		p->bxferptr += len;
		if (p->bptr == p->blen) {
			lun = cbw->bCBWLUN;
			if (p->bhd)
				p->bhd(p, lun, p->blen);
			p->fsm_bulk = FSM_BULK_DATA_OUT_WAIT;
		}
		break;

	case FSM_BULK_DATA_OUT_WAIT:
		break;

	default:
		goto err;
	}

	return;

err:	d12_stall_bulk(p, 1);			/* stall EP2 */
	p->fsm_bulk = FSM_BULK_IDLE;
}

static void ep2_tx(struct d12 *p)
{
	struct cbw *cbw;
	int stat, len, lun;

	stat = d12_read_last_trans(p, EPX_BULK_I);

	if (!(stat & D_READ_LTRNS_SUCCESS))
		return;

	if (p->fsm_dev != FSM_DEV_CONFIGURED)
		goto err;

	cbw = &p->cbw;

	switch (p->fsm_bulk) {
	case FSM_BULK_IDLE:			/* CSW ack */
		break;

	case FSM_BULK_DATA_IN:
		len = p->blen - p->bptr;
		if (len) {
			len = d12_buf_write(p, EPX_BULK_I, p->bbuf + p->bptr, len);
			p->bptr += len;
			p->bxferptr += len;
		} else {
			stat = d12_status_get(p, EPX_BULK_I);
			stat &= D_READ_ENDP_STAT_BUF0 | D_READ_ENDP_STAT_BUF1;
			if (!stat) {
				if ((p->bxferptr == p->bxferlen) || p->cswearly)
					csw_write(p);
				else {
					p->fsm_bulk = FSM_BULK_DATA_IN_WAIT;
					lun = cbw->bCBWLUN;
					if (p->bhd)
						p->bhd(p, lun, p->blen);
				}
			}
		}
		break;

	default:
		goto err;
	}

	return;

err:	d12_stall_bulk(p, 1);			/* stall EP2 */
	p->fsm_bulk = FSM_BULK_IDLE;
}

static void usbd12_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct d12 *p;
	int ints;

	p = (struct d12 *)dev_id;

	ints = d12_in2(p, CMD_READ_INT);

	if (ints & D_READ_INT_BUSRESET) {
		d12_reset(p);
		p->feat_wakeup = 0;
		p->fsm_dev = FSM_DEV_DEFAULT;
		p->fsm_ctl = FSM_CTL_IDLE;
	}

	if (ints & D_READ_INT_SUSPENDCHG)
		p->suspend = GPIO_D12_SUSP;

	if (ints & D_READ_INT_EP0_I)		/* host <- d12 ack, control */
		ep0_tx(p);

	if (ints & D_READ_INT_EP0_O)		/* host -> d12, control */
		ep0_rx(p);

	if (ints & D_READ_INT_EP1_I)		/* host <- d12 ack, bulk */
		ep1_tx(p);

	if (ints & D_READ_INT_EP1_O)		/* host -> d12, bulk */
		ep1_rx(p);

	if (ints & D_READ_INT_EP2_I)		/* host <- d12 ack, bulk */
		ep2_tx(p);

	if (ints & D_READ_INT_EP2_O)		/* host -> d12, bulk */
		ep2_rx(p);
}

/* ------------------------------------------------------------------------- */

/*
 * device I/O thread
 */

static int dio_thread(void *arg)
{
	struct d12 *p;
	volatile struct dioreq *dio;
	int cmd, lun;
	__u32 lba, blks, nblks, i;
	unsigned char *buf;
	struct buffer_head *bh;

	p = (struct d12 *)arg;
	dio = &p->dio;

	daemonize();
	reparent_to_init();
	strcpy(current->comm, "kusbd12");
	current->nice = -20;
#if 0
	spin_lock_irq(&current->sigmask_lock);
	sigemptyset(&current->blocked);
	recalc_sigpending(current);
	spin_unlock_irq(&current->sigmask_lock);
#endif

	complete(&p->cpl_dio_sync);		/* initialized */

	for ( ; ; ) {
		down(&p->wait_dio_cmd);
#if 0
		if (signal_pending(current)) {
			spin_lock_irq(&current->sigmask_lock);
			flush_signals(current);
			spin_unlock_irq(&current->sigmask_lock);
		}
#endif
		d12_lock(p);
		cmd = dio->cmd;
		lun = dio->lun;
		lba = dio->lba;
		blks = dio->blks;
		nblks = dio->nblks;
		buf = (unsigned char *)dio->buf;
		d12_unlock(p);

		if (cmd == DIO_CMD_EXIT)
			break;

		switch (cmd) {
		case DIO_CMD_READ:
			for (i = 0 ; i < blks ; i++) {
				bh = bread(p->atd_dev[lun], lba, p->atd_blk[lun]);
				if (!bh)
					break;
				memcpy(buf + i * p->atd_blk[lun], bh->b_data, p->atd_blk[lun]);
				brelse(bh);
				lba++;
			}
			
			d12_lock(p);
			ep_bulk_write(p, buf, i * p->atd_blk[lun]);
			if (i != blks) {
				scsi_sense_lba_set(p, lba, SCSI_ASC_UNRECOVERED_READ_ERROR);
				p->cswearly = 1;
			}
			d12_unlock(p);
			break;

		case DIO_CMD_WRITE:
			for (i = 0 ; i < blks ; i++) {
				bh = getblk(p->atd_dev[lun], lba, p->atd_blk[lun]);
				if (!bh)
					break;
				memcpy(bh->b_data, buf + i * p->atd_blk[lun], p->atd_blk[lun]);
				mark_buffer_uptodate(bh, 1);
				mark_buffer_dirty(bh);
				ll_rw_block(WRITE, 1, &bh);
				wait_on_buffer(bh);
				brelse(bh);
				lba++;
			}

			d12_lock(p);
			if (i != blks) {
				scsi_sense_lba_set(p, lba, SCSI_ASC_WRITE_ERROR);
				csw_write(p);
			} else if (nblks)
				ep_bulk_read(p, buf, nblks * p->atd_blk[lun]);
			else
				csw_write(p);
			d12_unlock(p);
			break;

		case DIO_CMD_VERIFY:
			for (i = 0 ; i < blks ; i++) {
				bh = bread(p->atd_dev[lun], lba, p->atd_blk[lun]);
				if (!bh)
					goto vfyerr;
				brelse(bh);
				lba++;
			}

vfyerr:			d12_lock(p);
			if (i != blks)
				scsi_sense_lba_set(p, lba, SCSI_ASC_UNRECOVERED_READ_ERROR);
			csw_write(p);
			d12_unlock(p);
			break;
		}
	}

	complete_and_exit(&p->cpl_dio_sync, 0);
}

/* ------------------------------------------------------------------------- */

#define w(x) ((x) & 0xFF), (((x) >> 8) & 0xFF)
#define u(x) (x), 0

#define MAX_DESC_STR		6

#define STR_MANUFACTURER	1
#define STR_PRODUCT		2
#define STR_SERIALNUM		3
#define STR_CONFIG		4
#define STR_INTERFACE		5

static const unsigned char T_DESC_STR_0[] = {
	4,				/* bLength */
	DESC_TYPE_STRING,		/* bDescriptorType */
	w(DEF_LANG)			/* (english, US) */
};

static const unsigned char T_DESC_STR_1[] = {
	26,				/* bLength */
	DESC_TYPE_STRING,		/* bDescriptorType */
	u('C'),				/* (manufacturer) */
	u('i'),
	u('r'),
	u('r'),
	u('u'),
	u('s'),
	u(' '),
	u('L'),
	u('o'),
	u('g'),
	u('i'),
	u('c')
};

static const unsigned char T_DESC_STR_2[] = {
	16,				/* bLength */
	DESC_TYPE_STRING,		/* bDescriptorType */
	u('e'),				/* (product) */
	u('d'),
	u('b'),
	u('7'),
	u('3'),
	u('1'),
	u('2')
};

static const unsigned char T_DESC_STR_3[] = {
	26,				/* bLength */
	DESC_TYPE_STRING,		/* bDescriptorType */
	u('0'),				/* (serial number) */
	u('0'),
	u('0'),
	u('0'),
	u('0'),
	u('0'),
 	u('0'),
	u('0'),
	u('0'),
	u('0'),
	u('0'),
	u('0')
};

static const unsigned char T_DESC_STR_4[] = {
	10,				/* bLength */
	DESC_TYPE_STRING,		/* bDescriptorType */
	u('c'),				/* (configuration) */
	u('f'),
	u('g'),
	u('0')
};

static const unsigned char T_DESC_STR_5[] = {
	8,				/* bLength */
	DESC_TYPE_STRING,		/* bDescriptorType */
	u('i'),				/* (interface) */
	u('f'),
	u('0')
};

static const void * const T_DESC_STR[MAX_DESC_STR] = {
	T_DESC_STR_0,
	T_DESC_STR_1,
	T_DESC_STR_2,
	T_DESC_STR_3,
	T_DESC_STR_4,
	T_DESC_STR_5
};

static const int T_DESC_STR_LEN[MAX_DESC_STR] = {
	sizeof(T_DESC_STR_0),
	sizeof(T_DESC_STR_1),
	sizeof(T_DESC_STR_2),
	sizeof(T_DESC_STR_3),
	sizeof(T_DESC_STR_4),
	sizeof(T_DESC_STR_5)
};

static const unsigned char T_DESC_DEV[] = {
	/* --- device descriptor --- */

	18,				/* bLength */
	DESC_TYPE_DEVICE,		/* bDescriptorType */
	0x01, 0x01,			/* bcdUSB */
	0,				/* bDeviceClass */
	0,				/* bDeviceSubClass */
	0,				/* bDeviceProtocol */
	EP_CTRL_SIZE,			/* bMaxPacketSize0 */
	0xDB, 0xCE,			/* idVendor */
	0x12, 0x73,			/* idProduct */
	0x00, 0x01,			/* bcdDevice */
	STR_MANUFACTURER,		/* iManufacturer */
	STR_PRODUCT,			/* iProduct */
	STR_SERIALNUM,			/* iSerialNumber */
	1				/* bNumConfigurations */
};

static const unsigned char T_DESC_CFG[] = {
	/* --- configuration descriptor --- */

	9,				/* bLength */
	DESC_TYPE_CONFIGURATION,	/* bDescriptorType */
	w(9 + 9 + 7 + 7),		/* wTotalLength */
	1,				/* bNumInterfaces */
	CONFIG_VAL,			/* bConfigurationValue */
	STR_CONFIG,			/* iConfiguration */
	0xE0,				/* bmAttributes */
	0,				/* MaxPower (0 mA) */

	/* --- interface descriptor --- */

	9,				/* bLength */
	DESC_TYPE_INTERFACE,		/* bDescriptorType */
	0,				/* bInterfaceNumber */
	0,				/* bAlternateSetting */
	2,				/* bNumEndpoints */
	CLASS_MASS_STORAGE,		/* bInterfaceClass (mass-storage) */
	SUBCLASS_SCSI,			/* bInterfaceSubClass (scsi) */
	PROTOCOL_BULK,			/* bInterfaceProtocol (bulk) */
	STR_INTERFACE,			/* iInterface */

	/* --- out endpoint descriptor --- */

	7,				/* bLength */
	DESC_TYPE_ENDPOINT,		/* bDescriptorType */
	EP_BULK | 0x00,			/* bEndpointAddress */
	0x02,				/* bmAttributes (bulk) */
	w(EP_BULK_SIZE),		/* wMaxPacketSize */
	0,				/* bInterval */

	/* --- in endpoint descriptor --- */

	7,				/* bLength */
	DESC_TYPE_ENDPOINT,		/* bDescriptorType */
	EP_BULK | 0x80,			/* bEndpointAddress */
	0x02,				/* bmAttributes (bulk) */
	w(EP_BULK_SIZE),		/* wMaxPacketSize */
	0				/* bInterval */
};

#undef w
#undef u

static const char T_STR_SCSI_VENDOR[] = "CIRRUSL ";
static const char T_STR_SCSI_PRODUCT[] = "EDB7312         ";
static const char T_STR_SCSI_REVISION[] = "0000";

/* --- */

static int thread_init(struct d12 *p)
{
	int r;

	init_completion(&p->cpl_dio_sync);
	sema_init(&p->wait_dio_cmd, 0);

	r = kernel_thread(dio_thread, p, 0);
	if (r < 0) {
		printk("%s: couldn't spawn thread\n", __FUNCTION__);
		return r;
	}

	wait_for_completion(&p->cpl_dio_sync);	/* wait thread initialize */

	return 0;
}

static void thread_exit(struct d12 *p)
{
	p->dio.cmd = DIO_CMD_EXIT;
	up(&p->wait_dio_cmd);

	wait_for_completion(&p->cpl_dio_sync);	/* wait thread exit */
}

static void dev_add(struct d12 *p, kdev_t dev)
{
	int lun, maj, min;

	lun = p->max_lun;
	if (lun == 16) {
		printk("ignored, limit of 16 reached\n");
		goto err0;
	}

	maj = MAJOR(dev);
	min = MINOR(dev);

	printk("%s: dev %u:%u - ", ID, maj, min);

	if (is_mounted(dev)) {
		printk("ignored, mounted\n");
		goto err0;
	}

	p->bdev[lun] = bdget(dev);
	if (!p->bdev[lun]) {
		printk("ignored, no memory\n");
		goto err0;
	}
	if (blkdev_get(p->bdev[lun], FMODE_READ | FMODE_WRITE, 0, BDEV_RAW)) {
		printk("ignored, blkdev_get() error\n");
		goto err0;
	}

	p->scsi_str_vendor[lun] = T_STR_SCSI_VENDOR;
	p->scsi_str_product[lun] = T_STR_SCSI_PRODUCT;
	p->scsi_str_revision[lun] = T_STR_SCSI_REVISION;

	p->atd_dev[lun] = dev;
	p->atd_blk[lun] = get_hardsect_size(dev);
	if (!p->atd_blk[lun] || (p->atd_blk[lun] > MAX_CHUNKBUF)) {
		printk("ignored (bogus sector size %u)\n", p->atd_blk[lun]);
		goto err1;
	}
	if (set_blocksize(dev, p->atd_blk[lun])) {
		printk("ignored, set_blocksize() error\n");
		goto err1;
	}
	p->atd_chunkblk[lun] = MAX_CHUNKBUF / p->atd_blk[lun];
	p->atd_lba_capacity[lun] = 0;
	if (blk_size[maj])
		p->atd_lba_capacity[lun] = blk_size[maj][min];
	if (!p->atd_lba_capacity[lun]) {
		printk("ignored (zero size)\n");
		goto err1;
	}

	p->atd_stat[lun] = ATD_STAT_READY;

	printk("added %s\n", (p->atd_stat[lun] & ATD_STAT_RO) ? "read-only" : "read-write");
	p->max_lun++;

	return;

err1:	blkdev_put(p->bdev[lun], BDEV_RAW);

err0:	return;
}

static void dev_del(struct d12 *p)
{
	int lun;

	for (lun = 0 ; lun < p->max_lun ; lun++) {
		blkdev_put(p->bdev[lun], BDEV_RAW);
	}
}

int __init usbd12_init_module(void)
{
	struct d12 *p;
	int r = -EINVAL;

	printk("PDIUSBD12 / mass-storage support for EDB7312\n");

	/* prepare context */

	p = &d12;
	p->reg_cmd = REG_D12_CMD;
	p->reg_dat = REG_D12_DAT;
	p->irq = IRQ_D12;

	p->desc_dev = T_DESC_DEV;
	p->desc_dev_len = sizeof(T_DESC_DEV);

	p->desc_cfg = T_DESC_CFG;
	p->desc_cfg_len = sizeof(T_DESC_CFG);

	p->desc_str_max = MAX_DESC_STR;
	p->desc_str = T_DESC_STR;
	p->desc_str_len = T_DESC_STR_LEN;

	/* attach devices */

	dev_add(p, MKDEV(1, 1));		/* /dev/ram1 */
#if 0
	dev_add(p, MKDEV(3, 1));		/* /dev/hda1 */
	dev_add(p, MKDEV(3, 65));		/* /dev/hdb1 */
#endif

	if (!p->max_lun) {
		printk("%s: no devices attached, skipping driver loading\n", ID);
		goto err0;
	}

	r = thread_init(p);
	if (r)
		goto err1;

	d12_init(p);
	p->suspend = GPIO_D12_SUSP;

	r = request_irq(p->irq, usbd12_interrupt, SA_SHIRQ, "pdiusbd12", p);
	if (r) {
		printk("%s: couldn't allocate IRQ %u\n", ID, p->irq);
		goto err2;
	}
        edb7312_enable_eint1();

	return 0;

err2:	thread_exit(p);
err1:	dev_del(p);
err0:	return r;
}

static void __exit usbd12_exit_module(void)
{
	struct d12 *p;

	p = &d12;

	free_irq(p->irq, p);
	thread_exit(p);
	dev_del(p);
}

module_init(usbd12_init_module);
module_exit(usbd12_exit_module);

MODULE_AUTHOR("Vladimir Ivanov <vladitx@nucleusys.com>");
MODULE_DESCRIPTION("PDIUSBD12 / mass-storage support for EDB7312");
MODULE_LICENSE("GPL");
