/* dma.c: DMA controller management on FR401 and the like
 *
 * Copyright (C) 2004 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <asm/dma.h>
#include <asm/gpio-regs.h>
#include <asm/irc-regs.h>
#include <asm/cpu-irqs.h>

struct frv_dma_channel {
	uint8_t			flags;
#define FRV_DMA_FLAGS_AVAILABLE 0x00
#define FRV_DMA_FLAGS_RESERVED	0x01
#define FRV_DMA_FLAGS_INUSE	0x02
#define FRV_DMA_FLAGS_PAUSED	0x04
#define FRV_DMA_FLAGS_DREQ	0x08		/* setting of SIR bit */
#define FRV_DMA_FLAGS_DACK	0x10		/* setting of SOR bit */
#define FRV_DMA_FLAGS_DONE	0x20		/* setting of SOR bit */
#define FRV_DMA_IOBITS		0x38		/* mask for SIR & SOR sttngs */
	uint8_t			cap;		/* capabilities available */
	int			irq;		/* completion IRQ */
	uint32_t		sirdreqbit;		
	uint32_t		sordackbit;		
	uint32_t		sordonebit;		
	uint32_t		gpdrdreqbit;		
	uint32_t		gpdrdackbit;		
	uint32_t		gpdrdonebit;		
	int			cnflct_msk;	/* possible channel conflict */
	int			cnflct_chn;	/* possible channel conflict */
	int			cnflct_chnmsk;	/* possible channel conflict */
	const unsigned long	ioaddr;		/* DMA controller regs addr */
	const char		*devname;
	dma_irq_handler_t	handler;
	void			*data;
};


#define __get_DMAC(IO,X)	({ *(volatile unsigned long *)((IO) + DMAC_##X##x); })

#define __set_DMAC(IO,X,V)					\
do {								\
	*(volatile unsigned long *)((IO) + DMAC_##X##x) = (V);	\
	mb();							\
} while(0)

#define ___set_DMAC(IO,X,V)					\
do {								\
	*(volatile unsigned long *)((IO) + DMAC_##X##x) = (V);	\
} while(0)


static struct frv_dma_channel frv_dma_channels[FRV_DMA_NCHANS] = {
	[0] = {
		.cap		= FRV_DMA_CAP_DREQ | FRV_DMA_CAP_DACK | FRV_DMA_CAP_DONE,
		.irq		= IRQ_CPU_DMA0,
		.sirdreqbit	= SIR_DREQ0_INPUT,
		.sordackbit	= SOR_DACK0_OUTPUT,
		.sordonebit	= SOR_DONE0_OUTPUT,
		.gpdrdreqbit	= GPDR_DREQ0_BIT,
		.gpdrdackbit	= GPDR_DACK0_BIT,
		.gpdrdonebit	= GPDR_DONE0_BIT,
		.cnflct_msk	= FRV_DMA_FLAGS_DONE,
		.cnflct_chn	= 4,
		.cnflct_chnmsk	= FRV_DMA_FLAGS_DREQ,
		.ioaddr		= 0xfe000900,
	},
	[1] = {
		.cap		= FRV_DMA_CAP_DREQ | FRV_DMA_CAP_DACK | FRV_DMA_CAP_DONE,
		.irq		= IRQ_CPU_DMA1,
		.sirdreqbit	= SIR_DREQ1_INPUT,
		.sordackbit	= SOR_DACK1_OUTPUT,
		.sordonebit	= SOR_DONE1_OUTPUT,
		.gpdrdreqbit	= GPDR_DREQ1_BIT,
		.gpdrdackbit	= GPDR_DACK1_BIT,
		.gpdrdonebit	= GPDR_DONE1_BIT,
		.cnflct_msk	= FRV_DMA_FLAGS_DONE,
		.cnflct_chn	= 5,
		.cnflct_chnmsk	= FRV_DMA_FLAGS_DREQ,
		.ioaddr		= 0xfe000980,
	},
	[2] = {
		.cap		= FRV_DMA_CAP_DREQ | FRV_DMA_CAP_DACK,
		.irq		= IRQ_CPU_DMA2,
		.sirdreqbit	= SIR_DREQ2_INPUT,
		.sordackbit	= SOR_DACK2_OUTPUT,
		.gpdrdreqbit	= GPDR_DREQ2_BIT,
		.gpdrdackbit	= GPDR_DACK2_BIT,
		.cnflct_msk	= FRV_DMA_FLAGS_DACK,
		.cnflct_chn	= 6,
		.cnflct_chnmsk	= FRV_DMA_FLAGS_DREQ,
		.ioaddr		= 0xfe000a00,
	},
	[3] = {
		.cap		= FRV_DMA_CAP_DREQ | FRV_DMA_CAP_DACK,
		.irq		= IRQ_CPU_DMA3,
		.sirdreqbit	= SIR_DREQ3_INPUT,
		.sordackbit	= SOR_DACK3_OUTPUT,
		.gpdrdreqbit	= GPDR_DREQ3_BIT,
		.gpdrdackbit	= GPDR_DACK3_BIT,
		.cnflct_msk	= FRV_DMA_FLAGS_DACK,
		.cnflct_chn	= 7,
		.cnflct_chnmsk	= FRV_DMA_FLAGS_DREQ,
		.ioaddr		= 0xfe000a80,
	},
	[4] = {
		.cap		= FRV_DMA_CAP_DREQ,
		.irq		= IRQ_CPU_DMA4,
		.sirdreqbit	= SIR_DREQ4_INPUT,
		.gpdrdreqbit	= GPDR_DREQ4_BIT,
		.cnflct_msk	= FRV_DMA_FLAGS_DREQ,
		.cnflct_chn	= 0,
		.cnflct_chnmsk	= FRV_DMA_FLAGS_DONE,
		.ioaddr		= 0xfe001000,
	},
	[5] = {
		.cap		= FRV_DMA_CAP_DREQ,
		.irq		= IRQ_CPU_DMA5,
		.sirdreqbit	= SIR_DREQ5_INPUT,
		.gpdrdreqbit	= GPDR_DREQ5_BIT,
		.cnflct_msk	= FRV_DMA_FLAGS_DREQ,
		.cnflct_chn	= 1,
		.cnflct_chnmsk	= FRV_DMA_FLAGS_DONE,
		.ioaddr		= 0xfe001080,
	},
	[6] = {
		.cap		= FRV_DMA_CAP_DREQ,
		.irq		= IRQ_CPU_DMA6,
		.sirdreqbit	= SIR_DREQ6_INPUT,
		.gpdrdreqbit	= GPDR_DREQ6_BIT,
		.cnflct_msk	= FRV_DMA_FLAGS_DREQ,
		.cnflct_chn	= 2,
		.cnflct_chnmsk	= FRV_DMA_FLAGS_DACK,
		.ioaddr		= 0xfe001100,
	},
	[7] = {
		.cap		= FRV_DMA_CAP_DREQ,
		.irq		= IRQ_CPU_DMA7,
		.sirdreqbit	= SIR_DREQ7_INPUT,
		.gpdrdreqbit	= GPDR_DREQ7_BIT,
		.cnflct_msk	= FRV_DMA_FLAGS_DREQ,
		.cnflct_chn	= 3,
		.cnflct_chnmsk	= FRV_DMA_FLAGS_DACK,
		.ioaddr		= 0xfe001180,
	},
};

static rwlock_t frv_dma_channels_lock = RW_LOCK_UNLOCKED;

unsigned long frv_dma_inprogress;

#define frv_clear_dma_inprogress(channel) \
	atomic_clear_mask(1 << (channel), &frv_dma_inprogress);

#define frv_set_dma_inprogress(channel) \
	atomic_set_mask(1 << (channel), &frv_dma_inprogress);

/*****************************************************************************/
/*
 * DMA irq handler - determine channel involved, grab status and call real handler
 */
static void dma_irq_handler(int irq, void *_channel, struct pt_regs *regs)
{
	struct frv_dma_channel *channel = _channel;

	frv_clear_dma_inprogress(channel - frv_dma_channels);
	channel->handler(channel - frv_dma_channels,
			 __get_DMAC(channel->ioaddr, CSTR),
			 channel->data,
			 regs);
} /* end dma_irq_handler() */



/*****************************************************************************/
/*
 * Determine which DMA controllers are present on this CPU
 */
void __init frv_dma_init(void)
{
	unsigned long psr = __get_PSR();
	int i;
	int num_dma;
	uint32_t val;

	/* First, determine how many DMA channels are available */
	switch (PSR_IMPLE(psr)) {

	case PSR_IMPLE_FR405:
	case PSR_IMPLE_FR451:
	case PSR_IMPLE_FR501:
	case PSR_IMPLE_FR551:
		num_dma = FRV_DMA_8CHANS;
		break;

	case PSR_IMPLE_FR401:
	default:
		num_dma = FRV_DMA_4CHANS;
		break;
	}

	/* Now mark all of the non-existent channels as reserved */
	for(i = num_dma; i < FRV_DMA_NCHANS; i++) {
		frv_dma_channels[i].flags = FRV_DMA_FLAGS_RESERVED;
	}

	/* Set SIR/SOR bits to an initial state so that H/W conflicts */
	/* are not created as we allocate DMA channels */
	val = __get_SIR();
	val &= ~SIR_DREQ_BITS;
	__set_SIR(val);

	val = __get_SOR();
	val &= ~(SOR_DONE_BITS | SOR_DACK_BITS);
	__set_SOR(val);
}

/*****************************************************************************/
/*
 * allocate a DMA controller channel and the IRQ associated with it
 */
int frv_dma_open(const char *devname,
		 unsigned long dmamask,
		 int dmacap,
		 dma_irq_handler_t handler,
		 unsigned long irq_flags,
		 void *data)
{
	struct frv_dma_channel *channel;
	int dma, ret;
	uint32_t val, gpdr;
	uint8_t flags = 0;
	int other;

	if(dmacap & FRV_DMA_CAP_DREQ)
		flags |= FRV_DMA_FLAGS_DREQ;
	if(dmacap & FRV_DMA_CAP_DACK)
		flags |= FRV_DMA_FLAGS_DACK;
	if(dmacap & FRV_DMA_CAP_DONE)
		flags |= FRV_DMA_FLAGS_DONE;

	write_lock(&frv_dma_channels_lock);

	ret = -ENOSPC;

	for (dma = FRV_DMA_NCHANS - 1; dma >= 0; dma--) {
		channel = &frv_dma_channels[dma];

		if (!test_bit(dma, &dmamask))
			continue;

		if ((channel->cap & dmacap) != dmacap)
			continue;

		if (frv_dma_channels[dma].flags)
			continue;

		/* check for possible conflict in settings of SIR/SOR */
		/* registers.  A GPIO pin can not be used for input and */
		/* output simultaneously */
		if(flags & frv_dma_channels[dma].cnflct_msk) {
			other = frv_dma_channels[dma].cnflct_chn;
			if((frv_dma_channels[dma].cnflct_chnmsk &
			    frv_dma_channels[other].flags) == 0)
				/* no conflict */
				goto found;
		} else {
			goto found;
		}
	}

	goto out;

 found:
	ret = request_irq(channel->irq, dma_irq_handler, irq_flags, devname, channel);
	if (ret < 0)
		goto out;

	/* okay, we've allocated all the resources */
	channel = &frv_dma_channels[dma];

	channel->flags		|= (FRV_DMA_FLAGS_INUSE | flags);
	channel->devname	= devname;
	channel->handler	= handler;
	channel->data		= data;

	/* Now make sure we are set up for DMA and not GPIO */
	/* SIR bit must be set for DMA to work */
	val = __get_SIR();
	gpdr = __get_GPDR();
	if(dmacap & FRV_DMA_CAP_DREQ) {
		val |= channel->sirdreqbit;
		gpdr &= ~channel->gpdrdreqbit;
	}
	else
		val &= ~channel->sirdreqbit;
	__set_SIR(val);

	/* SOR bits depend on what the caller requests */
	val = __get_SOR();
	if(dmacap & FRV_DMA_CAP_DACK) {
		val |= channel->sordackbit;
		gpdr |= channel->gpdrdackbit;
	}
	else
		val &= ~channel->sordackbit;
	if(dmacap & FRV_DMA_CAP_DONE) {
		val |= channel->sordonebit;
		gpdr |= channel->gpdrdonebit;
	}
	else
		val &= ~channel->sordonebit;
	__set_SOR(val);
	__set_GPDR(gpdr);
	
	ret = dma;
 out:
	write_unlock(&frv_dma_channels_lock);
	return ret;
} /* end frv_dma_open() */

/*****************************************************************************/
/*
 * close a DMA channel and its associated interrupt
 */
void frv_dma_close(int dma)
{
	struct frv_dma_channel *channel = &frv_dma_channels[dma];
	unsigned long flags;
	uint32_t val;

	write_lock_irqsave(&frv_dma_channels_lock, flags);

	free_irq(channel->irq, channel);
	frv_dma_stop(dma);
	
	channel->flags = FRV_DMA_FLAGS_AVAILABLE;

	/* reset SIR, SOR, and GPDR so they don't cause future conflicts */
	val = __get_SIR();
	val &= ~channel->sirdreqbit;
	__set_SIR(val);
	
	val = __get_SOR();
	val &= ~(channel->sordackbit | channel->sordonebit);
	__set_SOR(val);

	val = __get_GPDR();
	val &= ~(channel->gpdrdreqbit | channel->gpdrdackbit |
		 channel->gpdrdonebit);
	__set_GPDR(val);
	
	write_unlock_irqrestore(&frv_dma_channels_lock, flags);
} /* end frv_dma_close() */

/*****************************************************************************/
/*
 * set static configuration on a DMA channel
 */
void frv_dma_config(int dma, unsigned long ccfr, unsigned long cctr, unsigned long apr)
{
	unsigned long ioaddr = frv_dma_channels[dma].ioaddr;

	___set_DMAC(ioaddr, CCFR, ccfr);
	___set_DMAC(ioaddr, CCTR, cctr);
	___set_DMAC(ioaddr, APR,  apr);
	mb();

} /* end frv_dma_config() */

/*****************************************************************************/
/*
 * start a DMA channel
 */
void frv_dma_start(int dma,
		   unsigned long sba, unsigned long dba,
		   unsigned long pix, unsigned long six, unsigned long bcl)
{
	unsigned long ioaddr = frv_dma_channels[dma].ioaddr;

	___set_DMAC(ioaddr, SBA,  sba);
	___set_DMAC(ioaddr, DBA,  dba);
	___set_DMAC(ioaddr, PIX,  pix);
	___set_DMAC(ioaddr, SIX,  six);
	___set_DMAC(ioaddr, BCL,  bcl);
	___set_DMAC(ioaddr, CSTR, 0);
	mb();

	__set_DMAC(ioaddr, CCTR, __get_DMAC(ioaddr, CCTR) | DMAC_CCTRx_ACT);
	frv_set_dma_inprogress(dma);

} /* end frv_dma_start() */

/*****************************************************************************/
/*
 * restart a DMA channel that's been stopped in circular addressing mode by comparison-end
 */
void frv_dma_restart_circular(int dma, unsigned long six)
{
	unsigned long ioaddr = frv_dma_channels[dma].ioaddr;

	___set_DMAC(ioaddr, SIX,  six);
	___set_DMAC(ioaddr, CSTR, __get_DMAC(ioaddr, CSTR) & ~DMAC_CSTRx_CE);
	mb();

	__set_DMAC(ioaddr, CCTR, __get_DMAC(ioaddr, CCTR) | DMAC_CCTRx_ACT);
	frv_set_dma_inprogress(dma);

} /* end frv_dma_restart_circular() */

/*****************************************************************************/
/*
 * stop a DMA channel
 */
void frv_dma_stop(int dma)
{
	unsigned long ioaddr = frv_dma_channels[dma].ioaddr;
	uint32_t cctr;

	___set_DMAC(ioaddr, CSTR, 0);
	cctr = __get_DMAC(ioaddr, CCTR);
	cctr &= ~(DMAC_CCTRx_IE | DMAC_CCTRx_ACT);
	cctr |= DMAC_CCTRx_FC; 	/* fifo clear */
	__set_DMAC(ioaddr, CCTR, cctr);
	__set_DMAC(ioaddr, BCL,  0);
	frv_clear_dma_inprogress(dma);
} /* end frv_dma_stop() */

/*****************************************************************************/
/*
 * test interrupt status of DMA channel
 */
int is_frv_dma_interrupting(int dma)
{
	unsigned long ioaddr = frv_dma_channels[dma].ioaddr;

	return __get_DMAC(ioaddr, CSTR) & (1 << 23);

} /* end is_frv_dma_interrupting() */

/*****************************************************************************/
/*
 * dump data about a DMA channel
 */
void frv_dma_dump(int dma)
{
	unsigned long ioaddr = frv_dma_channels[dma].ioaddr;
	unsigned long cstr, pix, six, bcl;

	cstr = __get_DMAC(ioaddr, CSTR);
	pix  = __get_DMAC(ioaddr, PIX);
	six  = __get_DMAC(ioaddr, SIX);
	bcl  = __get_DMAC(ioaddr, BCL);

	printk("DMA[%d] cstr=%lx pix=%lx six=%lx bcl=%lx\n", dma, cstr, pix, six, bcl);

} /* end frv_dma_dump() */

/*****************************************************************************/
/*
 * pause all DMA controllers
 * - called by clock mangling routines
 * - caller must be holding interrupts disabled
 */
void frv_dma_pause_all(void)
{
	struct frv_dma_channel *channel;
	unsigned long ioaddr;;
	unsigned long cstr, cctr;
	int dma;

	write_lock(&frv_dma_channels_lock);

	for (dma = FRV_DMA_NCHANS - 1; dma >= 0; dma--) {
		channel = &frv_dma_channels[dma];

		if (!(channel->flags & FRV_DMA_FLAGS_INUSE))
			continue;

		ioaddr = channel->ioaddr;
		cctr = __get_DMAC(ioaddr, CCTR);
		if (cctr & DMAC_CCTRx_ACT) {
			cctr &= ~DMAC_CCTRx_ACT;
			__set_DMAC(ioaddr, CCTR, cctr);

			do {
				cstr = __get_DMAC(ioaddr, CSTR);
			} while (cstr & DMAC_CSTRx_BUSY);

			if (cstr & DMAC_CSTRx_FED)
				channel->flags |= FRV_DMA_FLAGS_PAUSED;
			frv_clear_dma_inprogress(dma);
		}
	}

} /* end frv_dma_pause_all() */

/*****************************************************************************/
/*
 * resume paused DMA controllers
 * - called by clock mangling routines
 * - caller must be holding interrupts disabled
 */
void frv_dma_resume_all(void)
{
	struct frv_dma_channel *channel;
	unsigned long ioaddr;
	unsigned long cstr, cctr;
	int dma;

	for (dma = FRV_DMA_NCHANS - 1; dma >= 0; dma--) {
		channel = &frv_dma_channels[dma];

		if (!(channel->flags & FRV_DMA_FLAGS_PAUSED))
			continue;

		ioaddr = channel->ioaddr;
		cstr = __get_DMAC(ioaddr, CSTR);
		cstr &= ~(DMAC_CSTRx_FED | DMAC_CSTRx_INT);
		__set_DMAC(ioaddr, CSTR, cstr);

		cctr = __get_DMAC(ioaddr, CCTR);
		cctr |= DMAC_CCTRx_ACT;
		__set_DMAC(ioaddr, CCTR, cctr);

		channel->flags &= ~FRV_DMA_FLAGS_PAUSED;
		frv_set_dma_inprogress(dma);
	}

	write_unlock(&frv_dma_channels_lock);
} /* end frv_dma_resume_all() */

/*****************************************************************************/
/*
 * dma status clear
 */
void frv_dma_status_clear(int dma)
{
	unsigned long ioaddr = frv_dma_channels[dma].ioaddr;
	uint32_t cctr;
	___set_DMAC(ioaddr, CSTR, 0);

	cctr = __get_DMAC(ioaddr, CCTR);
} /* end frv_dma_status_clear() */

EXPORT_SYMBOL(frv_dma_stop);
EXPORT_SYMBOL(frv_dma_start);
EXPORT_SYMBOL(frv_dma_close);
EXPORT_SYMBOL(frv_dma_config);
EXPORT_SYMBOL(frv_dma_dump);
EXPORT_SYMBOL(frv_dma_status_clear);
EXPORT_SYMBOL(frv_dma_open);
EXPORT_SYMBOL(frv_dma_restart_circular);
EXPORT_SYMBOL(is_frv_dma_interrupting);
EXPORT_SYMBOL(frv_dma_pause_all);
EXPORT_SYMBOL(frv_dma_resume_all);
