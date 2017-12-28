/* mb93493-vdc.c: Fujitsu MB93493 companion chip visual display driver
 *
 * Copyright (C) 2004 Red Hat, Inc. All Rights Reserved.
 * Modified by David Howells (dhowells@redhat.com)
 *
 * Derived from original:
 *	Copyright (C) 2002  AXE,Inc.
 *	COPYRIGHT FUJITSU LIMITED 2002
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>
#ifdef CONFIG_PM
#include <linux/pm.h>
#endif
#include <linux/circ_buf.h>
#include <linux/fujitsu/mb93493-vdc.h>
#include <asm/semaphore.h>
#include <asm/pgalloc.h>
#include <asm/uaccess.h>
#include <asm/smplock.h>
#include <asm/io.h>
#include <asm/mb93493-regs.h>
#include <asm/dma.h>
#include "mb93493.h"

//#define ktrace(FMT,...) printk(FMT ,## __VA_ARGS__)
#define ktrace(FMT,...)
//#define kdebug(FMT,...) printk(FMT ,## __VA_ARGS__)
#define kdebug(FMT,...)

#ifdef CONFIG_PM
static struct pm_dev *vdc_pm;
#endif

#define CONFIG_FUJITSU_MB93493_VDC_EVENTBUF_LOG2LEN	8 /* length 2^LOGLEN */

#define VDC_PIX_X_NTSC	720
#define VDC_PIX_Y_NTSC	480

static size_t image_buffer_size;
static struct page *image_buffer_pg;	/* page array covering image buffer */
static unsigned long image_buffer_phys;	/* image buffer physical address */

#ifdef CONFIG_FUJITSU_MB93493_VDC_DEFCONFIG
static char __initdata *vdc_defconfig = CONFIG_FUJITSU_MB93493_VDC_DEFCONFIG;
#else
static char __initdata *vdc_defconfig = "vga*1";
#endif

static struct fr400vdc_config default_config = {
	.skipbf			= 0,
	.stop_immediate		= 1,
	.rd_count_buf_idx	= 0,

	/* VDC param ... */

	.prm			= {
		/* ID=04:480:I BT656 */
		858,  720, 11,  67,  60,  262,  240,  2, 19, 1,  0,  0,  1
	},
	.rddl			= 2,
	.hls			= 0,
	.dsm			= 1, /* interlace */
	.dfp			= 1,
	.die			= 1,
	.dsr			= 0, /* no reset */
	.csron			= 0, /* no cursor */
	.dpf			= 3, /* YCbCr 4:2:2  8 bit */
	.dms			= 3, /* transfer state */
	.dma_mode		= DMAC_CCFRx_CM_2D,
	.dma_ats		= 2,
	.dma_rs			= DMAC_CCFRx_RS_EXTERN,
};

static struct fr400vdc_config vdc_config;

/*
 * a ring-queue of image start offsets in order of delivery to VDC
 * - the application pushes items into the queue at the head
 * - the driver pops items from the tail of the queue and displays them
 * - the driver redisplays the last frame in the queue repeatedly until more data is appeared or
 *   it is told to stop
 */
#define VDC_IMAGEQ_LOG2_MAX	5
#define VDC_IMAGEQ_N_MAX	(1 << VDC_IMAGEQ_LOG2_MAX)

static int image_queue[VDC_IMAGEQ_N_MAX];
static volatile int imageq_tail = 0;
static int imageq_head = 0;
static int vdc_resuming;

#define IMAGE_QSIZE		(vdc_config.buf_num + 1)
#define IMAGEQ_WRAP(idx)	((idx) % IMAGE_QSIZE)
#define IMAGEQ_EMP()		(imageq_tail == imageq_head)
#define IMAGEQ_FULL()		(IMAGEQ_WRAP(imageq_head + 1) == imageq_tail)

#define IMAGEQ_CLR()				\
do {						\
	unsigned long flags;			\
	local_irq_save(flags);			\
	imageq_tail = imageq_head = 0;		\
	local_irq_restore(flags);		\
} while(0)

#define IMAGEQ_PUSH(d)					\
do {							\
	image_queue[imageq_head] = (d);			\
	imageq_head = IMAGEQ_WRAP(imageq_head + 1);	\
} while(0)

#define IMAGEQ_POP()				\
({						\
	int t, x;				\
	t = imageq_tail;			\
	x = image_queue[t];			\
	imageq_tail = IMAGEQ_WRAP(t + 1);	\
	x;					\
})

#define IMAGEQ_TERM_SET()	IMAGEQ_PUSH(-1)
#define IS_IMAGEQ_TERM(d)	((d) == -1)

/*
 * event buffers - used for keeping track of errors and vsync
 */
#define VDC_EVENTBUF_LEN	(1 << CONFIG_FUJITSU_MB93493_VDC_EVENTBUF_LOG2LEN)
#define VDC_EVENTBUF_WRAP(X)	((X) & (VDC_EVENTBUF_LEN - 1))

struct vdc_eventbuf {
	volatile unsigned	head;
	unsigned		tail;
	spinlock_t		taillock;
	wait_queue_head_t	waitq;
	struct fr400vdc_read	ring[VDC_EVENTBUF_LEN];
};

static struct vdc_eventbuf rbuf_vdc = {
	.taillock	= SPIN_LOCK_UNLOCKED,
	.waitq		= __WAIT_QUEUE_HEAD_INITIALIZER(rbuf_vdc.waitq),
};

static struct vdc_eventbuf rbuf_vsync = {
	.taillock	= SPIN_LOCK_UNLOCKED,
	.waitq		= __WAIT_QUEUE_HEAD_INITIALIZER(rbuf_vsync.waitq),
};

/*
 * output state
 */
static unsigned long vdc_flags;
#define VDC_FLAG_VDC_OPEN		0	/* true if VDC device is open */
#define VDC_FLAG_VSYNC_OPEN		1	/* true if vsync device is open */
#define VDC_FLAG_RUNNING		2	/* true if frame dispatcher running */
#define VDC_FLAG_VSYNC_COLLECTING	3	/* true if vsync dev is collecting */
#define VDC_FLAG_SUSPENDED_RUNNING	4	/* true if frame dispatcher was running when we suspended */

static int vdc_in_blank;

static int vdc_dma_channel;
static int vdc_dma_intr_cnt;
static int vdc_intr_cnt;
static int vdc_intr_cnt_vsync;
static int vdc_intr_cnt_under;
static int vdc_frame_count;

static int vdc_current_frame;	/* frame currently being sent */
static int vdc_current_field;	/* interlace field in current frame: 0: top , 1: bottom */

static void vdc_stop(void);
static int __init mb93493_vdc_setup(char *options);
static int vdc_pm_event(struct pm_dev *dev, pm_request_t rqst, void *data);


/*****************************************************************************/
/*
 * clear the event buffer by advancing the tail to meet the head
 */
static void vdc_eventbuf_clear(struct vdc_eventbuf *q)
{
	spin_lock(&q->taillock);
	q->tail = q->head;
	spin_unlock(&q->taillock);
} /* end vdc_eventbuf_clear() */

/*****************************************************************************/
/*
 * add an object to an event buffer
 * - must be called either from interrupt context or with interrupts disabled
 */
static void vdc_eventbuf_push(struct vdc_eventbuf *q, int type, int count)
{
	struct fr400vdc_read *p;

	if (CIRC_SPACE(q->head, q->tail, VDC_EVENTBUF_LEN) > 0) {
		p = &q->ring[q->head];
		p->type = type;
		p->count = count;
		q->head = VDC_EVENTBUF_WRAP(q->head + 1);
		wake_up(&q->waitq);
	}
} /* end vdc_eventbuf_push() */

/*****************************************************************************/
/*
 * get an event from the event buffer
 * - should not be called from interrupt context
 */
static int vdc_eventbuf_get(struct vdc_eventbuf *q, struct fr400vdc_read *buf)
{
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&q->taillock, flags);

	if (CIRC_CNT(q->head, q->tail, VDC_EVENTBUF_LEN) != 0) {
		*buf = q->ring[q->tail];
		q->tail = VDC_EVENTBUF_WRAP(q->tail + 1);
		ret = 1;
	}

	spin_unlock_irqrestore(&q->taillock, flags);
	return ret;
} /* end vdc_eventbuf_get() */

/*****************************************************************************/
/*
 * calculate the consequences of the video buffer configuration
 */
static int vdc_check_vdc_config(struct fr400vdc_config *cfg)
{
	unsigned linesz = cfg->pix_x * cfg->pix_sz;

	/* for DMA-ing purposes, the line width must be a multiple of 32 bytes */
	if (linesz % VDC_TPO_WIDTH != 0)
		return -EINVAL;

	/* at least one image must fit inside the buffer */
	cfg->buf_unit_sz = cfg->pix_x * cfg->pix_y * cfg->pix_sz;

	if (cfg->skipbf)
		cfg->buf_unit_sz /= 2;	/* top field of interlace only */

	if (cfg->buf_unit_sz <= 0 || cfg->buf_unit_sz > image_buffer_size)
		return -ERANGE;

	/* calculate number of frames available up to ceiling */
	cfg->buf_num = image_buffer_size / cfg->buf_unit_sz;
	if (cfg->buf_num > VDC_IMAGEQ_N_MAX)
		cfg->buf_num = VDC_IMAGEQ_N_MAX;

	return 0;
} /* end vdc_check_vdc_config() */

/*****************************************************************************/
/*
 * initiate DMA transfer
 */
static void vdc_dma_set_2d(void)
{
	unsigned long src, six = 0, bcl = 0, apr = 0, ccfr, cctr;
	unsigned trans_bytes = 0;

	src = image_buffer_phys + vdc_config.buf_unit_sz * vdc_current_frame;

	switch (vdc_config.dma_mode) {
	case DMAC_CCFRx_CM_2D:
		six = vdc_config.pix_y;
		if (vdc_config.skipbf)
			six >>= 1;
		bcl = vdc_config.pix_x * vdc_config.pix_sz;
		apr = bcl;
		trans_bytes = apr * six;
		break;

	case DMAC_CCFRx_CM_SCA:
		six = 0xffffffff;
		bcl = vdc_config.pix_x * vdc_config.pix_y * vdc_config.pix_sz;
		trans_bytes = bcl;
		break;

	case DMAC_CCFRx_CM_DA:
		bcl = vdc_config.pix_x * vdc_config.pix_y * vdc_config.pix_sz;
		trans_bytes = bcl;
		break;

	default:
		printk(KERN_ERR "VDC: unsupported DMA mode %d\n", vdc_config.dma_mode);
		break;
	}

	cctr = DMAC_CCTRx_IE |
		DMAC_CCTRx_SAU_INC | DMAC_CCTRx_DAU_HOLD |
		DMAC_CCTRx_SSIZ_32 | DMAC_CCTRx_DSIZ_32;

	ccfr = vdc_config.dma_mode |
		(vdc_config.dma_ats << DMAC_CCFRx_ATS_SHIFT) |
		vdc_config.dma_rs;

	frv_dma_config(vdc_dma_channel, ccfr, cctr, apr);
	frv_dma_start(vdc_dma_channel, src, __addr_MB93493_VDC_TPO(0), 0, six, bcl);

} /* end vdc_dma_set_2d() */

/*****************************************************************************/
/*
 * write the video controller parameters to the VDC's registers
 */
static void vdc_set_hwparam(struct fr400vdc_config *p)
{
	int *prm = p->prm;

	__set_MB93493_VDC(RHDC,		(prm[0] << 16) | prm[1]);
	__set_MB93493_VDC(RH_MARGINS,	(prm[2] << 24) | (prm[3] << 16) | prm[4]);
	__set_MB93493_VDC(RVDC,		(prm[5] << 16) | prm[6]);
	__set_MB93493_VDC(RV_MARGINS,	(prm[7] << 24) | (prm[8] << 16) | (prm[9] << 8));
	__set_MB93493_VDC(RBLACK,	(prm[10] << 24) | (prm[11] << 16));
	__set_MB93493_VDC(RCLOCK,	(prm[12] << 24) | (p->rddl << 16));

	__set_MB93493_VDC(RC,
			  (p->hls  << 31) | (p->pal   << 22) | (p->cscv << 21) |
			  (p->dbls << 20) | (p->r601  << 19) | (p->tfop << 16) |
			  (p->dsm  << 14) | (p->dfp   << 12) | (p->die  << 11) |
			  (p->enop << 10) | (p->vsop  <<  9) | (p->hsop  << 8) |
			  (p->dsr  <<  7) | (p->csron <<  4) | (p->dpf   << 2) |
			  p->dms);
} /* end vdc_set_hwparam() */

/*****************************************************************************/
/*
 * handle a DMA interrupt
 */
static void vdc_dma_interrupt(int dmachan, unsigned long cstr, void *data, struct pt_regs *regs)
{
	int notify_arg;

	ktrace("--> vdc_dma_intr(%d,%lx,,)\n", dmachan, cstr);

	if (vdc_in_blank)
		return;

	vdc_dma_intr_cnt++;

	/* if the current frame is interlaced and we need to send the bottom field of it, then do
	 * so immediately
	 */
	if (vdc_current_field == 0 && vdc_config.skipbf != 1) {
		if (!(__get_MB93493_VDC(RS) & VDC_RS_DFI))
			vdc_current_field = 1;

		vdc_dma_set_2d();
		return;
	}

	/* see about starting next frame */
	vdc_current_field = 0;
	notify_arg = vdc_current_frame;

	if (!IMAGEQ_EMP()) {
		vdc_current_frame = IMAGEQ_POP();

		if (IS_IMAGEQ_TERM(vdc_current_frame)) {
			vdc_stop();
			IMAGEQ_CLR();
		}
		else {
			/* determine the next field */
			vdc_current_field = 1;
			if (__get_MB93493_VDC(RS) & VDC_RS_DFI)
				vdc_current_field = 0;

			vdc_dma_set_2d();
		}
	}

	if (!vdc_config.rd_count_buf_idx)
		notify_arg = vdc_frame_count;

	vdc_eventbuf_push(&rbuf_vdc, FR400CC_VDC_RTYPE_FRAME_FIN, notify_arg);
	vdc_frame_count++;
} /* end vdc_dma_interrupt() */

/*****************************************************************************/
/*
 * deal with DMA underrun
 */
static void vdc_dma_underran(void)
{
	if (!IMAGEQ_EMP())
		vdc_current_frame = IMAGEQ_POP();

	if (IS_IMAGEQ_TERM(vdc_current_frame)) {
		vdc_stop();
		IMAGEQ_CLR();
	}
	else {
		vdc_dma_set_2d();
	}
} /* end vdc_dma_underran() */

/*****************************************************************************/
/*
 * deal with VDC generated interrupt
 */
static void vdc_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned v;
	int vsync, underflow, field;

	ktrace("--> vdc_interrupt(%d,,)\n", irq);

	vdc_intr_cnt++;

	v = __get_MB93493_VDC(RS);
	kdebug("RS=%08x\n", v);

	vsync = (v >> 17) & 1;
	underflow = (v >> 18) & 1;
	field = (v >> 16) & 1;

	if (vsync) {
		vdc_intr_cnt_vsync++;
		if (test_bit(VDC_FLAG_VSYNC_COLLECTING, &vdc_flags))
			vdc_eventbuf_push(&rbuf_vsync, FR400CC_VDC_RTYPE_VSYNC, field);
	}

	if (underflow) {
		vdc_intr_cnt_under++;
		vdc_eventbuf_push(&rbuf_vdc, FR400CC_VDC_RTYPE_ERR_UNDERFLOW, 0);

		frv_dma_stop(vdc_dma_channel);

		vdc_dma_underran();
		if (vdc_config.dpf == 1) {
			vdc_in_blank = 1;
		}
		else if (field == 0) {
			vdc_in_blank = 1;
		}
	}

	if (vdc_in_blank) {
		if (vdc_config.dpf == 1) {
			vdc_dma_underran();
			vdc_in_blank = 0;
		}
		else if (vsync && field == 0) {
			vdc_dma_underran();
			vdc_in_blank = 0;
		}
	}

	/* clear interrupt status bits */
	v &= VDC_RS_DFI | VDC_RS_DCSR | VDC_RS_DCM;
	__set_MB93493_VDC(RS, v);

	ktrace("<-- vdc_interrupt()\n");
} /* end vdc_interrupt() */

/*****************************************************************************/
/*
 * stop the VDC and its attendant DMA controller
 */
static void vdc_stop(void)
{
	if (test_bit(VDC_FLAG_RUNNING, &vdc_flags)) {
		/* reset the video controller */
		__set_MB93493_VDC(RC, VDC_RC_DSR);
		frv_dma_stop(vdc_dma_channel);
		clear_bit(VDC_FLAG_RUNNING, &vdc_flags);
	}
} /* end vdc_stop() */

/*****************************************************************************/
/*
 * start the DMA controller shovelling data out to the VDC
 */
static int vdc_start(void)
{
	/* don't start it if it's already running */
	if (test_and_set_bit(VDC_FLAG_RUNNING, &vdc_flags))
		return -EBUSY;

	vdc_frame_count = 0;
	vdc_eventbuf_clear(&rbuf_vdc);

	if(! vdc_resuming) {
		if (IMAGEQ_EMP()) {
			printk("vdc: no data\n");
			goto error;
		}

		vdc_current_frame = IMAGEQ_POP();

		if (IS_IMAGEQ_TERM(vdc_current_frame)) {
			printk("vdc: no data (term?)\n");
			goto error;
		}
	}

	vdc_current_field = 0;
	vdc_dma_set_2d();
	vdc_set_hwparam(&vdc_config);
	return 0;

 error:
	clear_bit(VDC_FLAG_RUNNING, &vdc_flags);
	return -ENOENT;
} /* end vdc_start() */

/*****************************************************************************/
/*
 * flush a frame to RAM and specify it in the display queue
 */
static int vdc_set_data(int buf_idx)
{
	unsigned long start, end;

	if (IMAGEQ_FULL())
		return -ENOSPC;

	/* check image number if not termination marker */
	if (buf_idx != -1) {
		if (buf_idx < 0 || buf_idx >= vdc_config.buf_num)
			return -ERANGE;

		/* flush the new image from the cache
		 * - on MMU linux, this is tricky: we may not have any virtual addresses to flush
		 */
#ifndef CONFIG_UCLINUX
		start = (vdc_config.buf_unit_sz * buf_idx) >> PAGE_SHIFT;
		end = ((vdc_config.buf_unit_sz * (buf_idx + 1)) + PAGE_SIZE - 1) >> PAGE_SHIFT;

		for (; start < end; start++)
			flush_dcache_page(&image_buffer_pg[start]);

#else
		start = image_buffer_phys + vdc_config.buf_unit_sz * buf_idx;
		end = start + vdc_config.buf_unit_sz;
		
		frv_dcache_writeback(start, end);
#endif
	}

	IMAGEQ_PUSH(buf_idx);
	return 0;
} /* end vdc_set_data() */

static int regio_offset_chk(unsigned offset)
{
	return ((0x000 <= offset && offset < 0x100) ||
		(0x140 <= offset && offset < 0x180) ||
		(0x1c0 <= offset && offset < 0x1e0)
		) ? 0 : -1;
}

static int vdc_regio_get(struct fr400cc_vdc_regio *regio_u)
{
	struct fr400cc_vdc_regio regio;

	if (copy_from_user(&regio, regio_u, sizeof(regio)))
		return -EFAULT;

	if ((regio.reg_offset & 3) != 0)
		return -EINVAL;

	if (regio_offset_chk(regio.reg_offset) != 0)
		return -EINVAL;

	regio.value = __get_MB93493(regio.reg_offset);

	if (copy_to_user(regio_u, &regio, sizeof(regio)))
		return -EFAULT;

	return 0;
}

static int vdc_regio_set(struct fr400cc_vdc_regio *regio_u)
{
	struct fr400cc_vdc_regio regio;

	if (copy_from_user(&regio, regio_u, sizeof(regio)))
		return -EFAULT;

	if ((regio.reg_offset & 3) != 0)
		return -EINVAL;

	if (regio_offset_chk(regio.reg_offset) != 0)
		return -EINVAL;

	__set_MB93493(regio.reg_offset, regio.value);

	return 0;
}

static int vdc_test(unsigned long arg)
{
	int v;

	printk("VDC: test rs=%08lx\n", __get_MB93493_VDC(RS));

	frv_dma_dump(vdc_dma_channel);

	v = vdc_frame_count;
	if (copy_to_user((int *) arg, &v, sizeof(v)))
		return -EFAULT;

	return 0;
}

/*****************************************************************************/
/*
 * VDC device I/O control
 */
static int vdc_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	struct fr400vdc_config uconfig;
	int retval = 0;

	switch (cmd) {
	case VDCIOCGCFG:
		uconfig = vdc_config;
		if (copy_to_user((struct fr400vdc_config *) arg, &uconfig, sizeof(vdc_config)))
			retval = -EFAULT;
		break;

	case VDCIOCSCFG:
		retval = -EBUSY;
		if (test_bit(VDC_FLAG_RUNNING, &vdc_flags))
			break;

		retval = -EFAULT;
		if (copy_from_user(&uconfig, (struct fr400vdc_config *) arg, sizeof(vdc_config)))
			break;

		vdc_config.skipbf &= ~4; /* discard reset-on-underrun flag */

		retval = vdc_check_vdc_config(&uconfig);
		if (retval < 0)
			break;

		vdc_config = uconfig;
		break;

	case VDCIOCSTART:
		retval = vdc_start();
		break;

	case VDCIOCSTOP:
		if (vdc_config.stop_immediate)
			vdc_stop();
		else
			IMAGEQ_TERM_SET();
		break;

	case VDCIOCSDAT:
		retval = vdc_set_data((int) arg);
		break;

	case VDCIOCVSSTART:
		if (test_bit(VDC_FLAG_VSYNC_OPEN, &vdc_flags))
			set_bit(VDC_FLAG_VSYNC_COLLECTING, &vdc_flags);
		vdc_eventbuf_clear(&rbuf_vsync);
		break;

	case VDCIOCVSSTOP:
		clear_bit(VDC_FLAG_VSYNC_COLLECTING, &vdc_flags);
		break;

	case VDCIOCGREGIO:
		retval = vdc_regio_get((struct mb93493_vdc_regio *) arg);
		break;

	case VDCIOCSREGIO:
		retval = vdc_regio_set((struct mb93493_vdc_regio *) arg);
		break;

	case VDCIOCTEST:
		retval = vdc_test(arg);
		break;

#ifdef MB93493_VDC_DEBUG_IOCTL
	case __VDCIOCGREGIO: {
		struct __mb93493_vdc_regio regio;

		if (copy_from_user(&regio, (struct __mb93493_vdc_regio *) arg,
				   sizeof(struct __mb93493_vdc_regio))) {
			retval = -EFAULT;
			break;
		}

		regio.value = read_mb93493_register(regio.reg_no);
		if (copy_to_user((struct __mb93493_vdc_regio *)arg,
				 &regio,
				 sizeof(struct __mb93493_vdc_regio)))
			retval = -EFAULT;
	    }
	    break;

	case __VDCIOCSREGIO:
	    {
		struct __mb93493_vdc_regio regio;

		if (copy_from_user(&regio, (struct __mb93493_vdc_regio *)arg,
				   sizeof(struct __mb93493_vdc_regio))) {
			retval = -EFAULT;
			break;
		}
		write_mb93493_register(regio.reg_no, regio.value);
	    }
	    break;
#endif

	default:
		retval = -ENOIOCTLCMD;
		break;
	}

	return retval;
} /* end vdc_ioctl() */

/*****************************************************************************/
/*
 * open the VDC primary access device
 */
static int vdc_open(struct inode *inode, struct file *file)
{
	int retval;

	/* prohibit multiple simultaneous opening */
	if (test_and_set_bit(VDC_FLAG_VDC_OPEN, &vdc_flags))
		return -EBUSY;

	file->private_data = &rbuf_vdc;

	/* acquire a DMA channel and associated IRQ */
	retval = mb93493_dma_open("mb93493 vdc dma",
				  DMA_MB93493_VDC,
				  FRV_DMA_CAP_DREQ,
				  vdc_dma_interrupt,
				  SA_SHIRQ,
				  NULL);

	if (retval < 0)
		goto error;

	vdc_dma_channel = retval;

	/* acquire the VDC's own IRQ */
	retval = request_irq(IRQ_MB93493_VDC,
			     vdc_interrupt,
			     SA_INTERRUPT | SA_SHIRQ,
			     "mb93493_vdc",
			     &rbuf_vdc);

	if (retval < 0)
		goto error2;

#ifdef CONFIG_PM
	kdebug("%s: pm_register\n", __FUNCTION__);

	vdc_pm = pm_register(PM_SYS_DEV, PM_SYS_UNKNOWN, vdc_pm_event);
	if (vdc_pm == NULL) {
		printk(KERN_ERR "%s: could not register"
		       " with power management", __FUNCTION__);
	}
#endif
	/* we've got the resources, now we can set up the rest */
	vdc_config = default_config;

	IMAGEQ_CLR();

	vdc_dma_intr_cnt = 0;
	vdc_intr_cnt = 0;
	vdc_intr_cnt_vsync = 0;
	vdc_intr_cnt_under = 0;

	return 0;

 error2:
	mb93493_dma_close(vdc_dma_channel);
 error:
	clear_bit(VDC_FLAG_VDC_OPEN, &vdc_flags);
	return retval;
} /* end vdc_open() */

/*****************************************************************************/
/*
 * read event records from the VDC or the VSYNC devices
 */
static ssize_t vdc_read(struct file *file, char *buffer, size_t count, loff_t *ppos)
{
	struct fr400vdc_read read_pack;
	struct vdc_eventbuf *q = file->private_data;
	ssize_t amount = 0;

	DECLARE_WAITQUEUE(myself, current);

	if (count < sizeof(struct fr400vdc_read))
		return -EINVAL;

	while (count >= sizeof(struct fr400vdc_read)) {
		spin_lock(&q->taillock);

		if (amount == 0 && (file->f_flags & O_NONBLOCK) == 0) {
			/* wait for something to arrive in the queue */
			add_wait_queue(&q->waitq, &myself);
			for (;;) {
				set_current_state(TASK_INTERRUPTIBLE);
				if (CIRC_CNT(q->head, q->tail, VDC_EVENTBUF_LEN) != 0)
					break;
				if (signal_pending(current))
					goto interrupted;

				spin_unlock(&q->taillock);
				schedule();
				spin_lock(&q->taillock);
			}
			remove_wait_queue(&q->waitq, &myself);
		}

		/* get the next thing off of the queue and pass it to the user */
		if (!vdc_eventbuf_get(q, &read_pack))
			break;
		spin_unlock(&q->taillock);

		if (copy_to_user(buffer, &read_pack, sizeof(struct fr400vdc_read)))
			goto fault;

		buffer += sizeof(struct fr400vdc_read);
		count -= sizeof(struct fr400vdc_read);
		amount += sizeof(struct fr400vdc_read);
	}

	spin_unlock(&q->taillock);

	return (amount == 0) ? -EAGAIN : amount;

 interrupted:
	remove_wait_queue(&q->waitq, &myself);
	spin_unlock(&q->taillock);
	return -ERESTARTSYS;

 fault:
	return -EFAULT;
} /* end vdc_read() */

/*****************************************************************************/
/*
 * poll for event records on the VDC or the VSYNC devices
 * - also permit polling for space in the image queue
 */
static unsigned int vdc_poll(struct file *file, poll_table *wait)
{
	struct vdc_eventbuf *q = file->private_data;
	unsigned int pollev = 0;

	poll_wait(file, &q->waitq, wait);

	if (CIRC_CNT(q->head, q->tail, VDC_EVENTBUF_LEN) != 0)
		pollev |= POLLIN | POLLRDNORM;

	if (q == &rbuf_vdc && !IMAGEQ_FULL())
		pollev |= POLLOUT | POLLWRNORM;

	return pollev;
} /* end vdc_poll() */

/*****************************************************************************/
/*
 * deal with the VDC device being released
 */
static int vdc_release(struct inode *inode, struct file *file)
{
	vdc_stop();

	mb93493_dma_close(vdc_dma_channel);
	free_irq(IRQ_MB93493_VDC, &rbuf_vdc);
	clear_bit(VDC_FLAG_VDC_OPEN, &vdc_flags);

#if 0 /* debug */
	printk("intr cnt %d (vsync %d, under %d), dma intr cnt %d\n",
	       vdc_intr_cnt,
	       vdc_intr_cnt_vsync,
	       vdc_intr_cnt_under,
	       vdc_dma_intr_cnt);
#endif
#ifdef CONFIG_PM
	pm_unregister(vdc_pm);
#endif
	return 0;
} /* end vdc_release() */

/*****************************************************************************/
/*
 * permit userspace to gain access to the image buffer
 */
static int vdc_mmap(struct file *file, struct vm_area_struct *vma)
{
#ifndef NO_MM
	if (vma->vm_pgoff != 0 || vma->vm_end - vma->vm_start > image_buffer_size)
		return -EINVAL;

	vma->vm_page_prot = __pgprot(pgprot_val(vma->vm_page_prot) | _PAGE_NOCACHE);

	/* don't try to swap out physical pages */
	/* don't dump addresses that are not real memory to a core file */
	vma->vm_flags |= (VM_RESERVED | VM_IO);

	if (io_remap_page_range(vma->vm_start,
			     image_buffer_phys,
			     vma->vm_end - vma->vm_start,
			     vma->vm_page_prot))
		return -EAGAIN;
	return 0;

#else
	int ret = -EINVAL;

	if (vma->vm_start == image_buffer_phys &&
	    vma->vm_end - vma->vm_start <= image_buffer_size &&
	    vma->vm_pgoff == 0
	    ) {
		ret = (int) image_buffer_phys;
	}

	return ret;
#endif
} /* end vdc_mmap() */

/*****************************************************************************/
/*
 * on uClinux, specify exactly the address at which the mapping must be made
 */
#ifdef NO_MM
static unsigned long vdc_get_unmapped_area(struct file *file,
					   unsigned long addr, unsigned long len,
					   unsigned long pgoff, unsigned long flags)
{
  	unsigned long off = pgoff << PAGE_SHIFT;

	if (off < image_buffer_size)
	  	if (len <= image_buffer_size - off)
		  	return image_buffer_phys + off;

	return -EINVAL;
} /* end vdc_get_unmapped_area() */
#endif

/*****************************************************************************/
/*
 * set up the vsync notification device
 */
static int vdc_vsync_open(struct inode *inode, struct file *file)
{
	if (test_and_set_bit(VDC_FLAG_VSYNC_OPEN, &vdc_flags))
		return -EBUSY;

	file->private_data = &rbuf_vsync;

	vdc_eventbuf_clear(&rbuf_vsync);
	set_bit(VDC_FLAG_VSYNC_COLLECTING, &vdc_flags);
	return 0;
} /* end vdc_vsync_open() */

/*****************************************************************************/
/*
 * shut down the vsync notification device
 */
static int vdc_vsync_release(struct inode *inode, struct file *file)
{
	clear_bit(VDC_FLAG_VSYNC_COLLECTING, &vdc_flags);
	clear_bit(VDC_FLAG_VSYNC_OPEN, &vdc_flags);
	return 0;
} /* end vdc_vsync_release() */

static struct file_operations mb93493_vdc_fops = {
	.owner			= THIS_MODULE,
	.open			= vdc_open,
	.read			= vdc_read,
	.poll			= vdc_poll,
	.release		= vdc_release,
	.mmap			= vdc_mmap,
	.ioctl			= vdc_ioctl,
#ifdef NO_MM
	.get_unmapped_area	= vdc_get_unmapped_area,
#endif
};

static struct miscdevice mb93493_vdc_miscdev = {
	.minor		= FR400CC_VDC_MINOR,
	.name		= "mb93493 vdc",
	.fops		= &mb93493_vdc_fops,
};

static struct file_operations mb93493_vdc_vsync_fops = {
	.owner		= THIS_MODULE,
	.open		= vdc_vsync_open,
	.read		= vdc_read,
	.poll		= vdc_poll,
	.release	= vdc_vsync_release,
};

static struct miscdevice mb93493_vdc_vsync_miscdev = {
	.minor		= FR400CC_VDC_VSYNC_MINOR,
	.name		= "mb93493 vdc vsync",
	.fops		= &mb93493_vdc_vsync_fops
};


#ifdef CONFIG_PM
/*
 * Suspend VDC
 */
static void vdc_suspend(void)
{
	ktrace("%s\n", __FUNCTION__);
	if (test_bit(VDC_FLAG_RUNNING, &vdc_flags))
		set_bit(VDC_FLAG_SUSPENDED_RUNNING, &vdc_flags);

	vdc_stop();
}

/*
 * Resume VDC
 */
static void vdc_resume(void)
{
	ktrace("%s\n", __FUNCTION__);

	if (test_bit(VDC_FLAG_SUSPENDED_RUNNING, &vdc_flags)) {
		vdc_resuming = 1;
		vdc_start();
		vdc_resuming = 0;
		clear_bit(VDC_FLAG_SUSPENDED_RUNNING, &vdc_flags);
	}
}

static int vdc_pm_event(struct pm_dev *dev, pm_request_t rqst, void *data)
{
	switch (rqst) {
		case PM_SUSPEND:
			vdc_suspend();
			break;
		case PM_RESUME:
			vdc_resume();
			break;
	}
	return 0;
}
#endif /* CONFIG_PM */

/*****************************************************************************/
/*
 * initialise the VDC driver
 */
static int __devinit mb93493_vdc_init(void)
{
	unsigned long order;
	struct page *page, *pend;

	/* initialise with the built-in defaults if nothing specified on the cmdline */
	if (vdc_defconfig)
		mb93493_vdc_setup(vdc_defconfig);

	/* check the image size and format */
	if (image_buffer_size == 0) {
		image_buffer_size =
			default_config.pix_x * default_config.pix_y * default_config.pix_sz *
			default_config.buf_num;

		if (default_config.skipbf)
			image_buffer_size /= 2;
	}

	if (vdc_check_vdc_config(&default_config) < 0) {
		printk("MB93493-VDC: unable to set image size\n");
		return -ERANGE;
	}

	/* allocate an image buffer somewhere in memory - possibly even highmem (the kernel doesn't
	 * actually need to access it itself)
	 */
	order = PAGE_SHIFT;
	while (order < 31 && image_buffer_size > (1 << order))
		order++;
	order -= PAGE_SHIFT;

	image_buffer_pg = alloc_pages(GFP_HIGHUSER, order);
	if (!image_buffer_pg) {
		printk("MB93493-VDC: couldn't allocate image buffer (order=%lu)\n", order);
		return -ENOMEM;
	}

	image_buffer_size = PAGE_SIZE << order;
	image_buffer_phys = page_to_phys(image_buffer_pg);

	/* now mark the pages as reserved; otherwise remap_page_range */
	/* doesn't do what we want */
	pend = image_buffer_pg + (1 << order);
	for(page = image_buffer_pg; page < pend; page++)
	  SetPageReserved(page);

	__set_MB93493_LBSER(__get_MB93493_LBSER() | MB93493_LBSER_VDC | MB93493_LBSER_GPIO);

	__set_MB93493_GPIO_SOR(0, __get_MB93493_GPIO_SOR(0) | 0x00ff0000);
	__set_MB93493_GPIO_SOR(1, __get_MB93493_GPIO_SOR(1) | 0x0000ff00);

	printk("Fujitsu MB93493 VDC driver (irq=%d, buf=%uKB @ %lx)\n",
	       IRQ_MB93493_VDC, 1 << (order + PAGE_SHIFT - 10), image_buffer_phys);
	printk("Fujitsu MB93493 VDC VSYNC report driver\n");

	/* reset the VDC */
	__set_MB93493_VDC(RC, VDC_RC_DSR);

	misc_register(&mb93493_vdc_miscdev);
	misc_register(&mb93493_vdc_vsync_miscdev);

	return 0;
} /* end mb93493_vdc_init() */

__initcall(mb93493_vdc_init);

/*****************************************************************************/
/*
 * handle setup parameters for the VDC
 */
static int __init mb93493_vdc_setup(char *options)
{
	unsigned x, y, d = 2, nf = 1, skipbf = 0;
	char *p, *q;

	if (!options || !*options)
		return 0;

	vdc_defconfig = NULL; /* don't touch the default config */

	p = options;

	/* handle memory size or explicit image format:
	 *	vdc=nnnK		<- nnn kilobytes
	 *	vdc=nnnM		<- nnn megabytes
	 *	vdc=nnnxmmm[options]	<- nnn * mmm frames
	 */
	x = simple_strtoul(p, &q, 10);
	if (p != q) {
		p = q;
		switch (*p) {
		case 'K':
			image_buffer_size = x << 10;
			return 0;

		case 'M':
			image_buffer_size = x << 20;
			return 0;

		case 'x':
			p++;
			y = simple_strtoul(p, &q, 10);
			if (p == q)
				goto unknown_option;
			p = q;
			default_config.pix_x	= x;
			default_config.pix_y	= y;
			goto extract_options;

		default:
			goto unknown_option;
		}
	}

	/* handle named image format:
	 *	vdc=pal[options]	<-- frames sized for PAL
	 *	vdc=ntsc[options]	<-- frames sized for NTSC
	 *	vdc=vga[options]	<-- frames sized for 640x480 VGA
	 *	vdc=qvga[options]	<-- frames sized for QVGA
	 */
	if (strncmp(p, "pal", 3) == 0) {
		p += 3;
		default_config.pix_x	= 720;
		default_config.pix_y	= 576;
		goto extract_options;
	}

	if (strncmp(p, "ntsc", 4) == 0) {
		p += 4;
		default_config.pix_x	= VDC_PIX_X_NTSC;
		default_config.pix_y	= VDC_PIX_Y_NTSC;
		goto extract_options;
	}

	if (strncmp(p, "vga", 3) == 0) {
		p += 3;
#ifdef CONFIG_MB93093_PDK
		default_config.pix_x	= 640;
		default_config.pix_y	= 480;
		default_config.pix_sz	= 3;
		default_config.rddl	= 1;
		default_config.hls	= 0;
		default_config.pal	= 0;
		default_config.cscv	= 0;
		default_config.dbls	= 0;
		default_config.r601	= 0;
		default_config.tfop	= 0;
		default_config.dsm	= 0;
		default_config.dfp	= 0;
		default_config.die	= 1;
		default_config.enop	= 0;
		default_config.vsop	= 0;
		default_config.hsop	= 0;
		default_config.dsr	= 0;
		default_config.csron	= 0;
		default_config.dpf	= 1;
		default_config.dms	= 3;
		default_config.prm[1]	= 640;
		default_config.prm[2]	= 18;
		default_config.prm[3]	= 48;
		default_config.prm[4]	= 56;
		default_config.prm[6]	= 480;
		default_config.prm[7]	= 8;
		default_config.prm[8]	= 4;
		default_config.prm[9]	= 33;
		default_config.prm[10]	= 0;
		default_config.prm[11]	= 0;
		default_config.prm[12]	= 2;

#else
		default_config.pix_x	= 640;
		default_config.pix_y	= 480;
		default_config.pix_sz	= 2;
		default_config.rddl	= 1;
		default_config.hls	= 0;
		default_config.pal	= 0;
		default_config.cscv	= 2;
		default_config.dbls	= 0;
		default_config.r601	= 0;
		default_config.tfop	= 0;
		default_config.dsm	= 0;
		default_config.dfp	= 0;
		default_config.die	= 1;
		default_config.enop	= 0;
		default_config.vsop	= 0;
		default_config.hsop	= 0;
		default_config.dsr	= 0;
		default_config.csron	= 0;
		default_config.dpf	= 1;
		default_config.dms	= 3;
		default_config.prm[1]	= 640;
		default_config.prm[2]	= 18;
		default_config.prm[3]	= 48;
		default_config.prm[4]	= 56;
		default_config.prm[6]	= 480;
		default_config.prm[7]	= 8;
		default_config.prm[8]	= 4;
		default_config.prm[9]	= 33;
		default_config.prm[10]	= 0;
		default_config.prm[11]	= 0;
		default_config.prm[12]	= 2;
#endif
		goto extract_options;
	}

	if (strncmp(p, "qvga", 4) == 0) {
		p += 4;
		default_config.pix_x	= 640 / 2;
		default_config.pix_y	= 480 / 2;
		goto extract_options;
	}

#ifdef CONFIG_MB93093_PDK
	if (strncmp(p, "lcd", 3) == 0) {
		p += 3;
		default_config.pix_x	= 242;
		default_config.pix_y	= 320;
		default_config.pix_sz	= 3;
		default_config.rddl	= 2;
		default_config.hls	= 0;
		default_config.pal	= 0;
		default_config.cscv	= 2;
		default_config.dbls	= 0;
		default_config.r601	= 0;
		default_config.tfop	= 0;
		default_config.dsm	= 1;
		default_config.dfp	= 1;
		default_config.die	= 1;
		default_config.enop	= 0;
		default_config.vsop	= 0;
		default_config.hsop	= 0;
		default_config.dsr	= 0;
		default_config.csron	= 0;
		default_config.dpf	= 3;
		default_config.dms	= 3;
		default_config.prm[1]	= 320;
		default_config.prm[2]	= 1;
		default_config.prm[3]	= 1;
		default_config.prm[4]	= 1;
		default_config.prm[6]	= 242;
		default_config.prm[7]	= 1;
		default_config.prm[8]	= 1;
		default_config.prm[9]	= 1;
		default_config.prm[10]	= 0;
		default_config.prm[11]	= 0;
		default_config.prm[12]	= 6;
		goto extract_options;
	}
#endif

	goto unknown_option;

	/* handle options:
	 *	...-<nnn>		<-- bits per pixel
	 *	...*<nnn>		<-- number of frames
	 *	...,isb			<-- interlace skip bottom frame
	 */
 extract_options:
	if (*p == '-') {
		p++;
		d = simple_strtoul(p, &q, 10);
		if (p == q || d != 16 || d != 24 || d != 32)
			goto unknown_option;
		p = q;
		d /= 8;
	}

	while (*p) {
		switch (*p++) {
		case '*':
			nf = simple_strtoul(p, &q, 10);
			if (p == q || nf < 1)
				goto unknown_option;
			p = q;
			break;

		case ':':
			if (strncmp(p, "isb", 3) == 0) {
				p += 3;
				skipbf = 1;
			}
		}
	}

	default_config.pix_sz = d;
	default_config.skipbf = skipbf;
	default_config.buf_num = nf;
	return 0;

 unknown_option:
	printk("MB93493-VDC: unparseable option string\n");
	return 0;

} /* end mb93493_vdc_setup() */

__setup("vdc=", mb93493_vdc_setup);
