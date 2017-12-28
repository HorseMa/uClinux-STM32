/*
 *	Fujitsu FR400 Companion Chip I2S (Audio)
 *
 *	Copyright (C) 2002  AXE,Inc.
 *	COPYRIGHT FUJITSU LIMITED 2002
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/fujitsu/mb93493-i2s.h>
#include <linux/poll.h>
#ifdef CONFIG_PM
#include <linux/pm.h>
#endif
#include <asm/semaphore.h>
#include <asm/pgalloc.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <asm/mb93493-regs.h>
#include "mb93493.h"

static int i2s_in_dma_channel, i2s_out_dma_channel;

static int i2s_count = 0;	/* device open count */

static int i2s_in_stat_flg  = I2S_DRV_STS_STOP;
static int i2s_out_stat_flg = I2S_DRV_STS_STOP;

#define I2S_BUF_SZ_DFL		(16 * 1024)
#define I2S_BUF_N_DFL		20
#define I2S_BUF_AREA_SZ		(I2S_BUF_SZ_DFL * I2S_BUF_N_DFL)
#define I2S_TOT_BUF_AREA_SZ	(I2S_BUF_AREA_SZ * 2)
#define I2S_BUF_SZ_MIN		(1024)
#define I2S_BUF_N_MAX		(I2S_BUF_AREA_SZ / I2S_BUF_SZ_MIN)

static struct page *audio_buffers_pg;
static unsigned char *audio_buffers_va;
static unsigned long audio_buffers_phys;
static unsigned long i2s_order;

#define in_audio_buffer_va	((unsigned long) audio_buffers_va + 0)
#define out_audio_buffer_va	((unsigned long) audio_buffers_va + I2S_BUF_AREA_SZ)
#define in_audio_buffer_phys	((unsigned long) audio_buffers_phys + 0)
#define out_audio_buffer_phys	((unsigned long) audio_buffers_phys + I2S_BUF_AREA_SZ)

static struct fr400i2s_read in_frm_info[I2S_BUF_N_MAX];
static struct fr400i2s_read out_frm_info[I2S_BUF_N_MAX];

static DECLARE_MUTEX(i2s_sem);
static DECLARE_MUTEX(i2s_in_sem);
static DECLARE_MUTEX(i2s_out_sem);

static spinlock_t fr400i2s_lock = SPIN_LOCK_UNLOCKED;
static wait_queue_head_t i2s_waitq;

static struct timeval start_time;
static int frm_count, read_count;

/* out_write_offset:  offset into out_audio_buffer_va where to write
 * next data.  Add this value to out_audio_buffer_va to know where to
 * write. */
static int out_write_offset = 0;

/* out_write_dma_start: offset where DMA last started. */
static int out_write_dma_start = 0;

/* out_write_dma_end: offset where DMA will stop. */
static int out_write_dma_end = 0;

/* out_write_index:  buffer index to write next data into.  Doesn't
 * wrap around when reaches end of out_audio_buffer_va (i.e. keeps
 * counting). */
static unsigned long out_write_index = 0;

/* out_read_index:  buffer index where to read output data from.
 * Doesn't wrap around when reaches end of out_audio_buffer_va
 * (i.e. keeps counting). */
static unsigned long out_read_index  = 0;

static struct timeval out_start_time;
static unsigned long out_frm_count, out_read_count;
static int in_busy, out_busy;

static struct fr400i2s_config i2s_config = {
	.channel_n		= 2,
	.bit			= 16,
	.freq			= 8000,	/* UNUSED */
	.exch_lr		= 0,
	.buf_unit_sz		= I2S_BUF_SZ_DFL,
	.buf_num		= I2S_BUF_N_DFL,
	.out_buf_offset		= I2S_BUF_AREA_SZ, /* UNUSED */
	.out_channel_n		= 2,
	.out_bit		= 16,
	.out_freq		= 8000,	/* UNUSED */
	.out_exch_lr		= 0,
	.out_buf_unit_sz	= I2S_BUF_SZ_DFL,
	.out_buf_num		= I2S_BUF_N_DFL,
	.out_swap_bytes		= 0,
};

static unsigned	transferFlg = 2;	/* IM : transfer */
static unsigned	out_dmamode = 0;
static unsigned	out_dmasize = 32;
static unsigned	use_write = 0;

static int i2s_in_start(void);
static int i2s_in_stop(void);
static int i2s_out_stop(struct inode *inode, struct file *file);
static int i2s_debug(int ch);
static void i2s_out_go(void);
static void i2s_out_frm_info_init(void);

/* Values for 'out_f' parameter to buf_unit_sz_get(), buf_num_get(),
 * buf_addr_get(), dma_cache_work(), dma_restart(). */
#define DIR_IN 0
#define DIR_OUT 1

/* Values for 'phys' parameter to buf_addr_get(). */
#define ADDR_VIRT 0
#define ADDR_PHYS 1

static inline unsigned long i2s_out_free_space(void)
{
	unsigned long free_space;
	unsigned long total;

	total = i2s_config.out_buf_unit_sz * i2s_config.out_buf_num;
	if (out_write_offset > out_write_dma_start) {
		free_space = total - (out_write_offset - out_write_dma_start);
	}
	else if (out_write_offset < out_write_dma_start) {
		free_space = out_write_dma_start - out_write_offset;
	}
	else {
		/* if (out_write_offset == out_write_dma_start), we're
		 * either full (out_write_dma_start !=
		 * out_write_dma_end) or empty.  */
		if (out_write_dma_start != out_write_dma_end) {
			free_space = 0;
		}
		else {
			free_space = total;
		}
	}
	return free_space;
}

static int buf_size_adjust(void)
{
	if (i2s_config.buf_unit_sz < I2S_BUF_SZ_MIN)
		return -EINVAL;

	/* buf_unit_sz --> buf_num */
	i2s_config.buf_num = I2S_BUF_AREA_SZ / i2s_config.buf_unit_sz;

	if (i2s_config.out_buf_unit_sz < I2S_BUF_SZ_MIN)
		return -EINVAL;

	i2s_config.out_buf_num = I2S_BUF_AREA_SZ / i2s_config.out_buf_unit_sz;
	return 0;
}


static inline int buf_unit_sz_get(int out_f)
{
	return out_f ? i2s_config.out_buf_unit_sz : i2s_config.buf_unit_sz;
}

static inline int buf_num_get(int out_f)
{
	return out_f ? i2s_config.out_buf_num : i2s_config.buf_num;
}


static inline unsigned long buf_addr_get(int out_f, int frm, int phys)
{
	unsigned long buffer;
	int offset;

	if(phys)
	  buffer = out_f ? out_audio_buffer_phys : in_audio_buffer_phys;
	else
	  buffer = out_f ? out_audio_buffer_va : in_audio_buffer_va;
	offset = buf_unit_sz_get(out_f) * (frm % buf_num_get(out_f));

	return buffer + offset;
}

static void i2s_in_read_data_set(struct fr400i2s_read *pack)
{
	struct fr400i2s_read *inf;
	memset(pack, 0, sizeof(*pack));
	inf = &in_frm_info[read_count % i2s_config.buf_num];
	*pack = *inf;
}

static void i2s_out_read_data_set(struct fr400i2s_read *pack)
{
	struct fr400i2s_read *inf;
	int idx;

	memset(pack, 0, sizeof(*pack));
	idx = out_read_count % i2s_config.out_buf_num;
	inf = &out_frm_info[idx];
	*pack = *inf;
}

static void dma_cache_work(int out_f, int frm)
{
	unsigned long base_addr, frm_sz, frm_addr, frm_offset, max_sz, sz, sz2;

	/* Get how big each frame is. */
	frm_sz = buf_unit_sz_get(out_f);

	/* Get the address of our start frame. */
	frm_addr = buf_addr_get(out_f, frm, ADDR_VIRT);

	if (out_f) {
	    /* Get the base address of the input/outbuf buffer. */
	    base_addr = buf_addr_get(out_f, 0, ADDR_VIRT);

	    /* Get the offset of the frame we're interested in. */
	    frm_offset = frm_addr - base_addr;

	    /* Figure out the maximum offset */
	    max_sz = frm_sz * buf_num_get(out_f);

	    /* Figure out the number of bytes we need to invalidate */
	    sz = frm_sz;

	    if ((frm_offset + sz) > max_sz) {
		sz2 = max_sz - frm_offset;

		/* Invalidate from frm to the end */
		frv_dcache_writeback(frm_addr, frm_addr + sz2);

		/* Invalidate from frm 0 */
		frv_dcache_writeback(base_addr, base_addr + (sz - sz2));

	    }
	    else {
		/* Invalidate from frm */
	  	frv_dcache_writeback(frm_addr, frm_addr + sz);
	    }
	}
	else {
		frv_cache_wback_inv(frm_addr, frm_addr + frm_sz);
	}
}

static void dma_cache_work2(int out_f, int start, int end)
{
	unsigned long base_addr;

	/* Get the base address of the input/outbuf buffer. */
	base_addr = buf_addr_get(out_f, 0, ADDR_VIRT);

	/* If we've wrapped, set the end to the max */
	if (end == 0) {
		end = buf_unit_sz_get(out_f) * buf_num_get(out_f);
	}
#ifdef DEBUG
	printk(KERN_INFO
	       "dma_cache_work2: invalidating from 0x%08x to 0x%08x\n",
	       start, end);
#endif

	/* Invalidate from start to the end */
	if (out_f) {
		frv_dcache_writeback(base_addr + start, base_addr + end);
	}
	else {
		frv_cache_wback_inv(base_addr + start, base_addr + end);
	}
}

static void i2s_in_dma_start(void)
{
	unsigned long ccfr, cctr;

	ccfr = DMAC_CCFRx_CM_DCA | DMAC_CCFRx_RS_EXTERN;

	cctr = DMAC_CCTRx_IE | DMAC_CCTRx_ICE |
		DMAC_CCTRx_SAU_HOLD | DMAC_CCTRx_DAU_INC |
		DMAC_CCTRx_SSIZ_32 | DMAC_CCTRx_DSIZ_32;

	frv_dma_config(i2s_in_dma_channel, ccfr, cctr, 0);

	dma_cache_work(DIR_IN, 0);

	frv_dma_start(i2s_in_dma_channel,
		      __addr_MB93493_I2S_ADR(0), in_audio_buffer_phys,
		      0, buf_addr_get(DIR_IN, 1, ADDR_PHYS),
		      buf_unit_sz_get(DIR_IN) * buf_num_get(DIR_IN));
}

static void i2s_out_dma_start(void)
{
	unsigned long ccfr, cctr, dba, dmaextra;
	int buf_unit_sz;

	ccfr = DMAC_CCFRx_CM_SCA | (2 << DMAC_CCFRx_ATS_SHIFT)
	    | DMAC_CCFRx_RS_EXTERN;

	if (out_dmamode == 0) {
		cctr = DMAC_CCTRx_IE | DMAC_CCTRx_ICE |
			DMAC_CCTRx_SAU_INC |
		  	DMAC_CCTRx_SSIZ_32 | DMAC_CCTRx_DSIZ_32 |
			DMAC_CCTRx_DAU_HOLD;
		out_dmasize = 32;

		dba = __addr_MB93493_I2S_ADR(0);
	}
	else {
		cctr = DMAC_CCTRx_IE | DMAC_CCTRx_ICE |
			DMAC_CCTRx_SAU_INC |
			DMAC_CCTRx_SSIZ_4 | DMAC_CCTRx_SSIZ_4 |
			DMAC_CCTRx_DAU_HOLD;
		out_dmasize = 4;

		dba = __addr_MB93493_I2S_APDR(0);

		__set_MB93493_I2S(AISTR,
				  __get_MB93493_I2S(AISTR) & ~I2S_AISTR_ODS);
	}

	frv_dma_config(i2s_out_dma_channel, ccfr, cctr, 0);

	out_write_dma_start = 0;
	buf_unit_sz = buf_unit_sz_get(DIR_OUT);
	out_write_dma_end = ((out_write_offset >  buf_unit_sz)
			     ? buf_unit_sz : out_write_offset);

	/* Make sure out_write_offset is a multiple of out_dmasize. */
	dmaextra = out_write_dma_end % out_dmasize;
	if (dmaextra) {
#ifdef DEBUG
	    printk(KERN_INFO "i2s_out_dma_start: zeroing out %ld bytes"
		   " from out_write_dma_end (0x%08x)\n",
		   dmaextra, out_write_dma_end);
#endif
	    memset((char *)(out_audio_buffer_va + out_write_dma_end),
		   0, dmaextra);
	}

	dma_cache_work2(DIR_OUT, out_write_dma_start,
			out_write_dma_end + dmaextra);

#ifdef DEBUG
	printk(KERN_INFO
	       "i2s_out_dma_start: out_write_offset = 0x%08x,\n",
	       out_write_offset);
	printk(KERN_INFO "    out_write_dma_start = 0x%08x,"
	       " out_write_dma_end = 0x%08x\n",
	       out_write_dma_start, out_write_dma_end);
	printk(KERN_INFO "    out_write_index = %ld, out_frm_count = %ld\n",
	       out_write_index, out_frm_count);
#endif

	frv_dma_start(i2s_out_dma_channel,
		      out_audio_buffer_phys, dba,
		      0, out_audio_buffer_phys + out_write_dma_end + dmaextra,
		      buf_unit_sz * buf_num_get(DIR_OUT));

}

static void dma_restart(int ch, int out_f)
{
	unsigned long count, six;
	unsigned long dmaextra = 0;

	count = out_f ? out_frm_count : frm_count;

	six = buf_addr_get(out_f, count + 1, ADDR_PHYS);
	if (!out_f) {
		dma_cache_work(out_f, count);
	}
	else {
		unsigned long six_offset, unwrapped_write_offset, total;

		out_write_dma_start = out_write_dma_end;

		total = i2s_config.out_buf_num * i2s_config.out_buf_unit_sz;
		six_offset = six - out_audio_buffer_phys;

		if (out_write_dma_start > out_write_offset) {
			/* we've wrapped */
			unwrapped_write_offset = total + out_write_offset;
		}
		else {
			unwrapped_write_offset = out_write_offset;
		}

		/* If six_offset is 0, it has wrapped.  Unwrap it. */
		if (six_offset == 0) {
		    six_offset = total;
		}

		if (six_offset > unwrapped_write_offset) {
			six = out_audio_buffer_phys + out_write_offset;
		}

		out_write_dma_end = six - out_audio_buffer_phys;

		/* Make sure out_write_offset is a multiple of out_dmasize. */
		dmaextra = out_write_dma_end % out_dmasize;
		if (dmaextra) {
#ifdef DEBUG
			printk(KERN_INFO "i2s_out_dma_start: zeroing out"
			       " %ld bytes from out_write_dma_end (0x%08x)\n",
			       dmaextra, out_write_dma_end);
#endif
			memset((char *)(out_audio_buffer_va
					+ out_write_dma_end),
			       0, dmaextra);
		}

#ifdef DEBUG
		printk(KERN_INFO "dma_restart: out_write_dma_start = 0x%08x,"
		       " out_write_dma_end = 0x%08x\n",
		       out_write_dma_start, out_write_dma_end);
#endif
		dma_cache_work2(out_f, out_write_dma_start, out_write_dma_end
				+ dmaextra);
	}

	frv_dma_restart_circular(ch, six + dmaextra);
}

static void i2s_in_dma_interrupt(int ch, unsigned long cstr, void *dev_id, struct pt_regs *regs)
{
	struct fr400i2s_read *inf;

	inf = &in_frm_info[frm_count % i2s_config.buf_num];
	inf->count = frm_count;
	do_gettimeofday(&inf->captime);

	frm_count++;

	frv_dma_status_clear(i2s_in_dma_channel);
	dma_restart(i2s_in_dma_channel, DIR_IN);
	wake_up(&i2s_waitq);

}

static void i2s_out_dma_interrupt(int ch, unsigned long cstr, void *dev_id, struct pt_regs *regs)
{
	struct fr400i2s_read *inf;
	int idx;

	idx = out_frm_count % i2s_config.out_buf_num;
	if (use_write) {
		if (out_write_offset == out_write_dma_end) {
			frv_dma_status_clear(i2s_out_dma_channel);
			__set_MB93493_I2S(AICR, __get_MB93493_I2S(AICR) & ~I2S_AICR_OM);

			i2s_out_stat_flg = I2S_DRV_STS_PAUSE;
#ifdef DEBUG
			printk(KERN_INFO
			       "i2s_out_dma_interrupt: output paused\n");
#endif
		}
		else {
			if ((out_write_dma_end % i2s_config.out_buf_unit_sz)
			    == 0) {
				inf = &out_frm_info[idx];
				inf->count = out_frm_count;
				do_gettimeofday(&inf->captime);
				out_frm_count++;
				if (out_busy == 1)
				    out_read_index++;
			}
			frv_dma_status_clear(i2s_out_dma_channel);
			dma_restart(i2s_out_dma_channel, DIR_OUT);
#ifdef DEBUG
			printk(KERN_INFO
			       "i2s_out_dma_interrupt: dma restarted1\n");
#endif
		}
	}
	else {
		if ((out_write_dma_end % i2s_config.out_buf_unit_sz) == 0) {
			inf = &out_frm_info[idx];
			inf->count = out_frm_count;
			do_gettimeofday(&inf->captime);
			out_frm_count++;
			if (out_busy == 1)
			    out_read_index++;
		}
		frv_dma_status_clear(i2s_out_dma_channel);
		dma_restart(i2s_out_dma_channel, DIR_OUT);
#ifdef DEBUG
		printk(KERN_INFO
		       "i2s_out_dma_interrupt: dma restarted2\n");
#endif
	}

#ifdef DEBUG
	printk(KERN_INFO
	       "i2s_out_dma_interrupt: out_write_offset = 0x%08x,\n",
	       out_write_offset);
	printk(KERN_INFO "    out_write_dma_start = 0x%08x, out_write_dma_end = 0x%08x\n",
	       out_write_dma_start, out_write_dma_end);
	printk(KERN_INFO "    out_write_index = %ld, out_frm_count = %ld\n",
	       out_write_index, out_frm_count);
#endif
	wake_up(&i2s_waitq);
}

static void i2s_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned long aistr, pat;

	aistr = __get_MB93493_I2S(AISTR);

	pat = I2S_AISTR_IOR;
	if (aistr & pat) {
		printk("i2s: FIFO overrun !\n");
		aistr &= ~pat;
	}

	pat |= I2S_AISTR_IUR;
	if (aistr & pat) {
		printk("i2s: FIFO underrun !\n");
		aistr &= ~pat;
	}

	pat |= I2S_AISTR_ITR;
	if (aistr & pat) {
		aistr &= ~pat;
	}

	pat = I2S_AISTR_OOR;
	if (aistr & pat) {
		printk("i2s: FIFO overrun (out) !\n");
		aistr &= ~pat;
	}

	pat = I2S_AISTR_OUR;
	if (aistr & pat) {
		/* This happens so often, we'll have to ignore it. */
#if 0
		printk("i2s: FIFO underrun (out) !\n");
#endif
		aistr &= ~pat;
	}

	pat = I2S_AISTR_OTR;
	if (aistr & pat) {
		aistr &= ~pat;
	}

	__set_MB93493_I2S(AISTR, aistr);
}

/*****************************************************************************/
/*
 * handle a file being opened
 */
int mb93493_i2s_open(struct inode *inode, struct file *file)
{
	int retval;

	i2s_out_stat_flg = I2S_DRV_STS_STOP;

	down(&i2s_sem);
	if (i2s_count) {
		up(&i2s_sem);
		return -EBUSY;
	}

	retval = mb93493_dma_open("mb93493 i2s in dma",
				  DMA_MB93493_I2S_IN,
				  FRV_DMA_CAP_DREQ,
				  i2s_in_dma_interrupt,
				  SA_INTERRUPT | SA_SHIRQ,
				  NULL);

	if (retval < 0) {
		up(&i2s_sem);
		return retval;
	}

	i2s_in_dma_channel = retval;

	retval = mb93493_dma_open("mb93493 i2s out dma",
				  DMA_MB93493_I2S_OUT,
				  FRV_DMA_CAP_DREQ,
				  i2s_out_dma_interrupt,
				  SA_INTERRUPT | SA_SHIRQ,
				  NULL);

	if (retval < 0) {
		mb93493_dma_close(i2s_in_dma_channel);
		up(&i2s_sem);
		return retval;
	}

	i2s_out_dma_channel = retval;

	retval = request_irq(IRQ_MB93493_AUDIO_IN,
			     i2s_interrupt,
			     SA_INTERRUPT | SA_SHIRQ,
			     "mb93493 i2s in", &i2s_count);
	if (retval) {
		printk("i2s: open, request_irq NG.\n");
		mb93493_dma_close(i2s_in_dma_channel);
		mb93493_dma_close(i2s_out_dma_channel);
		up(&i2s_sem);
		return retval;
	}

	retval = request_irq(IRQ_MB93493_AUDIO_OUT,
			     i2s_interrupt,
			     SA_INTERRUPT | SA_SHIRQ,
			     "mb93493 i2s out", &i2s_count);
	if (retval) {
		printk("i2s: open, request_irq NG.\n");
		mb93493_dma_close(i2s_in_dma_channel);
		mb93493_dma_close(i2s_out_dma_channel);
		free_irq(IRQ_MB93493_AUDIO_IN, &i2s_count);
		up(&i2s_sem);
		return retval;
	}

	in_busy = out_busy = 0;
	i2s_count++;

	out_write_offset = 0;
	out_write_dma_start = 0;
	out_write_dma_end = 0;
	out_write_index = 0;
	out_read_index = 0;
	out_frm_count = 0;
	out_read_count = 0;

	up(&i2s_sem);

#ifdef DEBUG
#ifdef CONFIG_MB93093_PDK
	printk(KERN_INFO "mb93493_i2s_open: FPGATR = 0x%08lx\n",
	       (unsigned long)__get_FPGATR());
	printk(KERN_INFO "mb93493_i2s_open: LEDS = 0x%02lx\n",
	       __builtin_read8((volatile void *) __addr_LEDS()));
#endif
#endif

	return 0;
} /* end mb93493_i2s_open() */

static void bswap_data(void *buffer, int bytes)
{
	u16 *p = buffer;
	int count = bytes / 2;		/* convert from bytes to shorts */

	while (count--) {
		*p = *p << 8 | *p >> 8; p++;
	}
}

/*****************************************************************************/
/*
 * allow userspace to give us audio data to output
 */
ssize_t mb93493_i2s_write(struct file *file, const char *buffer,
			  size_t count, loff_t *ppos)
{
	struct fr400i2s_read *inf;
	unsigned long flags, area_out;
	int idx, cnt1, cnt2;
	size_t max_sz, sz, sz2;
	unsigned long avail;
	ssize_t retval = 0;
	DECLARE_WAITQUEUE(wait, current);

#ifdef DEBUG
	printk(KERN_INFO "mb93493_i2s_write: 0x%x bytes request\n",
	       count);
#endif

	max_sz = i2s_config.out_buf_unit_sz * i2s_config.out_buf_num;

	down(&i2s_out_sem);
	add_wait_queue(&i2s_waitq, &wait);

	while (count > 0) {
		/* wait for output space */
		do {
			spin_lock_irqsave(&fr400i2s_lock, flags);
			avail = i2s_out_free_space();
			if (avail == 0)
				__set_current_state(TASK_INTERRUPTIBLE);
			spin_unlock_irqrestore(&fr400i2s_lock, flags);
			if (avail == 0) {
				if (file->f_flags & O_NONBLOCK) {
					if (!retval)
						retval = -EAGAIN;
					goto out;
				}
				up(&i2s_out_sem);
				schedule();
				if (signal_pending(current)) {
					if (!retval)
						retval = -ERESTARTSYS;
					goto out2;
				}
				down(&i2s_out_sem);
			}
		} while (avail == 0);

		/* place data into the output buffer */
#ifdef DEBUG
		printk(KERN_INFO "mb93493_i2s_write: 0x%lx bytes avail\n",
		       avail);
#endif
		area_out = out_audio_buffer_va + out_write_offset;
		sz = count > avail ? avail : count;
		if (out_write_offset + sz > max_sz) {
			sz2 = max_sz - out_write_offset;

			if (copy_from_user((void *) area_out, buffer, sz2)) {
				retval = -EFAULT;
				goto out;
			}

			if (i2s_config.out_swap_bytes) {
				bswap_data((void *)area_out, sz2);
			}

			area_out = out_audio_buffer_va;
			if (copy_from_user((void *) area_out, (buffer + sz2),
					   (sz - sz2))) {
				retval = -EFAULT;
				goto out;
			}

			if (i2s_config.out_swap_bytes) {
				bswap_data((void *)area_out, (sz - sz2));
			}
		}
		else {
			if (copy_from_user((void *) area_out, buffer, sz)) {
				retval = -EFAULT;
				goto out;
			}
			if (i2s_config.out_swap_bytes) {
				bswap_data((void *)area_out, sz);
			}
			sz2 = 0;
		}

		/* Take account of the fact we've just put data into
		 * the buffer. */
		spin_lock_irqsave(&fr400i2s_lock, flags);

		cnt1 = out_write_offset / i2s_config.out_buf_unit_sz;
		cnt2 = (out_write_offset + sz) / i2s_config.out_buf_unit_sz;

		if (cnt1 != cnt2) {
			out_write_index += ( cnt2 - cnt1 );
		}
		use_write = 1;

		out_write_offset += sz;
		if (out_write_offset >= max_sz)
			out_write_offset -= max_sz;

#ifdef DEBUG
		printk(KERN_INFO
		       "mb93493_i2s_write: out_write_offset = 0x%08x,\n",
		       out_write_offset);
		printk(KERN_INFO "    out_write_dma_start = 0x%08x, out_write_dma_end = 0x%08x\n",
		       out_write_dma_start, out_write_dma_end);
		printk(KERN_INFO
		       "    out_write_index = %ld, out_frm_count = %ld\n",
		       out_write_index, out_frm_count);
#endif

		if (i2s_out_stat_flg == I2S_DRV_STS_PAUSE) {
			__set_MB93493_I2S(AICR,
					  __get_MB93493_I2S(AICR)|I2S_AICR_OM);
			if ((out_write_dma_end % i2s_config.out_buf_unit_sz)
			    == 0) {

				idx = out_frm_count % i2s_config.out_buf_num;
				inf = &out_frm_info[idx];
				inf->count = out_frm_count;
				do_gettimeofday(&inf->captime);
				out_frm_count++;
				if (out_busy == 1)
				    out_read_index++;
			}

#ifdef DEBUG
			printk(KERN_INFO "calling out_go1\n");
#endif
			frv_dma_status_clear(i2s_out_dma_channel);
			dma_restart(i2s_out_dma_channel, DIR_OUT);
			out_busy = 1;
			i2s_out_stat_flg = I2S_DRV_STS_PLAY;
		}
		else if (i2s_out_stat_flg == I2S_DRV_STS_STOP) {
#ifdef DEBUG
			printk(KERN_INFO
			       "mb93493_i2s_write: starting output\n");
#endif
			frv_dma_stop(i2s_out_dma_channel);
			i2s_out_frm_info_init();

			out_frm_count = out_read_count = 0;
			out_read_index = 0;

			i2s_out_dma_start();
			i2s_out_go();

			i2s_out_stat_flg = I2S_DRV_STS_PLAY;
			out_busy = 1;
		}

		if (count > sz) {
		    count -= sz;
		}
		else {
		    count = 0;
		}
		buffer += sz;
		retval += sz;
		spin_unlock_irqrestore(&fr400i2s_lock, flags);
#ifdef DEBUG
		printk(KERN_INFO "mb93493_i2s_write: 0x%x bytes left\n",
		       count);
#endif
	}

out:
	up(&i2s_out_sem);
out2:
	remove_wait_queue(&i2s_waitq, &wait);
	set_current_state(TASK_RUNNING);
	return retval;
} /* end i2s_write() */

/*****************************************************************************/
/*
 * allow userspace to retrieve information about inputted audio data
 * ('struct fr400i2s_read' data)
 */
static ssize_t mb93493_i2s_read(struct file *file, char *buffer,
				size_t count, loff_t *ppos)
{
	struct fr400i2s_read read_pack;
	int sz, retval;

	sz = sizeof(struct fr400i2s_read);
	if (count < sz)
		return -EINVAL;

	if (!access_ok (VERIFY_WRITE, buffer, count)) {
#ifdef DEBUG
		printk(KERN_ERR "mb93493_i2s_read_data: can't write"
		       " to buffer (EFAULT)\n");
#endif
		return -EFAULT;
	}

	down(&i2s_sem);

	retval = 0;
	while (read_count < frm_count && count >= sz) {
		i2s_in_read_data_set(&read_pack);
		read_count++;
		if (copy_to_user(buffer, &read_pack, sz)) {
			retval = -EFAULT;
			break;
		}
		buffer += sz;
		count -= sz;
		retval += sz;
	}

	while (out_read_count < out_frm_count && count >= sz) {
		i2s_out_read_data_set(&read_pack);
		out_read_count++;
		if (copy_to_user(buffer, &read_pack, sz)) {
			retval = -EFAULT;
			break;
		}
		buffer += sz;
		count -= sz;
		retval += sz;
	}

	up(&i2s_sem);
	return retval;
} /* end mb93493_i2s_read() */

/*****************************************************************************/
/*
 * Allow userspace to retrieve inputted audio data itself.  This entry
 * point is used by the standard linux audio driver instead of
 * mb93493_i2s_read(), since the standard linux audio driver wants
 * actual audio data, not information about the audio data (which is
 * what mb93493_i2s_read() returns).
 */
ssize_t mb93493_i2s_read_data(struct file *file, char *buffer,
			      size_t count, loff_t *ppos)
{
	int sz, retval;
	DECLARE_WAITQUEUE(wait, current);

	sz = buf_unit_sz_get(0);
	if (count < sz) {
#ifdef DEBUG
		printk(KERN_ERR
		       "mb93493_i2s_read_data: buffer too small (EINVAL)\n");
#endif
		return -EINVAL;
	}

	if (!access_ok (VERIFY_WRITE, buffer, count)) {
#ifdef DEBUG
		printk(KERN_ERR "mb93493_i2s_read_data: can't write"
		       " to buffer (EFAULT)\n");
#endif
		return -EFAULT;
	}

	/* If we're not recording, start recording. */
	if (! in_busy) {
#ifdef DEBUG
		printk(KERN_INFO
		       "mb93493_i2s_read_data: starting recording...\n");
#endif
		i2s_in_start();
	}

	add_wait_queue(&i2s_waitq, &wait);
	retval = 0;
	while (read_count >= frm_count) {
#ifdef DEBUG
		printk(KERN_INFO
		       "mb93493_i2s_read_data: waiting for data...\n");
#endif
		if (file->f_flags & O_NONBLOCK) {
			retval = -EAGAIN;
			break;
		}
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
		if (signal_pending(current)) {
			retval = -ERESTARTSYS;
			break;
		}
	}
	remove_wait_queue(&i2s_waitq, &wait);
	set_current_state(TASK_RUNNING);
	if (retval) {
#ifdef DEBUG
		printk(KERN_ERR "mb93493_i2s_read_data: returning error %d\n",
		       retval);
#endif
		return retval;
	}

	down(&i2s_sem);
	while (read_count < frm_count && count >= sz) {
#ifdef DEBUG
		printk(KERN_INFO "mb93493_i2s_read_data: copying - "
		       "read_count = %d, frm_count = %d\n",
		       read_count, frm_count);
#endif
		if (copy_to_user(buffer,
				 (void *)buf_addr_get(DIR_IN, read_count,
						      ADDR_VIRT),
				 sz)) {
#ifdef DEBUG
			printk(KERN_ERR "mb93493_i2s_read_data: copy failed"
			       " (EFAULT)\n");
#endif
			read_count++;
			retval = -EFAULT;
			break;
		}
		read_count++;
		buffer += sz;
		count -= sz;
		retval += sz;
	}

	up(&i2s_sem);
	return retval;
} /* end mb93493_i2s_read_data() */

/*****************************************************************************/
/*
 * allow userspace to poll for the ability to push/pull data
 */
static unsigned int i2s_poll(struct file *file, poll_table * wait)
{
	unsigned long flags;
	int comp;

	spin_lock_irqsave(&fr400i2s_lock, flags);
	comp = (frm_count > read_count);
	comp |= (out_frm_count > out_read_count);
	spin_unlock_irqrestore(&fr400i2s_lock, flags);
	if (comp)
		return POLLIN | POLLRDNORM;

	poll_wait(file, &i2s_waitq, wait);

	spin_lock_irqsave(&fr400i2s_lock, flags);
	comp = (frm_count > read_count);
	comp |= (out_frm_count > out_read_count);
	spin_unlock_irqrestore(&fr400i2s_lock, flags);
	if (comp)
		return POLLIN | POLLRDNORM;

	return 0;
} /* end i2s_poll() */

/*****************************************************************************/
/*
 * wait until all output is drained
 */
static int i2s_out_drain(struct inode *inode, struct file *file)
{
	DECLARE_WAITQUEUE(wait, current);
	int retval = 0;
	int out_stat_flg;
	unsigned long flags;

	/* Make sure all the output has been drained... */
        if (file->f_mode & FMODE_WRITE) {
		down(&i2s_out_sem);
		add_wait_queue(&i2s_waitq, &wait);
		for (;;) {
			spin_lock_irqsave(&fr400i2s_lock, flags);
			out_stat_flg = i2s_out_stat_flg;
			if (out_stat_flg == I2S_DRV_STS_PLAY) {
				__set_current_state(TASK_INTERRUPTIBLE);
			}
                        spin_unlock_irqrestore(&fr400i2s_lock, flags);

                        if (out_stat_flg != I2S_DRV_STS_PLAY) {
				break;
			}

			if (file->f_flags & O_NONBLOCK) {
				retval = -EAGAIN;
				break;
			}

#ifdef DEBUG
			printk(KERN_INFO
			       "i2s_out_drain: calling schedule...\n");
#endif
			up(&i2s_out_sem);
			schedule();
                        if (signal_pending(current)) {
				/* We've got a pending signal, but we
				 * also need to finish release
				 * processing.  So, we'll break out of
				 * the loop but keep going. */
				down(&i2s_out_sem);
                                break;
			}
			down(&i2s_out_sem);
                }
		i2s_out_stop(inode, file);
		up(&i2s_out_sem);
                remove_wait_queue(&i2s_waitq, &wait);
		set_current_state(TASK_RUNNING);
#ifdef DEBUG
		printk(KERN_INFO
		       "i2s_out_drain: finished waiting...\n");
		if (retval) {
			printk(KERN_INFO
			       "i2s_out_drain: returning with error %d\n",
			       retval);
		}
#endif
        }
	return retval;
}

/*****************************************************************************/
/*
 * release the resources used by this device
 */
int mb93493_i2s_release(struct inode *inode, struct file *file)
{
	int retval;

#ifdef DEBUG
	printk(KERN_INFO
	       "mb93493_i2s_release: AICR = 0x%08lx, AISTR = 0x%08lx\n",
	       __get_MB93493_I2S(AICR), __get_MB93493_I2S(AISTR));
#endif
	retval = i2s_out_drain(inode, file);
	if (retval) {
#ifdef DEBUG
		printk(KERN_INFO
		       "mb93493_i2s_release: returning early with %d\n",
		       retval);
#endif
		return retval;
	}

	down(&i2s_sem);
#ifdef DEBUG
	printk(KERN_INFO
	       "mb93493_i2s_release: stopping everything...\n");
#endif
	i2s_in_stop();
	i2s_out_stop(inode, file);

	/* Normally, we'd turn off the "master enable" here since
	 * we're done.  But, if you do that you get this crackling
	 * noise from the speakers.  According to Fujitsu, the master
	 * clock must be supplied at all times.  So, we won't turn off
	 * the "master enable".*/
#if 0
	/* turn off the "master enable" */
	__set_MB93493_I2S(AICR, __get_MB93493_I2S(AICR) & ~I2S_AICR_ME);
#endif

	mb93493_dma_close(i2s_in_dma_channel);
	mb93493_dma_close(i2s_out_dma_channel);
	free_irq(IRQ_MB93493_AUDIO_IN, &i2s_count);
	free_irq(IRQ_MB93493_AUDIO_OUT, &i2s_count);

	if (xchg(&i2s_count, 0) != 1)
		BUG();

#ifdef DEBUG
	printk(KERN_INFO
	       "mb93493_i2s_release: done.\n");
#endif
	up(&i2s_sem);
	return 0;
} /* end mb93493_i2s_release() */

/*****************************************************************************/
/*
 * on uClinux, specify exactly the address at which the mapping must be made
 */
#ifdef NO_MM
static unsigned long i2s_get_unmapped_area(struct file *file,
					   unsigned long addr, unsigned long len,
					   unsigned long pgoff, unsigned long flags)
{
	return (unsigned long)audio_buffers_phys;
} /* end i2s_get_unmapped_area() */
#endif

/*****************************************************************************/
/*
 * allow the client to gain direct access to the audio buffers
 */
static int i2s_mmap(struct file *file, struct vm_area_struct *vma)
{
#ifndef NO_MM

	if (vma->vm_pgoff != 0 || vma->vm_end - vma->vm_start > I2S_TOT_BUF_AREA_SZ)
		return -EINVAL;

	vma->vm_page_prot = __pgprot(pgprot_val(vma->vm_page_prot) | _PAGE_NOCACHE);

	/* don't try to swap out physical pages */
	/* don't dump addresses that are not real memory to a core file */
	vma->vm_flags |= (VM_RESERVED | VM_IO);

	if (io_remap_page_range(vma->vm_start,
			     audio_buffers_phys,
			     vma->vm_end - vma->vm_start,
			     vma->vm_page_prot))
		return -EAGAIN;
	return 0;

#else
	int ret;

	ret = -EINVAL;
	if (vma->vm_start == audio_buffers_phys &&
	    vma->vm_end - vma->vm_start <= I2S_TOT_BUF_AREA_SZ &&
	    vma->vm_pgoff == 0)
		ret = (int) audio_buffers_phys;

	return ret;
#endif /* NO_MM */
} /* end i2s_mmap() */

static int i2s_debug(int ch)
{
	printk("i2s_debug ch=%d\n", ch);

	frv_dma_dump(ch);

	printk("AISTR=%08lx\n", __get_MB93493_I2S(AISTR));
	printk("AICR=%08lx\n", __get_MB93493_I2S(AICR));

	printk("----\n");
	return 0;
}

static int i2s_test(int arg)
{
	unsigned long aicr, aistr, sdmi, ami, lri, div, fl, mono, dst, end;

	if (arg == -2)
		return i2s_debug(i2s_out_dma_channel);

	if (arg == -1)
		return i2s_debug(i2s_in_dma_channel);

	if (arg == 123) {
		printk("AICR:%08lx\n", __get_MB93493_I2S(AICR));
		printk("ASTR:%08lx\n", __get_MB93493_I2S(AISTR));
		return 0;
	}

	/*
	  param
	  19-16 / AMI  / 0:i2s,1:justify
	  15-12/ FL  / 0:0(def), 1
	  11-8/ DIV  / 0:def(4div)..3
	  7-4 / lri  / 0:def,1:L,R rev
	  3-0 / SDMI / 0:32,2:16
	*/

	/*
	  sdmi = 2;
	*/
	sdmi = 0;

	lri = 1;
	div = 0;
	fl = 0;
	ami = 0;
	mono = arg;

	printk("stat reg=%08lx\n", __get_MB93493_I2S(AISTR));

	aicr = I2S_AICR_MI;

	if (fl)    aicr |= I2S_AICR_FL;
	if (lri)   aicr |= I2S_AICR_LRI;
	if (ami)   aicr |= I2S_AICR_AMI;
	if (mono)  aicr |= I2S_AICR_MI;

	aicr |= (div << I2S_AICR_DIV_SHIFT) | sdmi << I2S_AICR_SDMI_SHIFT;

	__set_MB93493_I2S(AICR, aicr);

	__set_MB93493_I2S(AICR, __get_MB93493_I2S(AICR) | transferFlg << I2S_AICR_IM_SHIFT);

	printk("cntl reg=%08lx\n", __get_MB93493_I2S(AICR));
	printk("stat reg=%08lx\n", __get_MB93493_I2S(AISTR));

	dst = in_audio_buffer_va;
	end = dst + sizeof(in_audio_buffer_va);

	while (dst < end) {
		do {
			aistr = __get_MB93493_I2S(AISTR);
			if (aistr != 0)
				printk("stat1 %08lx\n", aistr);

			aistr >>= I2S_AISTR_ITST_SHIFT;
			aistr &= 3;
		} while (aistr != 0 && aistr != 2);

		*(unsigned *) dst = __get_MB93493_I2S(ALDR);
		dst += 4;

		do {
			aistr = __get_MB93493_I2S(AISTR);
			if (aistr != 0x00010000)
				printk("stat2 %08lx\n", aistr);

			aistr >>= I2S_AISTR_ITST_SHIFT;
			aistr &= 3;
		} while (aistr != 0 && aistr != 1);

		*(unsigned *) dst = __get_MB93493_I2S(ARDR);
		dst += 4;
	}

	return 0;
}

static void i2s_in_go(void)
{
	unsigned aistr, aicr;
	int sdmi;

	aistr = __get_MB93493_I2S(AISTR) & I2S_AISTR__OUT_MASK;
	aistr |= I2S_AISTR_IORIE | I2S_AISTR_IURIE | I2S_AISTR_IDE | I2S_AISTR_IDS;
	__set_MB93493_I2S(AISTR, aistr);

	aicr = __get_MB93493_I2S(AICR) & ~(I2S_AICR__IN_MASK | I2S_AICR_DIV);

	if (i2s_config.channel_n == 1)
		aicr |= I2S_AICR_MI;

	if (i2s_config.exch_lr)
		aicr |= I2S_AICR_LRI;

	sdmi = 2; /* default 16 bit */
	switch (i2s_config.bit) {
	case 0:   sdmi = (i2s_config.sdmi & 7); break;
	case 8:   sdmi = 3; break;
	case 16:  sdmi = 2; break; /* 4 ? */
	case 32:  sdmi = 0; break;
	case -16: sdmi = 4; break; /* test */
	default:
		printk("i2s: config bit illegal value ?? --> 16 bit\n");
		break;
	}

	aicr |= sdmi << I2S_AICR_SDMI_SHIFT;
	aicr |= i2s_config.ami << 1;

	/* i2s_config.freq ... */

	aicr |= I2S_AICR_ME; /* master enable */

	if (i2s_config.fs) aicr |= I2S_AICR_FS;
	if (i2s_config.fl) aicr |= I2S_AICR_FL;
	aicr |= (i2s_config.div & 3) << I2S_AICR_DIV_SHIFT;

	aicr |= I2S_AICR_CLI;
	__set_MB93493_I2S(AICR, aicr);

	do_gettimeofday(&start_time);

	aicr = __get_MB93493_I2S(AICR);
	aicr |= transferFlg << I2S_AICR_IM_SHIFT; /* IM : transfer */
	__set_MB93493_I2S(AICR, aicr);
#ifdef DEBUG
	printk(KERN_INFO "i2s_in_go: AICR = 0x%08lx, AISTR = 0x%08lx\n",
	       __get_MB93493_I2S(AICR), __get_MB93493_I2S(AISTR));
#endif
}

static void i2s_out_go(void)
{
	unsigned long aistr, aicr;
	int sdmo;

	aistr = __get_MB93493_I2S(AISTR) & ~I2S_AISTR__OUT_MASK;
	aistr |= I2S_AISTR_OORIE | I2S_AISTR_OURIE | I2S_AISTR_ODE | I2S_AISTR_ODS;
	__set_MB93493_I2S(AISTR, aistr);

	aicr = __get_MB93493_I2S(AICR) & ~(I2S_AICR__OUT_MASK | I2S_AICR_DIV);
	if (i2s_config.out_channel_n == 1) {
		aicr |= I2S_AICR_MO;
	}

	if (i2s_config.out_exch_lr) {
		aicr |= I2S_AICR_LRO;
	}

	sdmo = 2; /* default 16 bit */
	switch (i2s_config.out_bit) {
	case 0: sdmo = (i2s_config.sdmo & 7); break;
	case 8:  sdmo = 3; break;
	case 16: sdmo = 2; break; /* 4 ? */
	case 32: sdmo = 0; break;
	case -16: sdmo = 4; break; /* test */
	default:
		printk("i2s: config bit illegal value ?? --> 16 bit\n");
	}

	aicr |= sdmo << I2S_AICR_SDMO_SHIFT;
	aicr |= i2s_config.amo << I2S_AICR_AMO_SHIFT;

	aicr |= I2S_AICR_ME;
	if (i2s_config.fs) {
		aicr |= I2S_AICR_FS;
	}
	if (i2s_config.fl) {
		aicr |= I2S_AICR_FL;
	}

	aicr |= (i2s_config.div & 3) << I2S_AICR_DIV_SHIFT;
	__set_MB93493_I2S(AICR, aicr);

	do_gettimeofday(&out_start_time);

	aicr = __get_MB93493_I2S(AICR);
	aicr |= I2S_AICR_OM;
	__set_MB93493_I2S(AICR, aicr);
#ifdef DEBUG
	printk(KERN_INFO "i2s_out_go: AICR = 0x%08lx, AISTR = 0x%08lx\n",
	       __get_MB93493_I2S(AICR), __get_MB93493_I2S(AISTR));
#ifdef CONFIG_MB93093_PDK
	printk(KERN_INFO "i2s_out_go: FPGATR = 0x%08lx\n",
	       __get_FPGATR());
#endif
#endif
}

static void i2s_in_frm_info_init(void)
{
	int i;
	for (i = 0; i < i2s_config.buf_num; i++) {
		in_frm_info[i].stat = I2S_READ_STAT_IN;
		in_frm_info[i].count = -1;
	}
}

static int i2s_in_start(void)
{
	unsigned long flags, aicr;

	if (in_busy)
		return -EBUSY;

	spin_lock_irqsave(&fr400i2s_lock, flags);

	frv_dma_stop(i2s_in_dma_channel);
	i2s_in_frm_info_init();

	frm_count = read_count = 0;

	spin_lock_irqsave(&fr400i2s_lock, flags);
	aicr = __get_MB93493_I2S(AICR);
	aicr &= ~I2S_AICR_ME;
	__set_MB93493_I2S(AICR, aicr);

	aicr = __get_MB93493_I2S(AICR);
	aicr |= I2S_AICR_ME;
	__set_MB93493_I2S(AICR, aicr);

	spin_unlock_irqrestore(&fr400i2s_lock, flags);

	i2s_in_dma_start();

	i2s_in_stat_flg = I2S_DRV_STS_PLAY;
	i2s_in_go();
	in_busy = 1;

	spin_unlock_irqrestore(&fr400i2s_lock, flags);
	return 0;
}

static int i2s_in_stop(void)
{
	unsigned long aicr;

	aicr = __get_MB93493_I2S(AICR);
	aicr &= ~I2S_AICR_IM;
	aicr |= I2S_AICR_CLI;
	__set_MB93493_I2S(AICR, aicr);

	transferFlg = 2;	/* IM : transfer */
	i2s_in_stat_flg  = I2S_DRV_STS_STOP;

	__set_MB93493_I2S(AISTR, __get_MB93493_I2S(AISTR) & I2S_AISTR__OUT_MASK);

	frv_dma_stop(i2s_in_dma_channel);
	in_busy = 0;
	return 0;
}

static void i2s_out_frm_info_init(void)
{
	int i;
	for (i = 0; i < i2s_config.out_buf_num; i++) {
		out_frm_info[i].stat = I2S_READ_STAT_OUT;
		out_frm_info[i].count = -1;
	}
}

static int i2s_out_start(struct inode *inode, struct file *file, unsigned long arg)
{
	unsigned long flags;
	int idx;

	if (out_busy) {
#ifdef DEBUG
		printk(KERN_INFO "i2s_out_start: already busy...\n");
#endif
		return 0;
	}

	idx = (int) arg;
	spin_lock_irqsave(&fr400i2s_lock, flags);

	frv_dma_stop(i2s_out_dma_channel);
	i2s_out_frm_info_init();

	out_frm_count = out_read_count = 0;
	out_read_index = 0;

	i2s_out_dma_start();
	i2s_out_go();

	i2s_out_stat_flg = I2S_DRV_STS_PLAY;
	out_busy = 1;
	spin_unlock_irqrestore(&fr400i2s_lock, flags);
	return 0;
}

static int i2s_out_stop(struct inode *inode, struct file *file)
{
	__set_MB93493_I2S(AICR, __get_MB93493_I2S(AICR) & ~I2S_AICR_OM);
	__set_MB93493_I2S(AISTR, __get_MB93493_I2S(AISTR) & I2S_AISTR__IN_MASK);

	frv_dma_stop(i2s_out_dma_channel);
	out_busy = 0;
	out_dmamode = 0;
	out_write_offset = 0;
	out_write_dma_start = 0;
	out_write_dma_end = 0;
	out_write_index = 0;
	out_read_index = 0;
	out_frm_count = 0;
	out_read_count = 0;
        use_write = 0;
	i2s_out_stat_flg = I2S_DRV_STS_STOP;

	return 0;
}

static inline unsigned long copy_to_mem(void *to, const void *from,
					unsigned long count, int user_mem)
{
	if (user_mem) {
		return(copy_to_user(to, from, count));
	}
	else {
		memcpy(to, from, count);
		return 0;
	}
}

static inline unsigned long copy_from_mem(void *to, const void *from,
					  unsigned long count, int user_mem)
{
	if (user_mem) {
		return(copy_from_user(to, from, count));
	}
	else {
		memcpy(to, from, count);
		return 0;
	}
}


int mb93493_i2s_ioctl_mem(struct inode *inode, struct file *file,
		      unsigned int cmd, unsigned long arg, int user_mem)
{
	struct fr400i2s_config uconfig;
	struct fr400i2s_read uread, *inf;
	struct timeval utimeval;
	unsigned long flags;
	unsigned long v;
        int retval, idx, utmp;

	retval = 0;

	switch (cmd) {
	case I2SIOCGCFG:
		spin_lock_irqsave(&fr400i2s_lock, flags);
		uconfig = i2s_config;
		spin_unlock_irqrestore(&fr400i2s_lock, flags);

		if (copy_to_mem((struct fr400i2s_config *)arg,
				&uconfig, sizeof(uconfig), user_mem)) {
			retval = -EFAULT;
		}
		break;

	case I2SIOCSCFG:
		if (copy_from_mem(&uconfig, (struct fr400i2s_config *)arg,
				  sizeof(uconfig), user_mem)) {
			retval = -EFAULT;
			break;
		}

		spin_lock_irqsave(&fr400i2s_lock, flags);
		i2s_config = uconfig;
		retval = buf_size_adjust();
		spin_unlock_irqrestore(&fr400i2s_lock, flags);
		break;

	case I2SIOCGSTATIME:
		if (copy_to_mem((struct timeval *) arg, &start_time,
				 sizeof(start_time), user_mem))
			return -EFAULT;
		break;

	case I2SIOCGCOUNT:
		spin_lock_irqsave(&fr400i2s_lock, flags);
		utmp = frm_count;
		spin_unlock_irqrestore(&fr400i2s_lock, flags);

		if (copy_to_mem((int *) arg, &utmp, sizeof(utmp), user_mem))
			retval = -EFAULT;
		break;

	case I2SIOCSTART:
		i2s_in_stat_flg = I2S_DRV_STS_READY;
		retval = i2s_in_start();
		break;

	case I2SIOCSTOP:
		retval = i2s_in_stop();
		i2s_in_stat_flg = I2S_DRV_STS_STOP;
		break;

	case I2SIOCTEST:
		i2s_test((int) arg);
		break;

	case I2SIOC_IN_TRANSFER:
		if (arg > 2) {
			retval = -EINVAL;
			break;
		}

		transferFlg = arg;	/* IM : transfer */
		break;

	case I2SIOC_IN_ERRINFO:
		spin_lock_irqsave(&fr400i2s_lock, flags);
		v = __get_MB93493_I2S(AISTR);
		utmp = (v & (0x00700000)) >> 20;
		spin_unlock_irqrestore(&fr400i2s_lock, flags);

		if (copy_to_mem((int *) arg, &v, sizeof(v), user_mem))
			retval = -EFAULT;
		break;

	case I2SIOC_IN_GETSTS:
		spin_lock_irqsave(&fr400i2s_lock, flags);
		utmp = i2s_in_stat_flg;
		spin_unlock_irqrestore(&fr400i2s_lock, flags);

		if (copy_to_mem((int *) arg, &utmp, sizeof(i2s_in_stat_flg),
				user_mem))
			retval = -EFAULT;
		break;

	case I2SIOC_IN_BLOCKTIME:
		spin_lock_irqsave(&fr400i2s_lock, flags);

		if (frm_count > 0) {
			v = (frm_count - 1) % i2s_config.buf_num;
			uread.stat  = in_frm_info[v].stat;
			uread.count = in_frm_info[v].count + 1;
			uread.captime.tv_sec  = in_frm_info[v].captime.tv_sec;
			uread.captime.tv_usec = in_frm_info[v].captime.tv_usec;
		}
		else {
			uread.stat  = 0;
			uread.count = 0;
			uread.captime.tv_sec  = start_time.tv_sec;
			uread.captime.tv_usec = start_time.tv_usec;
		}
		spin_unlock_irqrestore(&fr400i2s_lock, flags);

		if (copy_to_mem((int *) arg, &uread, sizeof(uread), user_mem))
			retval = -EFAULT;
		break;

	case I2SIOC_OUT_SDAT:
		return 0;

	case I2SIOC_OUT_START:
		if (i2s_out_stat_flg != I2S_DRV_STS_PAUSE) {
#ifdef DEBUG
			printk(KERN_INFO
			       "mb93493_i2s_ioctl: I2SIOC_OUT_START 1\n");
#endif
			i2s_out_stat_flg = I2S_DRV_STS_READY;
			retval = i2s_out_start(inode, file, arg);
		}
		else {
#ifdef DEBUG
			printk(KERN_INFO
			       "mb93493_i2s_ioctl: I2SIOC_OUT_START 2\n");
#endif
		        spin_lock_irqsave(&fr400i2s_lock, flags);

			v = __get_MB93493_I2S(AICR);
			if ((v & I2S_AICR_OM) != 0) {
	                        spin_unlock_irqrestore(&fr400i2s_lock, flags);
				retval = -EBUSY;
				break;
			}
			__set_MB93493_I2S(AICR, v | I2S_AICR_OM);

			if ((out_write_dma_end % i2s_config.out_buf_unit_sz)
			    == 0) {
				idx = out_frm_count % i2s_config.out_buf_num;
				inf = &out_frm_info[idx];
				inf->count = out_frm_count;
				do_gettimeofday(&inf->captime);
				out_frm_count++;
				if (out_busy == 1)
					out_read_index++;
			}
			frv_dma_status_clear(i2s_out_dma_channel);
                        dma_restart(i2s_out_dma_channel, DIR_OUT);

			i2s_out_stat_flg = I2S_DRV_STS_PLAY;

	                spin_unlock_irqrestore(&fr400i2s_lock, flags);
		}
		break;

	case I2SIOC_OUT_STOP:
		retval = i2s_out_stop(inode, file);
		i2s_out_stat_flg = I2S_DRV_STS_STOP;
		break;

	case I2SIOC_OUT_GSTATIME:
		spin_lock_irqsave(&fr400i2s_lock, flags);
		utimeval = out_start_time;
	        spin_unlock_irqrestore(&fr400i2s_lock, flags);

		if (copy_to_mem((struct timeval *) arg, &utimeval,
				sizeof(utimeval), user_mem))
			retval = -EFAULT;
		break;

	case I2SIOC_OUT_GCOUNT:
		spin_lock_irqsave(&fr400i2s_lock, flags);
		utmp = out_frm_count;
		spin_unlock_irqrestore(&fr400i2s_lock, flags);

		if (copy_to_mem((int *) arg, &utmp, sizeof(out_frm_count),
				user_mem))
			retval = -EFAULT;
		break;

	case I2SIOC_OUT_GCHKBUF:
		spin_lock_irqsave(&fr400i2s_lock, flags);
		v = i2s_out_free_space();
		spin_unlock_irqrestore(&fr400i2s_lock, flags);

		if (copy_to_mem((int *) arg, &v, sizeof(v), user_mem))
			retval = -EFAULT;
		break;

	case I2SIOC_OUT_PAUSE:
		spin_lock_irqsave(&fr400i2s_lock, flags);
		__set_MB93493_I2S(AICR,
				  __get_MB93493_I2S(AICR) & ~I2S_AICR_OM);
		i2s_out_stat_flg = I2S_DRV_STS_PAUSE;
		spin_unlock_irqrestore(&fr400i2s_lock, flags);
		break;


	case I2SIOC_OUT_DMAMODE:
		if (arg == 0 || arg == 1) {
#ifdef DEBUG
			printk(KERN_INFO
			       "mb93493_i2s_ioctl: I2SIOC_OUT_DMAMODE %ld\n",
			       arg);
#endif
			out_dmamode = arg;
		}
		else {
			retval = -EINVAL;
		}
		break;

	case I2SIOC_OUT_GETSTS:
		spin_lock_irqsave(&fr400i2s_lock, flags);
		utmp = i2s_out_stat_flg;
		spin_unlock_irqrestore(&fr400i2s_lock, flags);

		if (copy_to_mem((int *) arg, &utmp, sizeof(i2s_out_stat_flg),
				user_mem))
			retval = -EFAULT;
		break;

	case I2SIOC_OUT_DRAIN:
		retval = i2s_out_drain(inode, file);
		break;

	default:
		retval = -ENOIOCTLCMD;
		break;
	}

	return retval;
}

static int mb93493_i2s_ioctl(struct inode *inode, struct file *file,
		      unsigned int cmd, unsigned long arg)
{
    return mb93493_i2s_ioctl_mem(inode, file, cmd, arg, 1);
}

static struct file_operations fr400i2s_fops = {
	.owner		= THIS_MODULE,
	.open		= mb93493_i2s_open,
	.read		= mb93493_i2s_read,
	.write		= mb93493_i2s_write,
	.poll		= i2s_poll,
	.release	= mb93493_i2s_release,
	.mmap		= i2s_mmap,
	.ioctl		= mb93493_i2s_ioctl,
#ifdef NO_MM
	.get_unmapped_area	= i2s_get_unmapped_area,
#endif
};

static struct miscdevice fr400i2s_miscdev = {
	.minor		= FR400I2S_MINOR,
	.name		= "fr400i2s",
	.fops		= &fr400i2s_fops,
};

#ifdef CONFIG_PM
static struct pm_dev *i2s_pm;
static volatile int old_in_stat_flg  = I2S_DRV_STS_STOP;
static volatile int old_out_stat_flg = I2S_DRV_STS_STOP;

/*
 * Suspend audio input/output
 */
static void i2s_suspend(void)
{
	unsigned long flags;

#ifdef DEBUG
	printk("i2s_suspend\n");
#endif

	// Stop input (if needed)
	spin_lock_irqsave(&fr400i2s_lock, flags);
	old_in_stat_flg = i2s_in_stat_flg;
	spin_unlock_irqrestore(&fr400i2s_lock, flags);
	if (old_in_stat_flg != I2S_DRV_STS_STOP) {
#ifdef DEBUG
		printk("i2s_suspend: stopping input\n");
#endif
		mb93493_i2s_ioctl(NULL, NULL, I2SIOCSTOP, 0);
	}

	// Pause output (if needed)
	spin_lock_irqsave(&fr400i2s_lock, flags);
	old_out_stat_flg = i2s_out_stat_flg;
	spin_unlock_irqrestore(&fr400i2s_lock, flags);
	if (old_out_stat_flg != I2S_DRV_STS_STOP) {
#ifdef DEBUG
		printk("i2s_suspend: stopping output\n");
#endif
		mb93493_i2s_ioctl(NULL, NULL, I2SIOC_OUT_STOP, 0);
	}
#ifdef DEBUG
	else {
		printk("i2s_suspend: output status = %s\n",
		       ((old_out_stat_flg == I2S_DRV_STS_STOP)
			? "I2S_DRV_STS_STOP" : "I2S_DRV_STS_PAUSE"));
	}
#endif
	out_busy = 0;
}

/*
 * Resume audio input/output
 */
static void i2s_resume(void)
{
#ifdef DEBUG
	printk("i2s_resume\n");
#endif

	// Start output (if needed)
	if (old_out_stat_flg != I2S_DRV_STS_STOP) {
#ifdef DEBUG
		printk("i2s_resume: starting output\n");
#endif
		mb93493_i2s_ioctl(NULL, NULL, I2SIOC_OUT_START, 0);
		old_out_stat_flg = I2S_DRV_STS_STOP;
	}

	// Start input (if needed)
	if (old_in_stat_flg != I2S_DRV_STS_STOP) {
#ifdef DEBUG
		printk("i2s_resume: starting input\n");
#endif
		mb93493_i2s_ioctl(NULL, NULL, I2SIOCSTART, 0);
		old_in_stat_flg = I2S_DRV_STS_STOP;
	}
}

static int i2s_pm_event(struct pm_dev *dev, pm_request_t rqst, void *data)
{
	switch (rqst) {
		case PM_SUSPEND:
			i2s_suspend();
			break;
		case PM_RESUME:
			i2s_resume();
			break;
	}
	return 0;
}
#endif /* CONFIG_PM */

static int __devinit mb93493_i2s_init(void)
{
	struct page *page, *pend;

	__set_MB93493_LBSER(__get_MB93493_LBSER() | MB93493_LBSER_AUDIO);

	init_waitqueue_head(&i2s_waitq);
	misc_register(&fr400i2s_miscdev);

#ifdef CONFIG_PM
	i2s_pm = pm_register(PM_SYS_DEV, PM_SYS_UNKNOWN, i2s_pm_event);
	if (i2s_pm == NULL) {
		printk(KERN_ERR "mb93493_i2s_init: could not register"
		       " with power management");
	}
#endif

	/* Allocate buffers */
	i2s_order = PAGE_SHIFT;
	while (i2s_order < 31 && I2S_TOT_BUF_AREA_SZ > (1 << i2s_order))
		i2s_order++;
	i2s_order -= PAGE_SHIFT;

	audio_buffers_pg = alloc_pages(GFP_HIGHUSER, i2s_order);
	if (!audio_buffers_pg) {
		printk("MB93493-i2s: couldn't allocate audio buffer (order=%lu)\n", i2s_order);
		return -ENOMEM;
	}
	audio_buffers_va = page_address(audio_buffers_pg);
	audio_buffers_phys = page_to_phys(audio_buffers_pg);

	/* now mark the pages as reserved; otherwise remap_page_range */
	/* doesn't do what we want */
	pend = audio_buffers_pg + (1 << i2s_order);
	for (page = audio_buffers_pg; page < pend; page++)
		SetPageReserved(page);

	printk("FR400 Companion Chip I2S driver (irq-in=%d irq-out=%d dreq-in=%d dreq-out=%d)\n",
	       IRQ_MB93493_AUDIO_IN, IRQ_MB93493_AUDIO_OUT,
	       i2s_in_dma_channel, i2s_out_dma_channel);

	return 0;
}

static void mb93493_i2s_exit(void)
{
#ifdef CONFIG_PM
	pm_unregister(i2s_pm);
#endif

	free_pages((unsigned long)audio_buffers_pg, i2s_order);
}

__initcall(mb93493_i2s_init);
module_exit(mb93493_i2s_exit);
EXPORT_SYMBOL(mb93493_i2s_open);
EXPORT_SYMBOL(mb93493_i2s_release);
EXPORT_SYMBOL(mb93493_i2s_ioctl_mem);
EXPORT_SYMBOL(mb93493_i2s_write);
EXPORT_SYMBOL(mb93493_i2s_read_data);
