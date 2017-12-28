/*
 * fslfifo.c -- FSL FIFO driver for Microblaze
 *
 * Copyright (C) 2004 John Williams <jwilliams@itee.uq.edu.au>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *       
 */

/*
 * Simple driver to provide Linux-style FIFO interface to custom hardware
 * peripherals connected to Microblaze FSL ports.  See Microblaze user manual
 * for details of FSL architecture.
 *
 * read() and write() operate on software buffers, with a tasklet scheduled
 * to actually stuff data into / out of FSL ports.  This might be better 
 * served by interrupts, however the hardware doesn't support it yet.
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#include <asm/fsl.h>
#include <asm/fslfifo_ioctl.h>

MODULE_AUTHOR("John Williams <jwilliams@itee.uq.edu.au>");
MODULE_LICENSE("GPL");

#if 0
#define DBPRINTK(...) printk(__VA_ARGS__)
#else
#define DBPRINTK(...)
#endif

#ifndef MIN
#define MIN(a,b)	((a)<(b) ? (a) : (b))
#endif

#include "fslfifo.h"

DECLARE_WAIT_QUEUE_HEAD(fsl_read_queue);
DECLARE_WAIT_QUEUE_HEAD(fsl_write_queue);

/* Declarations of the various files ops */
static loff_t fsl_llseek(struct file *f, loff_t off, int whence);
static ssize_t fsl_read(struct file *f, char *buf, size_t count, loff_t *pos);
static ssize_t fsl_write(struct file *f, const char *buf, size_t count, loff_t *pos);
static int fsl_ioctl(struct inode *inode, struct file *f,
			unsigned int cmd, unsigned long arg);
static int fsl_open(struct inode *inode, struct file *f);
static int fsl_release(struct inode *inode, struct file *f);

void write_tasklet_func(unsigned long data);
void read_tasklet_func(unsigned long data);
	
static struct file_operations fsl_fops = {
	llseek:		fsl_llseek,
	read:		fsl_read,
	write:		fsl_write,
	ioctl:		fsl_ioctl,
	open:		fsl_open,
	release:	fsl_release};

/* Statically allocate descriptors - this is klunky but too bad */
static struct miscdevice fsl_miscdev[MAX_FSLFIFO_COUNT];
static struct fsl_fifo_t fsl_fifo_table[MAX_FSLFIFO_COUNT];

/* The tasklet data is the address of the first fsl_fifo in the array
   We do this so a single tasklet can process all fsl fifos
   hopefully saves us some time in doing multiple tasklet executions
*/
DECLARE_TASKLET(fsl_write_tasklet, write_tasklet_func, 
			(unsigned long)fsl_fifo_table);
DECLARE_TASKLET(fsl_read_tasklet, read_tasklet_func, 
			(unsigned long)fsl_fifo_table);

/* The fsl fifo writing tasklet.  It loops through *all* FIFOs
   see if any are waiting to write, and does so until it blocks.
   if *any* FIFO blocks on writing, the tasklet is rescheduled.

   We could alternatively have one tasklet per FIFO , not sure which would
   be more efficient... */
void write_tasklet_func(unsigned long data)
{
	struct fsl_fifo_t *fsl_fifo=(struct fsl_fifo_t *)data;
	int id;
	int do_reschedule=0;

	/* Loop through each fsl fifo*/
	for(id=0;id<MAX_FSLFIFO_COUNT;id++, fsl_fifo++)
	{
		unsigned flags;

		if(!fsl_fifo->exists)
			continue;

		save_flags_cli(flags);

		/* There must be at least "width" bytes in the buffer */
		if(fsl_fifo->tx_cnt < fsl_fifo->width)
		{
			restore_flags(flags);
			continue;
		}

		while(fsl_fifo->tx_cnt >= fsl_fifo->width)
		{
			unsigned fsl_status;
			unsigned int value;
			void *ptr = &(fsl_fifo->tx_buf[fsl_fifo->tx_tail]);

			switch(fsl_fifo->width)
			{
			case 1: value = (u32) *((u8 *)ptr);
				break;
			case 2: value = (u32) *((u16 *)ptr);
				break;
			case 4: value = (u32) *((u32 *)ptr);
				break;
			default:
				;
				/* errors in a tasklet?  yuck! */
			}

			fsl_nput(fsl_fifo->id, value, fsl_status);

			if(fsl_nodata(fsl_status))
			{
				do_reschedule++;
				break;
			}
			else
			{
				/* Update the buffer data structure */
				fsl_fifo->tx_cnt-=fsl_fifo->width;
				fsl_fifo->tx_tail+= fsl_fifo->width;
				fsl_fifo->tx_tail &= FSLFIFO_BUF_SIZE-1;
			}
		}
		restore_flags(flags);
	}

	if(do_reschedule)
		tasklet_schedule(&fsl_write_tasklet);

	/* Wake any processes blocked on FIFO write() operations */
	wake_up(&fsl_write_queue);
}


void read_tasklet_func(unsigned long data)
{
	struct fsl_fifo_t *fsl_fifo=(struct fsl_fifo_t *)data;
	int id;
	int do_reschedule=0;

	/* Loop through each fsl fifo*/
	for(id=0;id<MAX_FSLFIFO_COUNT;id++, fsl_fifo++)
	{
		unsigned flags;

		if(!fsl_fifo->exists)
			continue;
	
		/* If rx_buf is full, don't even try */
		save_flags_cli(flags);
		if((fsl_fifo->rx_cnt + fsl_fifo->width) >= FSLFIFO_BUF_SIZE)
		{
			restore_flags(flags);
			continue;
		}
		restore_flags(flags);

		while(1)
		{
			unsigned val;
			unsigned fsl_status;

			void *ptr=&(fsl_fifo->rx_buf[fsl_fifo->rx_head]);

			fsl_nget(fsl_fifo->id, val, fsl_status);
			if(fsl_nodata(fsl_status))
			{
				break;
			}

			save_flags_cli(flags);

			switch(fsl_fifo->width) {
			case 1:
				*((u8*)ptr) = val & 0xFF;
				break;
			case 2:
				*((u16*)ptr) = val & 0xFFFF;
				break;
			case 4:
				*((u32*)ptr) = val;
				break;
			default:
				;
				/* bleugh, error in tasklet */
			}
			
			fsl_fifo->rx_head += fsl_fifo->width;
			fsl_fifo->rx_head &= FSLFIFO_BUF_SIZE-1;
			fsl_fifo->rx_cnt += fsl_fifo->width;
			if(fsl_fifo->rx_cnt<FSLFIFO_BUF_SIZE)
			{
				restore_flags(flags);
				do_reschedule++;
				break;
			}
			restore_flags(flags);
		}
	}

	if(do_reschedule)
	{
		tasklet_schedule(&fsl_read_tasklet);
	}

	wake_up(&fsl_read_queue);
}


/* Seek doesn't make a lot of sense! */
static loff_t fsl_llseek(struct file *f, loff_t off, int whence)
{
	return 0;
}

/* Put data into the tx_buffer and schedule write_tasklet */
static ssize_t fsl_write(struct file *f, const char *buf, 
				size_t count, loff_t *pos)
{
	struct fsl_fifo_t *fsl_fifo;
	int do_sched_tasklet=0;
	unsigned flags;
	int total=0;

	fsl_fifo = (struct fsl_fifo_t *)f->private_data;
	if(!fsl_fifo->exists)
		return -ENODEV;

	save_flags_cli(flags);

	/* Is the tx buffer full, and we block */
	while(fsl_fifo->tx_cnt==FSLFIFO_BUF_SIZE)
	{
		restore_flags(flags);
		tasklet_schedule(&fsl_write_tasklet);
		interruptible_sleep_on(&fsl_write_queue);
		if(current->sigpending)
			return -EINTR;
		save_flags_cli(flags);
	}

	/* The following logic stolen from mcfserial.c */
	save_flags(flags);
	while(1)
	{
		int c;

		cli();

		/* How much can we write into the buffer? */
		c=MIN(count, MIN(FSLFIFO_BUF_SIZE - fsl_fifo->tx_cnt-1,
				 FSLFIFO_BUF_SIZE - fsl_fifo->tx_head));
		if(c<=0)
		{
			restore_flags(flags);
			break;
		}

		copy_from_user(fsl_fifo->tx_buf+fsl_fifo->tx_head, 
				buf, c);

		fsl_fifo->tx_head=(fsl_fifo->tx_head+c) & (FSLFIFO_BUF_SIZE-1);
		fsl_fifo->tx_cnt += c;

		restore_flags(flags);

		do_sched_tasklet++;

		buf+=c;
		count-=c;
		total+=c;
	}

	if(do_sched_tasklet)
		tasklet_schedule(&fsl_write_tasklet);

	return total;
}

static ssize_t fsl_read(struct file *f, char *buf, 
				size_t count, loff_t *pos)
{
	struct fsl_fifo_t *fsl_fifo=(struct fsl_fifo_t *)f->private_data;
	unsigned flags;
	int total=0;

	if(!fsl_fifo->exists)
		return -ENODEV;

	save_flags_cli(flags);

	/* Block until there's some data in the buffer */
	while(!fsl_fifo->rx_cnt)
	{
		restore_flags(flags);
		tasklet_schedule(&fsl_read_tasklet);
		interruptible_sleep_on(&fsl_read_queue);
		if(current->sigpending)
		{
			return -EINTR;
		}
		save_flags_cli(flags);
	}
		
	while(1)
	{
		int c; 

		cli();
		c = MIN(count, MIN(fsl_fifo->rx_cnt,
				   FSLFIFO_BUF_SIZE - fsl_fifo->rx_tail));

		if(c<=0)
		{
			restore_flags(flags);
			break;
		}

		copy_to_user(buf, fsl_fifo->rx_buf+fsl_fifo->rx_tail,c);

		/* Update the buffer data structure */
		fsl_fifo->rx_cnt-=c;
		fsl_fifo->rx_tail+=c;
		fsl_fifo->rx_tail &= FSLFIFO_BUF_SIZE-1;

		restore_flags(flags);

		buf += c;
		count -= c;
		total += c;
	}

	if(count)
		tasklet_schedule(&fsl_read_tasklet);

	return total;
}

int fsl_flush_buffers(int id)
{
	unsigned flags;

	struct fsl_fifo_t *fsl_fifo=&fsl_fifo_table[id];

	if(!fsl_fifo->exists)
		return 0;

	save_flags_cli(flags);
	fsl_fifo->rx_cnt=fsl_fifo->rx_head=fsl_fifo->rx_tail=0;
	fsl_fifo->tx_cnt=fsl_fifo->tx_head=fsl_fifo->tx_tail=0;
	restore_flags(flags);

	return 0;
}

static int fsl_ioctl(struct inode *inode, struct file *f,
			unsigned int cmd, unsigned long arg)
{
	int fsl_id;
	struct fsl_fifo_t *fsl_fifo;
	unsigned flags, fsl_status;

	fsl_id=FSLFIFO_ID(inode->i_rdev);

	if(fsl_id >= MAX_FSLFIFO_COUNT)
		return -ENODEV;

	fsl_fifo=&fsl_fifo_table[fsl_id];

	if(!fsl_fifo->exists)
		return -ENODEV;

	switch(cmd)
	{
	case FSLFIFO_IOCRESET:		/* Reset */
		save_flags_cli(flags);
		fsl_flush_buffers(fsl_id);
		restore_flags(flags);
		return 0;

	case FSLFIFO_IOCTCONTROL:	/* Write control value */
	/* Note this jumps the queue, and is blatted directly to the FSL
	   port.  It does not get queued in the main SW buffer */
		save_flags_cli(flags);
		fsl_ncput(fsl_id, arg, fsl_status);
		restore_flags(flags);
		if(fsl_error(fsl_status))
			return -EIO;
		else if(fsl_nodata(fsl_status))
			return -EBUSY;
		return 0;

	case FSLFIFO_IOCQCONTROL:	/* Read control value */
	/* This bypasses the normal software buffers.  It is very unlikely
	   to work unless those buffers are empty, and the tasklets are
 	   idling */
		save_flags_cli(flags);
		fsl_ncget(fsl_id, arg, fsl_status);
		restore_flags(flags);
		if(fsl_error(fsl_status))	/* Non-control value from FSL */
			return -EIO;	
		else if(fsl_nodata(fsl_status))	/* Nothing from FSL */
			return -EBUSY;	
		return arg;
		
	case FSLFIFO_IOCTWIDTH:		/* set data width */
		if(arg==1 || arg==2 || arg==4)
		{
			save_flags_cli(flags);
			fsl_flush_buffers(fsl_id);
			fsl_fifo->width=arg;
			return 0;
		}
		else
			return -EINVAL;

	case FSLFIFO_IOCQWIDTH:		/* get data width */
		return fsl_fifo->width;

	default:			/* bleugh */
		return -EINVAL;
	}

	return 0;
}

static int fsl_open(struct inode *inode, struct file *f)
{
	int fsl_id;
	struct fsl_fifo_t *fsl_fifo;

	fsl_id=FSLFIFO_ID(inode->i_rdev);

	if(fsl_id >= MAX_FSLFIFO_COUNT)
		return -ENODEV;

	fsl_fifo=&fsl_fifo_table[fsl_id];

	if(!fsl_fifo->exists)
		return -ENODEV;

	MOD_INC_USE_COUNT;
	
	/* Set the file's private data to be the fsl_fifo descriptor*/
	if(!f->private_data)
		f->private_data=(void *)fsl_fifo;

	return 0;
}

static int fsl_release(struct inode *inode, struct file *f)
{
	MOD_DEC_USE_COUNT;
	return 0;
}

static int __init init_fsl_fifo_devices(void)
{
	int i;
	int rtn;

/* This is horrible.  In fact, the whole static table thing is horrible. */
#ifdef CONFIG_MICROBLAZE_FSLFIFO0
	fsl_fifo_table[0].exists=1;
#endif
#ifdef CONFIG_MICROBLAZE_FSLFIFO1
	fsl_fifo_table[1].exists=1;
#endif
#ifdef CONFIG_MICROBLAZE_FSLFIFO2
	fsl_fifo_table[2].exists=1;
#endif
#ifdef CONFIG_MICROBLAZE_FSLFIFO3
	fsl_fifo_table[3].exists=1;
#endif
#ifdef CONFIG_MICROBLAZE_FSLFIFO4
	fsl_fifo_table[4].exists=1;
#endif
#ifdef CONFIG_MICROBLAZE_FSLFIFO5
	fsl_fifo_table[5].exists=1;
#endif
#ifdef CONFIG_MICROBLAZE_FSLFIFO6
	fsl_fifo_table[6].exists=1;
#endif
#ifdef CONFIG_MICROBLAZE_FSLFIFO7
	fsl_fifo_table[7].exists=1;
#endif

	printk("Microblaze FSL FIFO Driver. ");
	printk("John Williams <jwilliams@itee.uq.edu.au>\n");
	for(i=0;i<MAX_FSLFIFO_COUNT;i++)
	{
		struct fsl_fifo_t *fsl_fifo=&fsl_fifo_table[i];
		if(!fsl_fifo->exists)
			continue;

		fsl_miscdev[i].minor=FSLFIFO_MINOR(i);
		fsl_miscdev[i].name="data";
		fsl_miscdev[i].fops=&fsl_fops;

		if((rtn=misc_register(&fsl_miscdev[i])))
		{
			printk("Error registering fslfifo[%i]\n",i);
			continue;
			/* return rtn; */
		}

		fsl_fifo->id=i;
		fsl_fifo->width=4;	/* Assume word-wide FSL channel */
		fsl_fifo->tx_cnt=0;
		fsl_fifo->tx_head=0;
		fsl_fifo->tx_tail=0;
	
		fsl_fifo->rx_cnt=0;
		fsl_fifo->rx_head=0;
		fsl_fifo->rx_tail=0;
	
		printk(KERN_INFO "FSL FIFO[%i] initialised\n",i);
	}

	return 0;
}

static int __init fsl_fifo_init(void)
{
	int rtn = init_fsl_fifo_devices();

	if(!rtn)
		return 0;
	else
	{
		printk(KERN_INFO "Error registering PicoFIFO devices\n");
		return -ENODEV;
	}
}

static void __exit fsl_fifo_cleanup(void)
{
	int i;

	for(i=0;i<MAX_FSLFIFO_COUNT;i++)
		if(fsl_fifo_table[i].exists)
			misc_deregister(&fsl_miscdev[i]);
}

EXPORT_NO_SYMBOLS;

module_init(fsl_fifo_init);
module_exit(fsl_fifo_cleanup);

