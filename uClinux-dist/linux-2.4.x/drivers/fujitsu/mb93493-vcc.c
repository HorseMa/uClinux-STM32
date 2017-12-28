/*
 *	Fujitsu FR400 Companion Chip VCC
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
#include <linux/poll.h>
#include <linux/circ_buf.h>
#include <linux/fujitsu/mb93493-vcc.h>
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

//#define VCC_RCC_TO	0x01000000	/* kon test */
//#define VCC_RCC_TO_DBG	0		/* kon debug */
//#undef VCC_RCC_TO
//#undef VCC_RCC_TO_DBG

#define CONFIG_MB93091_CB30   1
//#undef CONFIG_FR400CC_VCC_VSYNC_REPORT
#define CONFIG_FR400CC_VCC_VSYNC_REPORT
#define USE_INTERRUPT  1
//#undef NOUSE_VCC_INTERRUPT
//#define USE_DUMMY_BUFFER

#define READ_BUF_LOG2LEN	8
#define READ_BUF_LEN		(1 << READ_BUF_LOG2LEN)
#define READ_BUF_WRAP(X)	((X) & (READ_BUF_LEN - 1))

static int vcc_count;

struct frame_info {
	int status;
	int count;
	int lost_count;
	int over_flow;
	struct timeval captime;
	unsigned long buffer;
};

#define BYTES_PER_PIXEL	2

#define	FRAME_NTSC
#ifdef FRAME_VGA
#define FRAME_WIDTH	640 //CONFIG_FR400CC_VCC_BUFWIDTH
#define FRAME_HEIGHT	480 // CONFIG_FR400CC_VCC_BUFHEIGHT
#define BUFFER_NUM	4 // CONFIG_FR400CC_VCC_BUFLEN
#define FRAME_BYTES	(FRAME_WIDTH*FRAME_HEIGHT*BYTES_PER_PIXEL)
#endif
#ifdef FRAME_NTSC
#define FRAME_WIDTH	720 //CONFIG_FR400CC_VCC_BUFWIDTH
#define FRAME_HEIGHT	480 // CONFIG_FR400CC_VCC_BUFHEIGHT
#define BUFFER_NUM	6 // CONFIG_FR400CC_VCC_BUFLEN
#define FRAME_BYTES	(FRAME_WIDTH*FRAME_HEIGHT*BYTES_PER_PIXEL)
#endif


//static char imagebuffer[FRAME_BYTES * BUFFER_NUM] __cacheline_aligned;
//#ifdef USE_DUMMY_BUFFER
//static char dummy_buf[FRAME_BYTES] __cacheline_aligned;
//#endif

#define DRVNAME   "MB93493-VCC"
static size_t imagebuffer_size;
static struct page *imagebuffer_pg;
static unsigned long imagebuffer_phys;

static size_t dummy_buf_size;
//static struct page *dummy_buf_pg;
static unsigned long dummy_buf_phys;


static DECLARE_MUTEX(vcc_sem);
static spinlock_t fr400cc_vcc_lock = SPIN_LOCK_UNLOCKED;
static spinlock_t fr400cc_defer_dma_lock = SPIN_LOCK_UNLOCKED;
static struct frame_info frame_info[BUFFER_NUM];
static struct fr400cc_vcc_mbuf vcc_mbuf;
static int vcc_dma_channel;
static int capture_next_at_vsync;
static int defer_dma_start;

#ifdef CONFIG_FR400CC_VCC_VSYNC_REPORT
static DECLARE_MUTEX(vcc_vsync_sem);
static spinlock_t fr400cc_vcc_vsync_lock = SPIN_LOCK_UNLOCKED;
#endif

struct vcc_i {
	int status;
	int vsync;
	int vsync_field_last_captured;
	int vsync_frame_last_captured;
	int count;
	int field;	/* top field:0 / bottom field:1 */
	int interlace;
	int frame_head;
	int frame_tail_k;
	int frame_tail_u;
	int frame_num_k;
	int frame_num_u;
	int rest;
	int buffull_lost_count;	/* count lost frames while kernel buffer */
    				/* full */
	int total_lost;		/* Total count of lost frames */
	int total_lost_bf;		/* Total count of lost frames (buffer full) */
	int total_ov;		/* Total count of over run in RIS */
	int total_err;		/* Total count of error in RIS */
  	__u32 rccval;
	wait_queue_head_t wait;
#ifdef CONFIG_FR400CC_VCC_VSYNC_REPORT
	wait_queue_head_t vsync_wait;
#endif
};

#define VCC_RCC_CIE_VALUE 0
#ifdef USE_INTERRUPT
#ifndef NOUSE_VCC_INTERRUPT
#undef  VCC_RCC_CIE_VALUE
#define VCC_RCC_CIE_VALUE VCC_RCC_CIE
#endif
#endif

#define VCC_COMPUTE_RCC(val, cfg) \
	(val) = VCC_RCC_MOV | \
	  	((cfg).rcc_to ? VCC_RCC_TO : 0) | \
		(((cfg).rcc_fdts << VCC_RCC_FDTS_SHIFT) & VCC_RCC_FDTS) | \
		((cfg).rcc_ifi ? VCC_RCC_IFI : 0) | \
		((cfg).rcc_es ? VCC_RCC_ES_POS : VCC_RCC_ES_NEG) | \
	  	(((cfg).rcc_csm << VCC_RCC_CSM_SHIFT) & VCC_RCC_CSM) | \
		((cfg).rcc_cfp ? VCC_RCC_CFP_1TO1 : VCC_RCC_CFP_4TO3) | \
		((cfg).rcc_vsip ? VCC_RCC_VSIP_HIACT : VCC_RCC_VSIP_LOACT) | \
		((cfg).rcc_hsip ? VCC_RCC_HSIP_HIACT : VCC_RCC_HSIP_LOACT) | \
		(((cfg).rcc_cpf <<  VCC_RCC_CPF_SHIFT) & VCC_RCC_CPF) | \
		VCC_RCC_CIE_VALUE;


static struct vcc_i vcc_info = {
	.wait = __WAIT_QUEUE_HEAD_INITIALIZER(vcc_info.wait)
};

static struct fr400cc_vcc_config vcc_config;
static struct timeval vcc_start_tm;
static struct timeval vcc_dma_time;

#if defined(CONFIG_MB93091_CB30) || defined(CONFIG_FR400PDK2_BOARD)
static int reset_v = 0;
#endif

static struct fr400cc_vcc_read vcc_rbuf[READ_BUF_LEN];
static volatile int vcc_rbuf_head, vcc_rbuf_tail;

static int vcc_rbuf_put(int type, int count, struct timeval *tv);
static int vcc_config_check(struct fr400cc_vcc_config *conf);

#ifdef CONFIG_FR400CC_VCC_VSYNC_REPORT
static void vcc_vsync_rbuf_put(int type, int count, struct timeval *tv);
#endif

#ifdef NEVER
static void vcc_reg_dump(void)
{
	printk("%s: RCC=%08x, RIS=%08x\n",
	       DRVNAME,
	       __get_MB93493_VCC(RCC),
	       __get_MB93493_VCC(RIS));
}
#endif

#ifdef VCC_RCC_TO
#define Enable_RCC_TO(cfg)	((cfg).skipbf && (cfg).interlace && (cfg).rhr > 0x100)
#endif

#ifdef VCC_RCC_TO_DBG
static int dma_intr_cnt = 0;
#endif

static int get_mb93401_dmasiz(int rdts)
{
	switch (rdts) {
	default:
		printk("%s: invalid RDTS (%d)\n", DRVNAME, rdts);
	case 1:
		return DMAC_CCTRx_SSIZ_4 | DMAC_CCTRx_DSIZ_4;
	case 3:
		return DMAC_CCTRx_SSIZ_32 | DMAC_CCTRx_DSIZ_32;
	}
}

static int vcc_start_2D(int ch, unsigned long src, unsigned long dst,
			int w, int h, int w2,
			int dmasiz, int sau, int dau, int rs)
{
	unsigned cctr;

	dst &= ~3;

	//	kdebug("mb93401_dma_start_2D(ch=%d, src=%08x, dst=%08x, w=%d, h=%d,"
	//       " w2=%d dmasiz=%d sau=%08x dau=%08x)\n",
	//       ch, (unsigned) src, (unsigned) dst, w, h, w2, dmasiz, sau, dau);

	cctr = DMAC_CCTRx_IE | sau | dau | dmasiz;

	frv_dma_config(ch, DMAC_CCFRx_CM_2D | (rs << DMAC_CCFRx_RS_SHIFT), cctr, w2);

	frv_dcache_writeback(src, src + (w2*h));

	kdebug("%s start dma %x %x %x %x %x %x\n",
	       __FUNCTION__, ch, src, dst, 0, h, w);
	frv_dma_start(ch, src, dst, 0, h, w);
	return 0;
}

static int vcc_reset(void)
{
  	int loopcount = 0;

	kdebug("%s\n", __FUNCTION__);
	__set_MB93493_VCC(RCC, VCC_RCC_CSR);
	while (__get_MB93493_VCC(RCC) & VCC_RCC_CE){
	  if(++loopcount > 100000) {
	    kdebug("%s: reset FAILED\n", __FUNCTION__);
	    return 1;
	  }
		continue;
	}

#if defined(CONFIG_MB93091_CB30) || defined(CONFIG_FR400PDK2_BOARD)
	reset_v = 1;
#endif
	return 0;
}

static void vcc_st_init(int num, int interlace)
{
	unsigned long flags;

	kdebug("%s\n", __FUNCTION__);

	spin_lock_irqsave(&fr400cc_vcc_lock, flags);
	vcc_info.vsync = 0;
	vcc_info.vsync_field_last_captured = 0;
	vcc_info.vsync_frame_last_captured = 0;
	vcc_info.count = 0;
	vcc_info.buffull_lost_count = 0;
	vcc_info.field = 0;
	vcc_info.rest = num;
	vcc_info.interlace = interlace;
	do_gettimeofday(&vcc_start_tm);

#if 0
	vcc_info.frame_head	= 0;
	vcc_info.frame_tail_k	= 0;
	vcc_info.frame_tail_u	= 0;
	vcc_info.frame_num_k	= 0;
	vcc_info.frame_num_u	= 0;

	vcc_info.total_lost	= 0;
	vcc_info.total_lost_bf	= 0;

	for (i = 0; i < BUFFER_NUM; i++)
	{
	  frame_info[i].status = FR400CC_VCC_FI_FREE;
		frame_info[i].count = 0;
		frame_info[i].lost_count = 0;
		memset(&frame_info[i].captime,0,sizeof(struct timeval));
	}
#endif

	spin_unlock_irqrestore(&fr400cc_vcc_lock, flags);
}

#define ST_SUCCESS	0
#define ST_BUFFERFULL	1
#define ST_NOREST	2
static int vcc_st_start(void)
{
	unsigned long flags;
	int status, retval;

	//kdebug("%s\n", __FUNCTION__);

	spin_lock_irqsave(&fr400cc_vcc_lock, flags);

	if (vcc_info.rest == 0) {
		retval = ST_NOREST;
		goto end_func;
	}

	status = frame_info[vcc_info.frame_head].status;
	if (!(status == FR400CC_VCC_FI_FREE ||
	      (vcc_info.interlace && vcc_info.field == 1 && status == FR400CC_VCC_FI_CAPTURING))) {

		if (status == FR400CC_VCC_FI_CAPTURED_K ||
		    status == FR400CC_VCC_FI_CAPTURED_U) {
			vcc_info.buffull_lost_count = 0;
			retval = ST_BUFFERFULL;
			goto end_func;
		}

		retval = -1;
		goto end_func;
	}

	if ((! vcc_info.interlace) || (vcc_info.interlace && vcc_info.field == 0)) {
	  frame_info[vcc_info.frame_head].status = FR400CC_VCC_FI_CAPTURING;
		frame_info[vcc_info.frame_head].count = vcc_info.count;
		if (vcc_info.status == FR400CC_VCC_ST_FRAMEFULL)
			frame_info[vcc_info.frame_head].lost_count = vcc_info.buffull_lost_count;
		else
			frame_info[vcc_info.frame_head].lost_count = 0;
	}

	kdebug("%s:success\n", __FUNCTION__);
	retval = ST_SUCCESS;

 end_func:
	spin_unlock_irqrestore(&fr400cc_vcc_lock, flags);
	return retval;
}

static int vcc_st_end(void)
{
	unsigned long flags;
	int status, retval;

	kdebug("%s\n", __FUNCTION__);

	retval = 0;
	spin_lock_irqsave(&fr400cc_vcc_lock, flags);
	status = frame_info[vcc_info.frame_head].status;
	if (status != FR400CC_VCC_FI_CAPTURING) {
		retval = -1;
		goto end_func;
	}

	if (vcc_info.status == FR400CC_VCC_ST_STOP) {
		/* when vcc is stopped by ioctl */
		vcc_info.field = 0;
		frame_info[vcc_info.frame_head].status = FR400CC_VCC_FI_FREE;
		retval = -1;
		goto end_func;
	}

	if (vcc_info.interlace && vcc_info.field == 0) {
		vcc_info.field = 1;
		vcc_info.vsync_field_last_captured = vcc_info.vsync;
	} else {
		vcc_info.count++;
		vcc_info.field = 0;
		frame_info[vcc_info.frame_head].status = FR400CC_VCC_FI_CAPTURED_K;
		do_gettimeofday(&frame_info[vcc_info.frame_head].captime);
		vcc_info.vsync_field_last_captured = vcc_info.vsync;
		vcc_info.vsync_frame_last_captured = vcc_info.vsync;

		vcc_rbuf_put(FR400CC_VCC_RTYPE_CAPTURE,
			     frame_info[vcc_info.frame_head].count,
			     &frame_info[vcc_info.frame_head].captime);

		vcc_info.frame_head = (vcc_info.frame_head + 1) % BUFFER_NUM;
		vcc_info.frame_num_k++;

		if (vcc_info.rest != -1) {
			if (vcc_info.rest <= 0)
				printk("%s: bad status frame=%d, rest=%d (vcc_st_end)\n",
				       DRVNAME, vcc_info.frame_head, vcc_info.rest);
			else
				vcc_info.rest--;
		}
	}

 end_func:
	spin_unlock_irqrestore(&fr400cc_vcc_lock, flags);
	return retval;
}

static void vcc_start(void)
{
	unsigned long flags;

	kdebug("%s\n", __FUNCTION__);

	spin_lock_irqsave(&fr400cc_vcc_lock, flags);

	//kdebug("vcc_start, RCC=%08x\n", vcc_info.rccval);

	__set_MB93493_VCC(RCC, vcc_info.rccval | VCC_RCC_CS | VCC_RCC_CE);
	vcc_info.status = FR400CC_VCC_ST_CAPTURING;
	spin_unlock_irqrestore(&fr400cc_vcc_lock, flags);
}

static void vcc_vsync_start_1(void)
{
	unsigned long flags;

	kdebug("%s\n", __FUNCTION__);

	spin_lock_irqsave(&fr400cc_vcc_lock, flags);

	__set_MB93493_VCC(RCC, vcc_info.rccval | VCC_RCC_CE);
	vcc_info.status = FR400CC_VCC_ST_VSYNC;
	spin_unlock_irqrestore(&fr400cc_vcc_lock, flags);
}

static void vcc_param_set(unsigned long capture_address)
{
	__u16 rhtcc, rhbc, rhcc, rvcc, rssp, rsep, rhr, rvr;
	__u16 rvbc, rdts;
	int mb93401_dmasiz, dma_w, dma_h;

	kdebug("%s\n", __FUNCTION__);

	rhtcc = vcc_config.rhtcc;
	rhcc = vcc_config.rhcc;
	rhbc = vcc_config.rhbc;
	rvcc = vcc_config.rvcc;
	rvbc = vcc_config.rvbc;
	rssp = vcc_config.rssp;
	rsep = vcc_config.rsep;
	rhr = vcc_config.rhr;
	rvr = vcc_config.rvr;
	rdts = 3; 	/* 32bytes */
	mb93401_dmasiz = get_mb93401_dmasiz(rdts);
	dma_w = (rhcc << 8) / rhr;
	dma_h = (rvcc << 8) / rvr;

	__set_MB93493_VCC(RHSIZE, ((rhtcc << VCC_RHSIZE_RHTCC_SHIFT) & VCC_RHSIZE_RHTCC) |
				  ((rhcc << VCC_RHSIZE_RHCC_SHIFT) & VCC_RHSIZE_RHCC));
	__set_MB93493_VCC(RHBC, rhbc);
	__set_MB93493_VCC(RVCC, rvcc);

	__set_MB93493_VCC(RVBC, (rvbc << VCC_RVBC_RVBC_SHIFT) & VCC_RVBC_RVBC);
	__set_MB93493_VCC(RDTS, rdts << MB93493_VCC_RDTS_SHIFT);

	/* set reduction rate = (h:1.00, v:1.00) */
	__set_MB93493_VCC(RREDUCT, ((rhr << VCC_RREDUCT_RHR_SHIFT) & VCC_RREDUCT_RHR) |
				   ((rvr << VCC_RREDUCT_RVR_SHIFT) & VCC_RREDUCT_RVR));

#ifdef NEVER
	printk("RHTCC\t:%04x = %08x\n"
	       "RHBC\t:%04x = %08x\n"
	       "RVCC\t:%04x  = %08x\n"
	       "RVBC\t:%04x  = %08x\n"
	       "RDTS\t:%04x  = %08x\n"
	       "RHR\t:%04x   = %08x\n",
	       VCC_RHSIZE, (((rhtcc << VCC_RHTCC_RHTCC_SHIFT) & VCC_RHTCC_RHTCC) |
			    ((rhcc << VCC_RHTCC_RHCC_SHIFT) & VCC_RHTCC_RHCC)),
	       VCC_RHBC, rhbc,
	       VCC_RVCC, rvcc,
	       VCC_RVBC, (rvbc << VCC_RVBC_RVBC_SHIFT) & VCC_RVBC_RVBC,
	       VCC_RDTS, rdts << 24,
	       VCC_RREDUCT, (((rhr << VCC_RREDUCT_RHR_SHIFT) & VCC_RREDUCT_RHR) |
			     ((rvr << VCC_RREDUCT_RVR_SHIFT) & VCC_RREDUCT_RVR)));
#endif

	vcc_start_2D(vcc_dma_channel,
		     __addr_MB93493_VCC_TPI(0),
		     capture_address,
		     dma_w * BYTES_PER_PIXEL, dma_h,
		     vcc_config.fw * BYTES_PER_PIXEL,
		     mb93401_dmasiz,
		     DMAC_CCTRx_SAU_HOLD, DMAC_CCTRx_DAU_INC, DMAC_CCFRx_RS_EXTERN);
}

static void vcc_stop(int temp, int nowait)
{
	//unsigned long flags;
  	unsigned long rcc;
	int loop, try;

	kdebug("%s\n", __FUNCTION__);

	// spin_lock_irqsave(&fr400cc_vcc_lock, flags);
	rcc = __get_MB93493_VCC(RCC);

	try = 10;
	while ((rcc & VCC_RCC_CS) && try-- > 0) {
	  	rcc = vcc_info.rccval | VCC_RCC_STP;
		__set_MB93493_VCC(RCC, rcc );

		if (nowait)
			goto skip_wait;

		for (loop = 100000; loop > 0; loop--)
			if (!(__get_MB93493_VCC(RCC) & VCC_RCC_CE))
				break;
	}

	if (rcc & VCC_RCC_CS) {
		printk("%s: fail to stop VCC\n", DRVNAME);
		goto stop_end;
	}

 skip_wait:
	if (temp)
		vcc_info.status = FR400CC_VCC_ST_FRAMEFULL;
	else
		vcc_info.status = FR400CC_VCC_ST_STOP;

 stop_end:
	;
	// spin_unlock_irqrestore(&fr400cc_vcc_lock, flags);
}

static void vcc_disable(void)
{
	unsigned long rcc;
	int loop, try;

	kdebug("%s\n", __FUNCTION__);

	try = 10;
	while ((__get_MB93493_VCC(RCC) & VCC_RCC_CS) && try-- > 0) {
	  	rcc = vcc_info.rccval | VCC_RCC_STP;
		__set_MB93493_VCC(RCC, rcc);

		for (loop = 100000; loop > 0; loop--) {
		  if (!(__get_MB93493_VCC(RCC) & VCC_RCC_CS))
				break;
		  cond_resched();
		}
	}

	if (!(__get_MB93493_VCC(RCC) & VCC_RCC_CS))
		vcc_info.status = FR400CC_VCC_ST_STOP;
#if 0
	else
		printk("%s: fail to stop VCC\n", DRVNAME);
#endif
	if (__get_MB93493_VCC(RCC) & VCC_RCC_CE) {
		for (try = 10; try > 0; try--) {
		  	rcc = vcc_info.rccval | VCC_RCC_STP;
			__set_MB93493_VCC(RCC, rcc);

			for (loop = 100000; loop > 0; loop--) {
			  if (!(__get_MB93493_VCC(RCC) & VCC_RCC_CE))
					return;
				cond_resched();
			}
		}

#if 0
		printk("%s: fail to disable VCC (RCC: %x)\n",
		       DRVNAME, __get_MB93493_VCC(RCC));
#endif
	}
}

static void vcc_capture_start(int num)
{
	unsigned long capture_address;
	int rval;

	kdebug("vcc_capture_start, num=%d\n", num);

	vcc_st_init(num, vcc_config.interlace);
	rval = vcc_st_start();
	switch (rval) {
	case ST_BUFFERFULL:
		vcc_stop(1, 1);
		return;
	case ST_NOREST:
		vcc_stop(0, 1);
		break;
	case ST_SUCCESS:
		break;
	default:
		return;
	}

	if (vcc_info.field == 0) {
		capture_address = (unsigned long) frame_info[vcc_info.frame_head].buffer;
	} else {
		if (vcc_config.skipbf )
			capture_address = (unsigned) dummy_buf_phys;
		else
			capture_address = (unsigned) frame_info[vcc_info.frame_head].buffer +
				vcc_config.fw * (vcc_config.fh >> 1) * BYTES_PER_PIXEL;
	}

	vcc_param_set(capture_address);
	vcc_start();
}

static int vcc_capture_next(int restart_force)
{
	unsigned capture_address;
	int rval, restart;

	kdebug("vcc_capture_next [fl=%d]\n", restart_force);

	if (restart_force) {
		restart = 1;
	} else {
		rval = vcc_st_end();
		if (rval < 0)
			return 1;
		if (rval == 1)
			restart = 1;
		else
			restart = 0;
	}

	rval = vcc_st_start();
	switch (rval) {
	case ST_BUFFERFULL:
		vcc_stop(1, 1);
		return 0;
	case ST_NOREST:
		vcc_stop(0, 1);
		return 0;
	case ST_SUCCESS:
		break;
	}

	if (vcc_info.field == 0) {
		capture_address = (unsigned long) frame_info[vcc_info.frame_head].buffer;
	} else {
		if( vcc_config.skipbf )
			capture_address = (unsigned long) dummy_buf_phys;
		else
			capture_address = (unsigned long) frame_info[vcc_info.frame_head].buffer +
				(vcc_config.fw * (vcc_config.fh>>1) * BYTES_PER_PIXEL);
	}

	vcc_param_set(capture_address);
	if (restart)
		vcc_start();
	return 0;
}

#ifdef NEVER
static void vcc_st_set_rest(int rest, int add)
{
	unsigned long flags;

	spin_lock_irqsave(&fr400cc_vcc_lock, flags);
	if (rest == -1)
		vcc_info.rest = -1;
	else{
		if (add)
			vcc_info.rest += rest;
		else
			vcc_info.rest = rest;
	}
	spin_unlock_irqrestore(&fr400cc_vcc_lock, flags);
}

static int vcc_capture_stop(void)
{
	vcc_st_set_rest(0, 0);
	return 0;
}
#endif

static int vcc_capture_wait(struct fr400cc_vcc_wait *info, int noblock)
{
	unsigned long flags;
	int i, frame, retval;

	DECLARE_WAITQUEUE(myself, current);

	if (info->num > BUFFER_NUM || info->num <= 0)
		return -EINVAL;

	kdebug("%s\n", __FUNCTION__);

	retval = 0;

	info->first_frame = vcc_info.frame_tail_k;
	spin_lock_irqsave(&fr400cc_vcc_lock, flags);

	/* if blocked, we must wait for sufficient data to accrue */
	if ((noblock == 0) && (vcc_info.frame_num_k < info->num)) {
		add_wait_queue(&vcc_info.wait, &myself);

		for (;;) {
			set_current_state(TASK_INTERRUPTIBLE);
			if (vcc_info.frame_num_k >= info->num)
				break;

			spin_unlock_irqrestore(&fr400cc_vcc_lock, flags);

			if (signal_pending(current))
				goto interrupted;

			schedule();

			spin_lock_irqsave(&fr400cc_vcc_lock, flags);
		}

		remove_wait_queue(&vcc_info.wait, &myself);
	}

	set_current_state(TASK_RUNNING);

	/* see what the yield was */
	retval = -EAGAIN;
	info->captured_num = vcc_info.frame_num_k;
	if (info->captured_num == 0)
		goto out;

	if (info->captured_num >= info->num)
		info->captured_num = info->num;

	vcc_info.frame_num_k -= info->captured_num;
	vcc_info.frame_num_u += info->captured_num;

	for (i = 0; i < info->captured_num; i++) {
		frame = (vcc_info.frame_tail_k + i) % BUFFER_NUM;
		if (frame_info[frame].status != FR400CC_VCC_FI_CAPTURED_K)
			printk("%s: vcc_ioctl_wait: broken frame_info[]\n", DRVNAME);
		frame_info[frame].status = FR400CC_VCC_FI_CAPTURED_U;
	}

	vcc_info.frame_tail_k = (vcc_info.frame_tail_k + info->num) % BUFFER_NUM;
	retval = 0;

 out:
	spin_unlock_irqrestore(&fr400cc_vcc_lock, flags);
	return retval;

 interrupted:
	set_current_state(TASK_RUNNING);
	remove_wait_queue(&vcc_info.wait, &myself);
	return -ERESTARTSYS;
}

static int vcc_capture_free(int num)
{
	unsigned long flags;
	int i, frame;

	if (num > BUFFER_NUM || num <= 0)
		return -EINVAL;

	if (num > vcc_info.frame_num_u)
		num = vcc_info.frame_num_u;

	kdebug("vcc_capture_free\n");

	spin_lock_irqsave(&fr400cc_vcc_lock, flags);
	vcc_info.frame_num_u -= num;

	for (i = 0; i < num; i++) {
		frame = (vcc_info.frame_tail_u + i) % BUFFER_NUM;
		if (frame_info[frame].status != FR400CC_VCC_FI_CAPTURED_U)
			printk("%s: vcc_ioctl_free: broken frame_info[]\n", DRVNAME);
		frame_info[frame].status = FR400CC_VCC_FI_FREE;
		vcc_rbuf_tail = READ_BUF_WRAP(vcc_rbuf_tail + 1);
	}

	vcc_info.frame_tail_u  = (vcc_info.frame_tail_u + num) % BUFFER_NUM;

	if (vcc_info.status == FR400CC_VCC_ST_FRAMEFULL) {
		unsigned long flags;
		spin_lock_irqsave(&fr400cc_defer_dma_lock, flags);
		if (defer_dma_start)
			capture_next_at_vsync = 1;
		else
			vcc_capture_next(1);
		spin_unlock_irqrestore(&fr400cc_defer_dma_lock, flags);
	}

	spin_unlock_irqrestore(&fr400cc_vcc_lock, flags);
	return num;
}

static void vcc_vsync_start(void)
{
	vcc_st_init(0, vcc_config.interlace);
	vcc_vsync_start_1();
}

static int vcc_ioctl_start(struct inode *inode, struct file *file, int num)
{
	int retval;

	down(&vcc_sem);
	if ((retval = vcc_config_check(&vcc_config)) < 0)
		goto func_end;

	retval = -EBUSY;
	if (vcc_info.status != FR400CC_VCC_ST_STOP)
		goto func_end;

	retval = -EINVAL;
	if (num == -1 || num > 0) {
		vcc_capture_start(num);
		retval = 0;
	}

 func_end:
	up(&vcc_sem);
	return retval;
}

static int vcc_ioctl_stop(struct inode *inode, struct file *file)
{
	down(&vcc_sem);
	frv_dma_stop(vcc_dma_channel);
	vcc_disable();
	up(&vcc_sem);
	return 0;
}

static int vcc_ioctl_wait(struct inode *inode, struct file *file,
		    struct fr400cc_vcc_wait *info, int noblock)
{
	int retval;

	down(&vcc_sem);
	retval = vcc_capture_wait(info, noblock);
	up(&vcc_sem);
	return retval;
}

static int vcc_ioctl_free(struct inode *inode, struct file *file,
		     int num)
{
	int retval;

	down(&vcc_sem);
	retval = vcc_capture_free(num);
	up(&vcc_sem);
	return retval;
}

static int vcc_ioctl_vsync_start(struct inode *inode, struct file *file)
{
	int retval;

	down(&vcc_sem);

	retval = vcc_config_check(&vcc_config);
	if (retval == 0) {
		retval = -EBUSY;
		if (vcc_info.status == FR400CC_VCC_ST_STOP) {
			vcc_vsync_start();
			retval = 0;
		}
	}

	up(&vcc_sem);
	return retval;
}

static int vcc_ioctl_vsync_stop(struct inode *inode, struct file *file)
{
	down(&vcc_sem);
	vcc_disable();
	up(&vcc_sem);
	return 0;
}

static int vcc_config_check(struct fr400cc_vcc_config *conf)
{
	int capsize;

	kdebug("%s\n", __FUNCTION__);

#if 0
	if ((conf->fw * conf->fh * BYTES_PER_PIXEL) > FRAME_BYTES) {
		printk("fr400cc vcc: vcc_config error (VCCIOCSCFG: too large [(FW*FH*2)<=%d])\n",
		       FRAME_BYTES);
		return -EINVAL;
	}

	if ((conf->rhcc << 8) % conf->rhr != 0) {
		printk("fr400cc vcc: vcc_config error (VCCIOCSCFG: RHR)\n");
		return -EINVAL;
	}

	if ((conf->rvcc << 8) % conf->rvr != 0) {
		printk("fr400cc vcc: vcc_config error (VCCIOCSCFG: RVR)\n");
		return -EINVAL;
	}

	if ((conf->rhcc << 8) / conf->rhr > conf->fw) {
		printk("fr400cc vcc:"
		       " vcc_config error (VCCIOCSCFG: too large (RHCC*0x100/RVR <= %d)\n",
		       conf->fw);
		return -EINVAL;
	}
#endif

	capsize = ((conf->rhcc << 8) / conf->rhr) * ((conf->rvcc << 8) / conf->rvr) *
		BYTES_PER_PIXEL;

	if ((!conf->skipbf) || (!conf->interlace))
		capsize <<= 1;

	kdebug("capsize = %d\n", capsize);

	if (capsize > FRAME_BYTES) {
		printk("%s: vcc_config error (VCCIOCSCFG: too large capsize[<= %d]\n",
		       DRVNAME, FRAME_BYTES);
		return -EINVAL;
	}

	return 0;
}

static int regio_offset_chk(unsigned offset)
{
	return (0x104 <= offset && offset < 0x1a0) ? 0 : -1;
}

static int vcc_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	struct fr400cc_vcc_frame_info uframe;
	struct fr400cc_vcc_config uconfig;
	struct fr400cc_vcc_regio uregio;
	struct fr400cc_vcc_wait uwait;
	struct fr400cc_vcc_info uinfo;
	unsigned long flags;
	int i, frame, status, retval = 0;

	kdebug("%s: ", __FUNCTION__);

	switch (cmd) {
	case VCCIOCGCFG:
		kdebug("VCCIOCGCFG\n");
		spin_lock_irqsave(&fr400cc_vcc_lock, flags);
		uconfig = vcc_config;
		spin_unlock_irqrestore(&fr400cc_vcc_lock, flags);

		if (copy_to_user((struct fr400cc_vcc_config *) arg,
				 &uconfig,
				 sizeof(struct fr400cc_vcc_config)))
			retval = -EFAULT;
		break;

	case VCCIOCSCFG:
		kdebug("VCCIOCSCFG\n");
		retval = -EFAULT;
		if (copy_from_user(&uconfig,
				   (struct fr400cc_vcc_config *) arg,
				   sizeof(struct fr400cc_vcc_config))
		    )
			goto ioctl_end;

		retval = -EBUSY;
		if (vcc_info.status != FR400CC_VCC_ST_STOP)
			goto ioctl_end;

		if ((retval = vcc_config_check(&uconfig)) < 0)
			goto ioctl_end;

		spin_lock_irqsave(&fr400cc_vcc_lock, flags);
		vcc_config = uconfig;
		VCC_COMPUTE_RCC(vcc_info.rccval, vcc_config);
		spin_unlock_irqrestore(&fr400cc_vcc_lock, flags);
		break;

	case VCCIOCGMBUF:
		kdebug("VCCIOCGMBUF\n");
		if (copy_to_user((struct fr400cc_vcc_mbuf *)arg, &vcc_mbuf,
				 sizeof(struct fr400cc_vcc_mbuf)))
			retval = -EFAULT;
		break;

	case VCCIOCSTART:
		kdebug("VCCIOCSTART\n");
		retval = vcc_ioctl_start(inode, file, arg);
		break;

	case VCCIOCSTOP:
		kdebug("VCCIOCSTOP\n");
		retval = vcc_ioctl_stop(inode, file);
		status = frame_info[vcc_info.frame_head].status;
		if (status == FR400CC_VCC_FI_CAPTURING) {
		  frame_info[vcc_info.frame_head].status = FR400CC_VCC_FI_FREE;
		}
		break;

	case VCCIOCWAIT:
		kdebug("VCCIOCWAIT\n");
		retval = -EFAULT;
		if (copy_from_user(&uwait, (struct fr400cc_vcc_wait *) arg, sizeof(uwait)))
			break;

		retval = vcc_ioctl_wait(inode, file, &uwait, 0);
		if (copy_to_user((struct fr400cc_vcc_wait *) arg, &uwait, sizeof(uwait)))
			retval = -EFAULT;
		break;

	case VCCIOCWAITNB:
		kdebug("VCCIOCWAITNB\n");
		retval = -EFAULT;
		if (copy_from_user(&uwait, (struct fr400cc_vcc_wait *) arg, sizeof(uwait)))
			break;

		retval = vcc_ioctl_wait(inode, file, &uwait, 1);
		if (copy_to_user((struct fr400cc_vcc_wait *) arg, &uwait, sizeof(uwait)))
			retval = -EFAULT;
		break;

	case VCCIOCFREE:
		kdebug("VCCIOCFREE\n");
		retval = vcc_ioctl_free(inode, file, arg);
		break;

	case VCCIOCGINF:
		kdebug("VCCIOCGINF\n");
		spin_lock_irqsave(&fr400cc_vcc_lock, flags);
		uinfo.status = vcc_info.status;
		uinfo.count = vcc_info.count;
		uinfo.field = vcc_info.field;
		uinfo.frame_num_k = vcc_info.frame_num_k;
		uinfo.frame_num_u = vcc_info.frame_num_u;
		uinfo.rest = vcc_info.rest;
		uinfo.total_lost = vcc_info.total_lost;
		uinfo.total_lost_bf = vcc_info.total_lost_bf;
		uinfo.total_ov = vcc_info.total_ov;
		uinfo.total_err = vcc_info.total_err;
		spin_unlock_irqrestore(&fr400cc_vcc_lock, flags);

		if (copy_to_user((struct fr400cc_vcc_info *) arg, &uinfo, sizeof(uinfo)))
			retval = -EFAULT;
		break;

		/*  2003.07.31 Add START */
	case VCCIOCGSTTIME:
		kdebug("VCCIOCGSTTIME\n");
		if (copy_to_user((struct timeval *)arg, &vcc_start_tm, sizeof(struct timeval)))
			retval = -EFAULT;
		break;

		/*  2003.07.31 Add END */
		// 2003.08.14 add  START
	case VCCIOCSCFINF:
		kdebug("VCCIOCSCFINF\n");
		if (vcc_info.status != FR400CC_VCC_ST_STOP) {
			retval = -EBUSY;
			goto ioctl_end;
		}

		spin_lock_irqsave(&fr400cc_vcc_lock, flags);

		for (i = 0; i < BUFFER_NUM; i++) {
		  	frame_info[i].status = FR400CC_VCC_FI_FREE;
			frame_info[i].count = 0;
			frame_info[i].lost_count = 0;
			memset(&frame_info[i].captime,0,sizeof(struct timeval));
		}
		// vcc_info.vsync = 0;
		vcc_info.vsync_field_last_captured = 0;
		vcc_info.vsync_frame_last_captured = 0;
		vcc_info.status = FR400CC_VCC_ST_STOP;
		vcc_info.frame_head = 0;
		vcc_info.frame_tail_k = 0;
		vcc_info.frame_tail_u = 0;
		vcc_info.frame_num_k = 0;
		vcc_info.frame_num_u = 0;
		vcc_info.total_lost_bf = 0;
		vcc_info.total_ov = 0;
		vcc_info.total_err = 0;
		vcc_info.field = 0;
		vcc_rbuf_tail = vcc_rbuf_head;

		spin_unlock_irqrestore(&fr400cc_vcc_lock, flags);
		break;

		// 2003.08.14 add  END
	case VCCIOCGFINF:
		kdebug("VCCIOCGFINF\n");
		if (copy_from_user(&uframe, (struct fr400cc_vcc_frame_info *) arg, sizeof(uframe))) {
			retval = -EFAULT;
			goto ioctl_end;
		}

		frame = uframe.frame;

		spin_lock_irqsave(&fr400cc_vcc_lock, flags);
		uframe.status = frame_info[frame].status;
		uframe.count = frame_info[frame].count;
		uframe.lost_count = frame_info[frame].lost_count;
		uframe.captime = frame_info[frame].captime;
		spin_unlock_irqrestore(&fr400cc_vcc_lock, flags);

		if (copy_to_user((struct fr400cc_vcc_frame_info *) arg, &uframe, sizeof(uframe)))
			retval = -EFAULT;
		break;

	case VCCIOCVSSTART:
		kdebug("VCCIOCVSSTART\n");
		retval = vcc_ioctl_vsync_start(inode, file);
		break;

	case VCCIOCVSSTOP:
		kdebug("VCCIOCVSSTOP\n");
		retval = vcc_ioctl_vsync_stop(inode, file);
		break;

	case VCCIOCGREGIO:
		kdebug("VCCIOCGREGIO\n");
		if (copy_from_user(&uregio, (struct fr400cc_vcc_regio *) arg, sizeof(uregio))) {
			retval = -EFAULT;
			goto ioctl_end;
		}

		if ((uregio.reg_offset & 3) != 0 || regio_offset_chk(uregio.reg_offset) != 0) {
			retval = -EINVAL;
			goto ioctl_end;
		}

		uregio.value = __get_MB93493(uregio.reg_offset);
		if (copy_to_user((struct fr400cc_vcc_regio *)arg, &uregio, sizeof(uregio)))
			retval = -EFAULT;
		break;

	case VCCIOCSREGIO:
		kdebug("VCCIOCSREGIO\n");
		if (copy_from_user(&uregio, (struct fr400cc_vcc_regio *) arg, sizeof(uregio))) {
			retval = -EFAULT;
			goto ioctl_end;
		}

		if ((uregio.reg_offset & 3) != 0 || regio_offset_chk(uregio.reg_offset) != 0) {
			retval = -EINVAL;
			goto ioctl_end;
		}
		__set_MB93493(uregio.reg_offset, uregio.value);
		break;

#ifdef FR400CC_VCC_DEBUG_IOCTL
	case __VCCIOCGREGIO:
	{
		struct __fr400cc_vcc_regio regio;

		kdebug("__VCCIOCGREGIO\n");
		if (copy_from_user(&regio, (struct __fr400cc_vcc_regio *) arg, sizeof(regio))) {
			retval = -EFAULT;
			goto ioctl_end;
		}
		regio.value = __get_MB93493(regio.reg_no);
		if (copy_to_user((struct __fr400cc_vcc_regio *) arg, &regio, sizeof(regio)))
			retval = -EFAULT;
	}
	break;

	case __VCCIOCSREGIO:
	{
		struct __fr400cc_vcc_regio regio;

		kdebug("__VCCIOCSREGIO\n");
		if (copy_from_user(&regio, (struct __fr400cc_vcc_regio *) arg, sizeof(regio))) {
			retval = -EFAULT;
			goto ioctl_end;
		}
		__set_MB93493(regio.reg_no, regio.value);
	}
	break;
#endif

	default:
		retval = -ENOIOCTLCMD;
		break;
	}

 ioctl_end:
	return retval;
}

/*****************************************************************************/
/*
 * on uClinux, specify exactly the address at which the mapping must be made
 */
#ifdef NO_MM
static unsigned long vcc_get_unmapped_area(struct file *file,
					   unsigned long addr, unsigned long len,
					   unsigned long pgoff, unsigned long flags)
{
	return imagebuffer_phys;
} /* end vcc_get_unmapped_area() */
#endif

int vcc_mmap(struct file *file, struct vm_area_struct *vma)
{
#ifndef NO_MM
	if (vma->vm_pgoff != 0 || vma->vm_end - vma->vm_start > imagebuffer_size)
		return -EINVAL;

	/* accessing memory through a file pointer marked O_SYNC will be done uncached */
	if (file->f_flags & O_SYNC)
		vma->vm_page_prot = __pgprot(pgprot_val(vma->vm_page_prot) | _PAGE_NOCACHE);

	/* don't try to swap out physical pages */
	vma->vm_flags |= VM_RESERVED;

	/* don't dump addresses that are not real memory to a core file */
	if (file->f_flags & O_SYNC)
		vma->vm_flags |= VM_IO;

	if (remap_page_range(vma->vm_start,
			     imagebuffer_phys,
			     vma->vm_end - vma->vm_start,
			     vma->vm_page_prot))
		return -EAGAIN;
	return 0;

#else
	int ret;

	ret = -EINVAL;
	if (vma->vm_start == imagebuffer_phys &&
	    vma->vm_end - vma->vm_start <= FRAME_BYTES * BUFFER_NUM &&
	    vma->vm_pgoff == 0)
		ret = (int) imagebuffer_phys;

	return ret;
#endif /* NO_MM */
}

static void vcc_dma_interrupt(int dmachan, unsigned long cstr, void *data, struct pt_regs *regs)
{
	kdebug("%s:\n", __FUNCTION__);

#ifdef VCC_RCC_TO_DBG
	dma_intr_cnt++;
#endif
	do_gettimeofday(&vcc_dma_time);

#if 0
	// 2003.08.27 add s.okada  start
	// CSTR check

	if (!(cstr & 0x00000100)) {
		// if (cstr & 0x00000200)
		//	dmac_reg_write(ch, CSTR, 0);
		printk("%s: vcc_dma interrupt error(%08lx)\n", DRVNAME, cstr);
		return;
	}
	// printk("vcc_dma interrupt ch %d:(%08x)\n",ch,cstr);
	// 2003.08.27 add s.okada   end
#endif

	if(vcc_capture_next(0)) {
	  // Ack the interrupt if we didn't do anything
	  unsigned long flags;
	  spin_lock_irqsave(&fr400cc_defer_dma_lock, flags);
	  frv_dma_stop(dmachan);
	  defer_dma_start = 1;
	  spin_unlock_irqrestore(&fr400cc_defer_dma_lock, flags);
	}

#ifdef VCC_RCC_TO
	if (Enable_RCC_TO(vcc_config))
		vcc_capture_next(0);
#endif
}

static void vcc_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned long ris, flags;

	kdebug("%s\n", __FUNCTION__);

	ris = __get_MB93493_VCC(RIS);
	kdebug("RIS %x\n", ris);

	if (ris & VCC_RIS_VSYNC) {
#ifdef CONFIG_FR400CC_VCC_VSYNC_REPORT
		struct timeval tv;

		do_gettimeofday(&tv);
		vcc_vsync_rbuf_put(FR400CC_VCC_RTYPE_VSYNC, vcc_info.vsync, &tv);
		wake_up_interruptible(&vcc_info.vsync_wait);
#endif

		spin_lock_irqsave(&fr400cc_defer_dma_lock, flags);
		defer_dma_start = 0;
		if (capture_next_at_vsync) {
			capture_next_at_vsync = 0;
			vcc_capture_next(1);
		}
		spin_unlock_irqrestore(&fr400cc_defer_dma_lock, flags);

#if defined(CONFIG_MB93091_CB30) || defined(CONFIG_FR400PDK2_BOARD)
		if (reset_v == 2) {
			vcc_capture_next(1);		/* restart capturing */
#ifdef VCC_RCC_TO
			if (Enable_RCC_TO(vcc_config))
				vcc_capture_next(1);
#endif
			reset_v = 0;
		}
#endif
		// printk("vcc:vsync=%d\n", vcc_info.vsync);
		// if (vcc_info.vsync == 0 ) do_gettimeofday(&vcc_start_tm);
		vcc_info.vsync++;

		if (vcc_info.interlace) {
			int f = vcc_info.vsync - vcc_info.vsync_frame_last_captured;
			if ((f & 1) && f >= 3) {
				if (vcc_info.status == FR400CC_VCC_ST_FRAMEFULL) {
					vcc_info.buffull_lost_count++;
					vcc_info.total_lost++;
					vcc_info.total_lost_bf++;
				} else {
					frame_info[vcc_info.frame_head].lost_count++;
					vcc_info.total_lost++;
				}
			}
		} else {
			if (vcc_info.vsync > vcc_info.vsync_frame_last_captured + 1) {
				if (vcc_info.status == FR400CC_VCC_ST_FRAMEFULL) {
					vcc_info.buffull_lost_count++;
					vcc_info.total_lost++;
					vcc_info.total_lost_bf++;
				} else {
					frame_info[vcc_info.frame_head].lost_count++;
					vcc_info.total_lost++;
				}
			}
		}
	}

	if (ris & VCC_RIS_OV) {
		if (ris & VCC_RIS_OV)
			vcc_info.total_ov++;

		vcc_info.field = 0;
		frame_info[vcc_info.frame_head].status = FR400CC_VCC_FI_FREE;
		if (vcc_info.status != FR400CC_VCC_ST_STOP) {
#if defined(CONFIG_MB93091_CB30) || defined(CONFIG_FR400PDK2_BOARD)
			// printk("vcc:vsync=%d error ris(%08x)\n", vcc_info.vsync,ris);
			frv_dma_stop(vcc_dma_channel);
			vcc_reset();
			// Emergency code
			// vcc_vsync_start_1();
			__set_MB93493_VCC(RCC, VCC_RCC_MOV | VCC_RCC_CSM_INTERLACE | VCC_RCC_CIE | VCC_RCC_CE);
			vcc_info.status = FR400CC_VCC_ST_VSYNC;
			reset_v = 2;
#else
			for(loopcount = 0; loopcount < 10000; loopcount++) {
			  if(vcc_reset() == 0)
			    break;
			}
			vcc_capture_next(1);		/* restart capturing */
#endif
		}
	}

	if (!(ris & (VCC_RIS_VSYNC | VCC_RIS_OV)))
		printk("%s: got interrupt RIS=%08lx\n", DRVNAME, ris);

	__set_MB93493_VCC(RIS, 0);
	kdebug("%s:exit\n", __FUNCTION__);
}

static int vcc_rbuf_put(int type, int count, struct timeval *tv)
{
  if (CIRC_SPACE(vcc_rbuf_head, vcc_rbuf_tail, READ_BUF_LEN) == 0) {
		return -ENOSPC;
  }

	vcc_rbuf[vcc_rbuf_head].type = type;
	vcc_rbuf[vcc_rbuf_head].count = count;
	vcc_rbuf[vcc_rbuf_head].captime = *tv;

	vcc_rbuf_head = READ_BUF_WRAP(vcc_rbuf_head + 1);
	wake_up_interruptible(&vcc_info.wait);
	return 0;
}

static int vcc_rbuf_get(struct fr400cc_vcc_read *buf)
{
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&fr400cc_vcc_lock, flags);

	if (CIRC_CNT(vcc_rbuf_head, vcc_rbuf_tail, READ_BUF_LEN) != 0) {
		*buf = vcc_rbuf[vcc_rbuf_tail];
		vcc_rbuf_tail = READ_BUF_WRAP(vcc_rbuf_tail + 1);
		ret = 1;
	}

	spin_unlock_irqrestore(&fr400cc_vcc_lock, flags);
	return ret;
}


static int vcc_open(struct inode *inode, struct file *file)
{
	int retval;
	int i;

#ifdef VCC_RCC_TO_DBG
	dma_intr_cnt = 0;
#endif

	down(&vcc_sem);

	if (vcc_count) {
		up(&vcc_sem);
		return -EBUSY;
	}

#if defined(CONFIG_MB93091_CB30) || defined(CONFIG_FR400PDK2_BOARD)
	reset_v = 0;
#endif
	defer_dma_start = 0;
	capture_next_at_vsync = 0;

	vcc_dma_channel = mb93493_dma_open("mb93493 vcc",
		 DMA_MB93493_VCC,
		 FRV_DMA_CAP_DREQ,
		 vcc_dma_interrupt,
		 SA_SHIRQ,
		 NULL);

	if(vcc_dma_channel < 0){
		up(&vcc_sem);
		printk("%s: fail to open dma\n", DRVNAME);
		return -1;
	}

#ifdef USE_INTERRUPT
	retval = request_irq(IRQ_MB93493_VCC, vcc_interrupt,
			     SA_INTERRUPT | SA_SHIRQ,
			     "mb93493_vcc", &vcc_info);
	if (retval) {
		up(&vcc_sem);
		printk("%s: fail to allocate irq=%d\n", DRVNAME, IRQ_MB93493_VCC);
		mb93493_dma_close(vcc_dma_channel);
		return retval;
	}
#endif
	kdebug("%s\n", __FUNCTION__);

	vcc_info.status = FR400CC_VCC_ST_STOP;
	vcc_info.frame_head = 0;
	vcc_info.frame_tail_k = 0;
	vcc_info.frame_tail_u = 0;
	vcc_info.frame_num_k = 0;
	vcc_info.frame_num_u = 0;
	vcc_info.total_lost_bf = 0;
	vcc_info.total_ov = 0;
	vcc_info.total_err = 0;

	for (i = 0; i < BUFFER_NUM; i++){
	  	frame_info[i].status = FR400CC_VCC_FI_FREE;
		frame_info[i].buffer = imagebuffer_phys + (FRAME_BYTES * i);
		vcc_mbuf.offsets[i] = FRAME_BYTES * i;
	}

	vcc_config.encoding = FR400CC_VCC_ENCODE_YC16;
	vcc_config.interlace = 1;
	vcc_config.skipbf = 0;
	vcc_config.fw = FRAME_WIDTH;
	vcc_config.fh = FRAME_HEIGHT;
	vcc_config.rhtcc = 858;
	vcc_config.rhcc = 720;
	vcc_config.rhbc = 64;
	vcc_config.rvcc = 240;
	vcc_config.rvbc = 14;
	vcc_config.rssp = 1;
	vcc_config.rsep = vcc_config.rvcc + (vcc_config.rssp - 1);
	vcc_config.rhr = 0x100;
	vcc_config.rvr = 0x100;
	vcc_config.rcc_to = 0;
	vcc_config.rcc_fdts = 0;
	vcc_config.rcc_ifi = 0;
	vcc_config.rcc_es = 0;
	vcc_config.rcc_csm = 1;
	vcc_config.rcc_cfp = 0;
	vcc_config.rcc_vsip = 0;
	vcc_config.rcc_hsip = 0;
	vcc_config.rcc_cpf = 0;
	vcc_rbuf_tail = vcc_rbuf_head;

	VCC_COMPUTE_RCC(vcc_info.rccval, vcc_config);

	vcc_count++;
	up(&vcc_sem);
	return 0;
}

static ssize_t vcc_read(struct file *file, char *buffer, size_t count, loff_t *ppos)
{
	struct fr400cc_vcc_read read_pack;
	int retval;

	DECLARE_WAITQUEUE(myself, current);

	if (count < sizeof(struct fr400cc_vcc_read))
		return -EINVAL;

	kdebug("%s: count=%d\n", __FUNCTION__, count);

	down(&vcc_sem);

	if (CIRC_CNT(vcc_rbuf_head, vcc_rbuf_tail, READ_BUF_LEN) == 0) {
		retval = -EAGAIN;
		if (file->f_flags & O_NONBLOCK)
			goto read_end;

		add_wait_queue(&vcc_info.wait, &myself);

		for (;;) {
			set_current_state(TASK_INTERRUPTIBLE);
			if (CIRC_CNT(vcc_rbuf_head, vcc_rbuf_tail, READ_BUF_LEN) != 0)
				break;

			if (signal_pending(current))
				goto interrupted;

			schedule();
		}

		remove_wait_queue(&vcc_info.wait, &myself);
	}

	retval = 0;

	while (vcc_rbuf_get(&read_pack)) {
		if (copy_to_user(buffer + retval, &read_pack, sizeof(struct fr400cc_vcc_read))) {
			retval = -EFAULT;
			break;
		}

		retval += sizeof(struct fr400cc_vcc_read);
		if (retval > count - sizeof(struct fr400cc_vcc_read))
			break;
	}

 read_end:
	up(&vcc_sem);
	return retval;

 interrupted:
	retval = -ERESTARTSYS;
	remove_wait_queue(&vcc_info.wait, &myself);
	goto read_end;
}

static unsigned int vcc_poll(struct file *file, poll_table * wait)
{
	unsigned int ret;

	poll_wait(file, &vcc_info.wait, wait);

	ret = 0;
	if (CIRC_CNT(vcc_rbuf_head, vcc_rbuf_tail, READ_BUF_LEN) != 0)
		ret = POLLIN | POLLRDNORM;

	return ret;
}

static int vcc_release(struct inode *inode, struct file *file)
{
	down(&vcc_sem);

	if (xchg(&vcc_count, 0) != 1)
		BUG();

#ifdef VCC_RCC_TO_DBG
	printk("dma_intr_cnt=%d\n", dma_intr_cnt);
#endif

	frv_dma_stop(vcc_dma_channel);
	vcc_disable();

	mb93493_dma_close(vcc_dma_channel);
	free_irq(IRQ_MB93493_VCC, &vcc_info);

	up(&vcc_sem);
	return 0;
}

static struct file_operations fr400cc_vcc_capture_fops = {
	.owner		= THIS_MODULE,
	.open		= vcc_open,
	.read		= vcc_read,
	.poll		= vcc_poll,
	.release	= vcc_release,
	.mmap		= vcc_mmap,
	.ioctl		= vcc_ioctl,
#ifdef NO_MM
	.get_unmapped_area	= vcc_get_unmapped_area,
#endif
};

static struct miscdevice fr400cc_vcc_capture_miscdev = {
	.minor		= FR400CC_VCC_CAPTURE_MINOR,
	.name		= "fr400cc vcc capture",
	.fops		= &fr400cc_vcc_capture_fops,
};


#ifdef CONFIG_FR400CC_VCC_VSYNC_REPORT
static int vcc_vsync_count = 0;

static struct fr400cc_vcc_read vcc_vsync_rbuf[READ_BUF_LEN];
static volatile int vcc_vsync_rbuf_head = 0, vcc_vsync_rbuf_tail = 0;

static void vcc_vsync_rbuf_put(int type, int count, struct timeval *tv)
{
	if (CIRC_SPACE(vcc_vsync_rbuf_head, vcc_vsync_rbuf_tail, READ_BUF_LEN) == 0)
		return;

	vcc_vsync_rbuf[vcc_vsync_rbuf_head].type = type;
	vcc_vsync_rbuf[vcc_vsync_rbuf_head].count = count;
	vcc_vsync_rbuf[vcc_vsync_rbuf_head].captime = *tv;

	vcc_vsync_rbuf_head = READ_BUF_WRAP(vcc_vsync_rbuf_head + 1);
	return;
}

static int vcc_vsync_rbuf_get(struct fr400cc_vcc_read *buf)
{
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&fr400cc_vcc_vsync_lock, flags);

	if (CIRC_CNT(vcc_vsync_rbuf_head, vcc_vsync_rbuf_tail, READ_BUF_LEN) != 0) {
		*buf = vcc_vsync_rbuf[vcc_vsync_rbuf_tail];
		vcc_vsync_rbuf_tail = READ_BUF_WRAP(vcc_vsync_rbuf_tail + 1);
		ret = 1;
	}

	spin_unlock_irqrestore(&fr400cc_vcc_vsync_lock, flags);
	return ret;
}

static int vcc_vsync_open(struct inode *inode, struct file *file)
{
	int ret = -EBUSY;

	down(&vcc_vsync_sem);

	if (!vcc_vsync_count) {
		vcc_vsync_count++;
		vcc_vsync_rbuf_tail = vcc_vsync_rbuf_head;
		ret = 0;
	}

	up(&vcc_vsync_sem);
	return 0;
}

static ssize_t vcc_vsync_read(struct file *file, char *buffer, size_t count, loff_t *ppos)
{
	struct fr400cc_vcc_read read_pack;
	int retval;

	DECLARE_WAITQUEUE(myself, current);

	if (count < sizeof(struct fr400cc_vcc_read))
		return -EINVAL;

	down(&vcc_vsync_sem);

	if (CIRC_CNT(vcc_vsync_rbuf_head, vcc_vsync_rbuf_tail, READ_BUF_LEN) == 0) {

		retval = -EAGAIN;
		if (file->f_flags & O_NONBLOCK)
			goto read_end;

		add_wait_queue(&vcc_info.vsync_wait, &myself);

		for (;;) {
			set_current_state(TASK_INTERRUPTIBLE);
			if (CIRC_CNT(vcc_vsync_rbuf_head, vcc_vsync_rbuf_tail, READ_BUF_LEN) != 0)
				break;

			if (signal_pending(current))
				goto interrupted;

			schedule();
		}

		remove_wait_queue(&vcc_info.vsync_wait, &myself);
	}

	retval = 0;

	while (vcc_vsync_rbuf_get(&read_pack)) {
		if (copy_to_user(buffer, &read_pack, sizeof(struct fr400cc_vcc_read))) {
			retval = -EFAULT;
			break;
		}

		retval += sizeof(struct fr400cc_vcc_read);
		if (retval > count - sizeof(struct fr400cc_vcc_read))
			break;

		buffer += sizeof(struct fr400cc_vcc_read);
	}

 read_end:
	up(&vcc_vsync_sem);
	return retval;

 interrupted:
	remove_wait_queue(&vcc_info.vsync_wait, &myself);
	goto read_end;
}

static unsigned int vcc_vsync_poll(struct file *file, poll_table * wait)
{
	unsigned int ret;

	poll_wait(file, &vcc_info.vsync_wait, wait);

	ret = 0;
	if (CIRC_CNT(vcc_vsync_rbuf_head, vcc_vsync_rbuf_tail, READ_BUF_LEN) != 0)
		ret = POLLIN | POLLRDNORM;

	return ret;
}

static int vcc_vsync_release(struct inode *inode, struct file *file)
{
	down(&vcc_vsync_sem);

	if (xchg(&vcc_vsync_count, 0) != 1)
		BUG();

	up(&vcc_vsync_sem);
	return 0;
}

static struct file_operations fr400cc_vcc_vsync_fops={
	.owner		= THIS_MODULE,
	.open		= vcc_vsync_open,
	.read		= vcc_vsync_read,
	.poll		= vcc_vsync_poll,
	.release	= vcc_vsync_release,
};

static struct miscdevice fr400cc_vcc_vsync_miscdev={
	.minor		= FR400CC_VCC_VSYNC_MINOR,
	.name		= "fr400cc vcc vsync",
	.fops		= &fr400cc_vcc_vsync_fops,
};
#endif

/*
 * Allocate a buffer using alloc_pages, return the rounded up size, buffer_pg
 * and physical address.
 */
static int alloc_buffer_pages(size_t *buffer_size, struct page **buffer_pg, unsigned long *buffer_phys)
{
	struct page *page, *pend;
	unsigned long order = PAGE_SHIFT;
	while ((order < 31) && ((*buffer_size) > (1 << order))) {
		order++;
	}
	order -= PAGE_SHIFT;

	(*buffer_pg) = alloc_pages(GFP_HIGHUSER, order);
	if (! (*buffer_pg)) {
		printk("%s: couldn't allocate buffer (order=%lu)\n", DRVNAME, order);
		return -ENOMEM;
	}
	*buffer_size = PAGE_SIZE << order;
	*buffer_phys = page_to_phys(*buffer_pg);

	/* now mark the pages as reserved; otherwise remap_page_range */
	/* doesn't do what we want */
	pend = *buffer_pg + (1 << order);
	for(page = *buffer_pg; page < pend; page++)
	  SetPageReserved(page);



	return 0;
}



static int __devinit fr400cc_vcc_init(void)
{
	int st;
#if defined(CONFIG_MB93091_CB30) || defined(CONFIG_FR400PDK2_BOARD)
	__set_MB93493_LBSER(__get_MB93493_LBSER() | MB93493_LBSER_VCC | MB93493_LBSER_GPIO);

	__set_MB93493_GPIO_SIR(1, __get_MB93493_GPIO_SIR(1) | 0x000000ff);
#endif
	__set_MB93493_GPIO_SIR(0, __get_MB93493_GPIO_SIR(0) | 0x0000ff00);

	imagebuffer_size = FRAME_BYTES * (BUFFER_NUM + 1);
	st = alloc_buffer_pages(&imagebuffer_size, &imagebuffer_pg, &imagebuffer_phys);

	if (st < 0)
		return st;

	dummy_buf_size = FRAME_BYTES;
	dummy_buf_phys = imagebuffer_phys + (FRAME_BYTES * BUFFER_NUM);

	vcc_mbuf.size = FRAME_BYTES * BUFFER_NUM;
	vcc_mbuf.frames = BUFFER_NUM;

#ifdef CONFIG_FR400CC_VCC_VSYNC_REPORT
	init_waitqueue_head(&vcc_info.vsync_wait);
#endif

	printk("Fujitsu MB93493 VCC driver (irq=%d)\n", IRQ_MB93493_VCC);
	kdebug("imagebuffer_phys=%08x\n", (unsigned int)imagebuffer_phys);
	kdebug("imagebuffer_size=%8d\n", imagebuffer_size);
	kdebug("dummy_buf_phys=%08x\n", (unsigned int)dummy_buf_phys);

	vcc_reset();

	misc_register(&fr400cc_vcc_capture_miscdev);

#ifdef CONFIG_FR400CC_VCC_VSYNC_REPORT
	misc_register(&fr400cc_vcc_vsync_miscdev);
	printk("FR400 Companion Chip VCC VSYNC report driver\n");
#endif

	return 0;
}


__initcall(fr400cc_vcc_init);
