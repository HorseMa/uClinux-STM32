/*
 * CL-PS7212/empeg Audio Device Driver
 *
 * (C)2000 empeg ltd, http://www.empeg.com
 *
 * linux/drivers/sound/ep7212_audio.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *   N.B: This driver requires that it be fed data 4608 bytes at a time.
 *        See http://www.mock.com/receiver/demo for demo players or better
 *        yet, fix this!
 *
 * Version history:
 *   Hugo Fiennes <hugo@empeg.com>
 *
 *   22-Feb-2000 - First version
 *   28-Feb-2000 - Now flushes RX fifo to prevent spurious FIQs
 *   29-Feb-2000 - Buffer needs to be half-full after emptying to start DMA
 *                 consumption of data. This helps prevent stuttering on
 *                 startup.
 *   08-Mar-2000 - Use new save_flags_clif() call to disable FIQs during buffer
 *                 twiddling.
 *   02-May-2000 - Added mute control (sets soft mute bit on pcm1716)
 *   07-May-2000 - Added hard mute control (sets hardware mute bit)
 *
 *   Umar Qureshey <uqureshe@cs.ucr.edu>
 *
 *   29-Jul-2002 - Ported to Linux v2.4; removed empeg hardware dependent
 *                 code i.e. mixer and DAC controls.
 *   04-Aug-2002 - Changed save_flags_clif() calls into successive cli()
 *                 and clf() calls because they're "standard" ARM Linux.
 *   05-Aug-2002 - Implemented a new bottom-half code activation in the
 *                 audio_7212dma.S file because the bottom-half calls kernel
 *                 queue functions and was run before the FIQ handler code
 *                 exited.  This crashed the kernel since kernel queue
 *                 functions are not allowed in FIQ handler code (it would
 *                 change an FIQ to an IRQ thus nullifying its advantage).
 *                 Workaround to this issue is activating a normal IRQ in the
 *                 FIQ handler and attaching the bottom-half code to that IRQ
 *                 handler so as soon as the FIQ handler exits, the IRQ is
 *                 executed with the bottom-half functionality.  To generate
 *                 an IRQ, the TC1 timer is loaded with a value of 1 which
 *                 effectively activates an IRQ in one clock cycle to be
 *                 executed after the FIQ code exits.
 *   22-Aug-2002 - Added GPL notice.  This software was originally
 *                 distributed under GPL licensing.  Added player notice.
 *
 *	Petko Manolov <petkan@nucleusys.com>, http://www.nucleusys.com/
 *
 *   29-Jul-2003 - Modularizing the driver and writing exit routine.
 *   31-Jul-2003 - Now the mono mode conversion is done in the fiq handler and
 *                 check if it's size is greater than 0x1e4.
 *                 Code cleanups, driver ported to edb7312.  The DAI clock
 *                 source is now a config option.
 *                 
 */

#include <linux/module.h>
#include <linux/config.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/major.h>
#include <linux/errno.h>
#include <linux/tqueue.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/vmalloc.h>
#include <linux/soundcard.h>
#include <linux/delay.h>
#include <asm/segment.h>
#include <asm/irq.h>
#include <asm/fiq.h>
#include <asm/io.h>
#include <asm/arch/hardware.h>
#include <asm/uaccess.h>
#include <asm/hardware/clps7111.h>


#define	DRIVER_AUTHOR	"Hugo Fiennes, Umar Qureshey and Petko Manolov"
#define	DRIVER_DESC	"ep7212/ep7312 audio driver"
MODULE_AUTHOR( DRIVER_AUTHOR );
MODULE_DESCRIPTION( DRIVER_DESC );
MODULE_LICENSE("GPL");

#ifdef	CONFIG_PROC_FS
#include <linux/stat.h>
#include <linux/proc_fs.h>
#endif

#define	DSP_PURGE			1
#undef	MIXER

/*
 * Debugging
 */
#if 0
#define AUDIO_DEBUG			1
#define AUDIO_DEBUG_VERBOSE		1
#define AUDIO_DEBUG_STATS		1 /*AUDIO_DEBUG | AUDIO_DEBUG_VERBOSE*/
#endif

/*
 * Constants
 */

/* Names */
#define AUDIO_NAME			"audio-ep7212"
#define AUDIO_NAME_VERBOSE		"empeg clps7212/edb7312 audio driver"
#define AUDIO_VERSION_STRING		"Revision: 0.03"

/* device numbers */
#define AUDIO_MAJOR			(245)	//(EMPEG_AUDIO_MAJOR)
#define AUDIO_NMINOR			(5)
#define	MIXER_MINOR			(0)
#define DSP_MINOR			(3)
#define AUDIO_MINOR			(4)

/* Client parameters */
#define AUDIO_NOOF_BUFFERS		(32)	/* Number of audio buffers */
/* NOTE! Need to move this to a header file as audio_7212dma needs this too */

#define AUDIO_BUFFER_SIZE		(4608)	/* Size of user buffer chunks */

/* Only EXTCLK mode works on the EDB7312. */
#ifdef CONFIG_ARCH_EDB7312
#define CONFIG_SOUND_EP7312_EXTCLK
#endif

extern int setup_cs43l42(void);

/*
 * Types
 */

/* statistics */
typedef struct {
	ulong samples;
	ulong interrupts;
	ulong wakeups;
	ulong fifo_err;
	ulong buffer_hwm;
	ulong first_fills;
	ulong below_halfs;
} audio_stats;

typedef struct {
	/* Buffer */
	unsigned char data[AUDIO_BUFFER_SIZE];
} audio_buf;

static int resid_size;
static unsigned char resid_buf[AUDIO_BUFFER_SIZE];

/* NOTE! This struct is accessed directly by audio_7212dma.S, and so the
   order and type of the first 7 entries are critical and should not be
   changed without appropriate modifications to the dma code */
typedef struct {
	/* Buffers */
	volatile int used;	/* Do not move */
	volatile int free;	/* Do not move */
	volatile int head;	/* Do not move */
	volatile int tail;	/* Do not move */
	audio_buf *buffers;	/* Do not move */
	volatile int mode;	/* Do not move */
	int state;		/* Do not move: Is DMA enabled? */
	audio_buf *zero;	/* Do not move */
	volatile int vbase;

	/* Stack for FIQ routine - not referenced by fiq code */
	int *fiqstack;		/* now defunct; should be removed UQ */

	/* Buffer management */
	wait_queue_head_t waitq;

	/* Statistics */
	audio_stats stats;
} audio_dev;

/* Devices in the system; just the one channel at the moment */
static audio_dev audio[1];
static int sample_rate;
static struct tq_struct emit_task;

#ifdef CONFIG_PROC_FS
static struct proc_dir_entry *proc_audio;
static int audio_read_proc(char *buf, char **start, off_t offset, int length,
			   int *eof, void *private)
{
	audio_dev *dev = &audio[0];

	length = 0;
	length += sprintf(buf + length, "buffers   : %d\n", AUDIO_NOOF_BUFFERS);
	length += sprintf(buf + length, "buf size  : %d\n", AUDIO_BUFFER_SIZE);
	length += sprintf(buf + length, "samples   : %ld\n", dev->stats.samples);
	length += sprintf(buf + length, "interrupts: %ld\n", dev->stats.interrupts);
	length += sprintf(buf + length, "wakeups   : %ld\n", dev->stats.wakeups);
	length += sprintf(buf + length, "fifo errs : %ld\n", dev->stats.fifo_err);
	length += sprintf(buf + length, "buffer hwm: %ld\n", dev->stats.buffer_hwm);
	length += sprintf(buf + length, "first fill: %ld\n", dev->stats.first_fills);
	length += sprintf(buf + length, "below half: %ld\n", dev->stats.below_halfs);

	return (length);
}

#endif				/* CONFIG_PROC_FS */

/* FIQ stuff */
static struct fiq_handler fh = { NULL, "audiodma", NULL, NULL };
extern char audio_7212_fiq, audio_7212_fiqend;

void newbuffer(int irq, void *dev_id, struct pt_regs *regs)
{
	queue_task(&emit_task, &tq_immediate);
	mark_bh(IMMEDIATE_BH);
}

static void emit_action(void *p)
{
	audio_dev *dev = &audio[0];

	/* If we have just dipped below halfway, note this */
	if (dev->used == ((AUDIO_NOOF_BUFFERS / 2) - 1))
		dev->stats.below_halfs++;

	wake_up_interruptible(&audio[0].waitq);
}

/* ***** arbitrary size buffers ***** */
static int audio_write(struct file *file, const char *buffer, size_t count,
		       loff_t * ppos)
{
	audio_dev *dev = file->private_data;
	int total = 0;
	int local_buffer = 0;

#if AUDIO_DEBUG_VERBOSE
	printk(AUDIO_NAME
	       ": audio_write: count=%d, used=%d free=%d head=%d tail=%d\n",
	       count, dev->used, dev->free, dev->head, dev->tail);
#endif

	if (count <= 0) {
		printk("ep7212_audio: %d byte write\n", count);
		return 0;
	}

	/* Any space left? (No need to disable IRQs: we're just checking for
	   a full buffer condition) */
	if (dev->free == 0) {
		if (file->f_flags & O_NONBLOCK) {
			return (-EAGAIN);
		}

		while (dev->free < 8) {
			/* next buffer full */
			interruptible_sleep_on(&dev->waitq);
		}
	}

	/* If we're filling an empty buffer, note this */
	if (dev->used == 0)
		dev->stats.first_fills++;

	/* if we have a residual buffer, try and use that first */
	if (resid_size > 0 && (resid_size + count) >= AUDIO_BUFFER_SIZE) {
		int remaining;
		unsigned long flags;

		if (0)
			printk("resid_size %d, count %d\n", resid_size, count);

		/* grab a buffer */
		save_flags(flags);
		cli();
		clf();
		dev->free--;
		restore_flags(flags);

		/* use residual first */
		memcpy(dev->buffers[dev->head].data, resid_buf, resid_size);

		/* if there's space, fill from user's write */
		if (resid_size < AUDIO_BUFFER_SIZE) {
			remaining = AUDIO_BUFFER_SIZE - resid_size;

			if (0)
				printk("remaining %d\n", remaining);
			if (local_buffer)
				memcpy(dev->buffers[dev->head].data +
				       resid_size, buffer, remaining);
			else
				copy_from_user(dev->buffers[dev->head].data +
					       resid_size, buffer, remaining);

			buffer += remaining;
			count -= remaining;
			total += remaining;
		}
//              fix_buffer(dev->buffers[dev->head].data);

		dev->head++;
		if (dev->head == AUDIO_NOOF_BUFFERS)
			dev->head = 0;

		dev->stats.samples += AUDIO_BUFFER_SIZE;
		resid_size = 0;

		/* buffer is ready, tell the FIQ section there's new data */
		save_flags(flags);
		cli();
		clf();
		dev->used++;
		restore_flags(flags);
	}

	if (0)
		printk("before loop, count %d\n", count);

	while (dev->free < 8) {
		/* next buffer full */
		interruptible_sleep_on(&dev->waitq);
	}

	/* fill as many buffers as we can */
	while (count >= AUDIO_BUFFER_SIZE) {
		unsigned long flags;

		if (dev->free == 0) {
			if (file->f_flags & O_NONBLOCK) {
				return total;
			}

			/* next buffer full */
			interruptible_sleep_on(&dev->waitq);
		}

		/* Critical sections kept as short as possible to give good
		   latency for other tasks: note we disable FIQs here too */
		save_flags(flags);
		cli();
		clf();
		dev->free--;
		restore_flags(flags);

		/* Copy chunk of data from user-space. We're safe updating the
		   head when not in cli() as this is the only place the head gets  twiddled */
		if (local_buffer)
			memcpy(dev->buffers[dev->head++].data,
			       buffer, AUDIO_BUFFER_SIZE);
		else
			copy_from_user(dev->buffers[dev->head++].data,
				       buffer, AUDIO_BUFFER_SIZE);

//              fix_buffer(dev->buffers[dev->head-1].data);

		if (dev->head == AUDIO_NOOF_BUFFERS)
			dev->head = 0;
		total += AUDIO_BUFFER_SIZE;
		dev->stats.samples += AUDIO_BUFFER_SIZE;
		count -= AUDIO_BUFFER_SIZE;
		buffer += AUDIO_BUFFER_SIZE;

		/* buffer is ready, tell the FIQ section there's new data */
		save_flags(flags);
		cli();
		clf();
		dev->used++;
		restore_flags(flags);
	}

	/* keep residual in a local buffer */
	if (count > 0) {
		if (0)
			printk("tail count %d\n", count);

		if (resid_size + count > AUDIO_BUFFER_SIZE)
			count = AUDIO_BUFFER_SIZE - resid_size;

		if (local_buffer)
			memcpy(&resid_buf[resid_size], buffer, count);
		else
			copy_from_user(&resid_buf[resid_size], buffer, count);

		resid_size += count;
		total += count;
	}

	/* Update hwm */
	if (dev->used > dev->stats.buffer_hwm)
		dev->stats.buffer_hwm = dev->used;

	/* If we were empty before, enable DMA if we have passed the half-full
	   threshold */
	if (dev->state == 0 && dev->used >= (AUDIO_NOOF_BUFFERS / 2))
		dev->state = 1;

	if (0)
		printk("total %d\n", total);

	/* Write complete */
	return (total);
}

/* Throw away all complete blocks waiting to go out to the DAC and return how
   many bytes that was. */
static int audio_purge(audio_dev * dev)
{
	unsigned long flags;
	int bytes;
	struct pt_regs regs;

	/* We don't want DMA to come in here! */
	save_flags(flags);
	cli();
	clf();

	/* Work out how many bytes are left to send to the audio device:
	   we only worry about full buffers */
	bytes = dev->used * AUDIO_BUFFER_SIZE;

	/* Empty buffers */
	dev->head = dev->tail = dev->used = 0;
	dev->free = AUDIO_NOOF_BUFFERS - 1;

	/* Stop DMA handler */
	get_fiq_regs(&regs);
	regs.ARM_fp = 0;	/* r11 */
	regs.ARM_ip = 0;	/* r12 */
	set_fiq_regs(&regs);

	/* Let it run again */
	restore_flags(flags);

	return bytes;
}

#define DAI64FS		0x2600
#define I2SF64		(1<<0)
#define AUDCLKEN	(1<<1)
#define AUDCLKSRC	(1<<2)
#define MCLK256EN	(1<<3)
#define LOOPBACK	(1<<5)
#define	RATE_48K	(6<<8)
#define RATE_44K	(2<<8)	/* assumes 11.2896Mhz external clock */
#define	RATE_32K	(9<<8)
#define	RATE_24K	(12<<8)
#define RATE_22K	(4<<8)	/* assumes 11.2896Mhz external clock */
#define	RATE_16K	(18<<8)
#define	RATE_12K	(24<<8)
#define RATE_11K	(8<<8)	/* assumes 11.2896Mhz external clock */
#define	RATE_8K		(36<<8)
#define	RATE_MASK	(0x7f<<8)

static int set_sample_rate(int rate)
{
	int tmp;
	
	tmp = clps_readl(DAI64FS);
	tmp &= ~RATE_MASK;

	switch (rate) {
#ifdef CONFIG_SOUND_EP7312_EXTCLK
	case 44100:
		clps_writel(tmp | RATE_44K, DAI64FS);
		sample_rate = rate;
		break;
	case 22050:
		clps_writel(tmp | RATE_22K, DAI64FS);
		sample_rate = rate;
		break;
	case 11025:
		clps_writel(tmp | RATE_11K, DAI64FS);
		sample_rate = rate;
		break;
#else	
	case 48000:
		clps_writel(tmp | RATE_48K, DAI64FS);
		sample_rate = rate;
		break;
	case 32000:
		clps_writel(tmp | RATE_32K, DAI64FS);
		sample_rate = rate;
		break;
	case 24000:
		clps_writel(tmp | RATE_24K, DAI64FS);
		sample_rate = rate;
		break;
	case 16000:
		clps_writel(tmp | RATE_16K, DAI64FS);
		sample_rate = rate;
		break;
	case 12000:
		clps_writel(tmp | RATE_12K, DAI64FS);
		sample_rate = rate;
		break;
	case 8000:
		clps_writel(tmp | RATE_8K, DAI64FS);
		sample_rate = rate;
		break;
#endif
	default:
		printk("unsupported sampling rate %d\n", rate);
		return -1;
	}

	return 0;
}

static int audio_ioctl(struct inode *inode, struct file *file, uint command,
		       ulong arg)
{
	audio_dev *dev = file->private_data;
	int val;

	switch (command) {
	case SNDCTL_DSP_RESET:
		resid_size = 0;
#ifdef AUDIO_DEBUG
		printk("ep7212_audio: reset\n");
#endif
		return 0;

	case SNDCTL_DSP_SYNC:
		resid_size = 0;
#ifdef AUDIO_DEBUG
		printk("ep7212_audio: sync\n");
#endif
		return 0;

	case SNDCTL_DSP_SPEED:
		if (get_user(val, (int *) arg))
			return -EFAULT;
#ifdef AUDIO_DEBUG
		if (val >= 0) {
			printk("ep7212_audio: set sample rate %d\n", val);
		}
#endif
		if (val != 44100)
			return 1;
		else
			return 0;
		sample_rate = val;
		set_sample_rate(sample_rate);
		return put_user(sample_rate, (int *) arg);

	case SNDCTL_DSP_SETFRAGMENT:
		if (get_user(val, (long *) arg))
			return -EFAULT;
#ifdef AUDIO_DEBUG
		printk("ep7212_audio: set frag 0x%08x\n", val);
#endif
		return 0;

	case SNDCTL_DSP_SETFMT:	/* Selects ONE fmt */
		if (get_user(val, (long *) arg))
			return -EFAULT;
#ifdef AUDIO_DEBUG
		printk("ep7212_audio: set format 0x%08x\n", val);
#endif
		return 0;

	case SNDCTL_DSP_CHANNELS:
		if (get_user(val, (int *) arg))
			return -EFAULT;
		if (val == 1)
			dev->mode = 0;
		else
			dev->mode = 1;
#ifdef AUDIO_DEBUG
		printk("ep7212_audio: set channels %d (%d)\n", val, dev->mode);
#endif
		return 0;

	case DSP_PURGE:
		{
			int bytes = audio_purge(dev);
			put_user(bytes, (int *) arg);
			return 0;
		}
	default:
		/* invalid command */
#ifdef AUDIO_DEBUG
		printk("ep7212_audio: unhandled ioctl 0x%08x\n", command);
#endif
		return -EINVAL;
	}
}

/* Open the audio device */
static int audio_open(struct inode *inode, struct file *file)
{
	audio_dev *dev = &audio[0];

#if AUDIO_DEBUG
	printk(AUDIO_NAME ": audio_open\n");
#endif

	file->private_data = dev;
	/* Flush buffer */
	audio_purge(dev);

	return (0);
}

/* Release the audio device */
static int audio_release(struct inode *inode, struct file *file)
{
#if AUDIO_DEBUG
	printk(AUDIO_NAME ": audio_release\n");
#endif
	return (0);
}

static struct file_operations audio_fops = {
	write:	audio_write,
	ioctl:	audio_ioctl,
	open:	audio_open,
	release:audio_release,
};

static int mixer_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int mixer_release(struct inode *inode, struct file *file)
{
	return 0;
}

static int mixer_ioctl(struct inode *inode, struct file *file, uint command,
		       ulong arg)
{
	return 0;
}

static struct file_operations mixer_fops = {
	ioctl:	mixer_ioctl,
	open:	mixer_open,
	release:mixer_release,
};


static int generic_open_switch(struct inode *inode, struct file *file)
{
	/* select based on minor device number */
	switch (MINOR(inode->i_rdev)) {
	case AUDIO_MINOR:
	case DSP_MINOR:
		file->f_op = &audio_fops;
		break;
	case MIXER_MINOR:
		file->f_op = &mixer_fops;
		break;
	default:
		return (-ENXIO);
	}

	/* invoke specialized open */
	return (file->f_op->open(inode, file));
}

static struct file_operations generic_fops = {
	open: generic_open_switch,	/* switch for real open */
};


int __init audio_7212_init(void)
{
	audio_dev *dev = &audio[0];
	struct pt_regs regs;
	int err;
	int timeout;

	if ((&audio_7212_fiqend - &audio_7212_fiq) > 0x1e4) {
		printk("the FIQ handler size is 0x%x, can't be greater than 0x%x\n",
                       &audio_7212_fiqend - &audio_7212_fiq, 0x1e4);
		return -EINVAL;
	}
#if AUDIO_DEBUG_VERBOSE
	printk(AUDIO_NAME ": audio_7212_init\n");
#endif
	/* Blank everything to start with */
	memset(dev, 0, sizeof (audio_dev));

	init_waitqueue_head(&audio[0].waitq);

	/* Allocate buffers: continuous in virtual memory is fine */
	if ((dev->buffers =
	     vmalloc(sizeof (audio_buf) * AUDIO_NOOF_BUFFERS)) == NULL) {
		printk(KERN_CRIT "audio: can't get memory for buffers");
	}
	/*
	 * We need one buffer which is just zeros, for outputting when
	 * idle. This mirrors the behaviour of the empeg-car display driver
	 */
	if ((dev->zero = vmalloc(sizeof (audio_buf))) == NULL) {
		printk(KERN_CRIT "audio: can't get memory for buffers");
	}

	/* Zero it */
	memset(dev->zero, 0, sizeof (audio_buf));

	/* Set up queue */
	dev->head = dev->tail = dev->used = 0;
	dev->free = AUDIO_NOOF_BUFFERS - 1;
	dev->mode = 1;	/* stereo is the default mode */
	dev->vbase = CLPS7111_VIRT_BASE;

	/* Allocate fiq stack */

	/* Setup callback */
	emit_task.routine = emit_action;
	emit_task.sync = 0;
	emit_task.data = NULL;
	if (request_irq(8, newbuffer, SA_INTERRUPT, "audio", NULL)) {
		printk(KERN_CRIT "Unable to get interrupt 8!\n");
		return -EBUSY;
	}

	/* Install DMA handler */
	regs.ARM_r9 = (int) dev;
	regs.ARM_r10 = CLPS7111_VIRT_BASE + 0x2000;
	regs.ARM_fp = 0;	/* r11 */
	regs.ARM_ip = 0;	/* r12 */
	regs.ARM_sp = 0;
	set_fiq_regs(&regs);
	set_fiq_handler(&audio_7212_fiq, (&audio_7212_fiqend - &audio_7212_fiq));
	if (claim_fiq(&fh)) {
		printk(KERN_CRIT "Couldn't claim FIQ.\n");
		return -1;
	}

	/* program for 64 F's */
#ifdef CONFIG_SOUND_EP7312_EXTCLK
	clps_writel(RATE_44K | I2SF64 | AUDCLKEN | AUDCLKSRC, DAI64FS);
//      clps_writel(RATE_22K | I2SF64 | AUDCLKEN | AUDCLKSRC, DAI64FS);
//      clps_writel(RATE_11K | I2SF64 | AUDCLKEN | AUDCLKSRC, DAI64FS);
#else
{
	int tmp = clps_readl(DAI64FS);
	tmp &= ~(RATE_MASK | 0xc);
	clps_writel(tmp | RATE_32K | 8 | 2 | 1, DAI64FS);
}
#endif
	/* Enable DAI interface */
	clps_writel((clps_readl(SYSCON3) | SYSCON3_DAISEL) & ~SYSCON3_DAIEN,
		    SYSCON3);

	/* Enable I2S output: select external clock input (11.2896Mhz) */
	clps_writel((0x0404 | DAIR_ECS | DAIR_RCTM), DAIR);

	/* Clear error bits */
	clps_writel(0xFFFFFFFF, DAISR);

	/* Enable DAI */
	clps_writel((clps_readl(DAIR) | DAIR_DAIEN), DAIR);
	clps_writel(0x002b0404, DAIR);
	/* Enable FIFOs */
	timeout = jiffies + HZ;
	while (!(clps_readl(DAISR) & DAISR_FIFO) && (jiffies < timeout));
	clps_writel(0x000d8000, DAIDR2);
	while (!(clps_readl(DAISR) & DAISR_FIFO) && (jiffies < timeout));
	clps_writel(0x00118000, DAIDR2);
	while (!(clps_readl(DAISR) & DAISR_FIFO) && (jiffies < timeout));
	/* If this timed out, then there is a problem with the serial clock */
	if (jiffies >= timeout) {
		printk("Failed to initialise audio_7212: is SSI_CLK active?\n");
	}
	/* Unmask FIQ */
	clps_writel((clps_readl(INTMR3) | (1 << 0)), INTMR3);
	/* Register device */
	if ((err =
	     register_chrdev(AUDIO_MAJOR, AUDIO_NAME, &generic_fops)) != 0) {
		/* Log error and fail */
		printk(AUDIO_NAME ": unable to register major device %d\n",
		       AUDIO_MAJOR);
		return (err);
	}
	
	setup_cs43l42();
	
#ifdef CONFIG_PROC_FS
	/* Register procfs devices */
	proc_audio = create_proc_entry("audio", 0, 0);
	if (proc_audio)
		proc_audio->read_proc = audio_read_proc;
#endif				/* CONFIG_PROC_FS */
	/* Log device registration */
	printk(AUDIO_NAME_VERBOSE " initialized\n");
	/* Everything OK */
	return (0);
}


void __exit audio_7212_exit(void)
{
	audio_dev *dev = &audio[0];
	
	/*
	 * first unmask DAIINT FIQ to avoid spontaneous crashes
	 */
	clps_writel((clps_readl(INTMR3) & ~1), INTMR3);
	release_fiq(&fh);
	free_irq(8, NULL);
	vfree(dev->buffers);
	vfree(dev->zero);
	unregister_chrdev(AUDIO_MAJOR, AUDIO_NAME);
}


module_init(audio_7212_init);
module_exit(audio_7212_exit);
