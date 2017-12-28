/*
 * adapter.c
 *
 * Xilinx HWICAP interface to Linux
 *
 * Author: John Williams <jwilliams@itee.uq.edu.au>
 *
 * 2004-2006 (c) John Williams.  This file is licensed under the 
 * terms of the GNU General Public License version 2.1.  This program is 
 * licensed "as is" without any warranty of any kind, whether express 
 * or implied.
 */

/*
 * This driver is a wrapper around the Xilinx level-0 drivers, and opb_hwicap
 * core.  It creates a Linux misc device, minor number 200, that should
 * be given a dev node something like /dev/icap
 *
 * It's simple, cat a partial bitstream to /dev/icap, and voila!
 *
 * Of course, since you are reconfiguring the same FPGA that contains the CPU, 
 * and busses, and operating system, and in fact everything, the potential for
 * catastrophic failure is high.  
 * 
 * With sufficient bad luck, determination or aliean technology you may even
 * damange the FPGA with it.  If so, please tell me, so we can have a good
 * over it some day.  But, don't call your lawyer, or my lawyer - it's not 
 * my fault.
 *
 * Finally, if you publish anything that builds on, or makes use of this driver,
 * or if you just feel like being generous with citations, please reference
 * the original paper where I described the idea:
 *
 * Williams, John A. and Bergmann, Neil W. (2004) "Embedded Linux as a Platform
 * for Dynamically Self-Reconfiguring Systems-On-Chip". In  Engineering of 
 * Reconfigurable Systems and Algorithms (ERSA 2004), 21-24 June, 2004, 
 * Las Vegas, Nevada, USA.
 *
 * You might also like to read it, to get a better idea of what I was thinking.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>

#include <linux/autoconf.h>

#include "xbasic_types.h"
#include "xhwicap.h"
#include "xhwicap_i.h"
#include <asm/xhwicap_ioctl.h>

MODULE_AUTHOR("John Williams <jwilliams@itee.uq.edu.au");
MODULE_DESCRIPTION("Xilinx HWICAP driver");
MODULE_LICENSE("GPL");

#if 0
#define DBPRINTK(...) printk(__VA_ARGS__)
#else
#define DBPRINTK(...)
#endif

#define ASSERT(x) { if(!(x)) printk("Assertion failed: ## x ## :%s:%s:%i\n",__FUNCTION__,__FILE__,__LINE__);}

/* Our private per interface data. */
struct xhwicap_instance {
	struct xhwicap_instance *next_inst;	/* The next instance in inst_list */
	int index;		/* Which interface is this */
	u32 save_BaseAddress;	/* Saved physical base address */
	XHwIcap HwIcap;
	unsigned int pendingWord;
	unsigned pendingByteCount;
};
/* List of instances we're handling. */
static struct xhwicap_instance *inst_list = NULL;

static int
xhwicap_open(struct inode *inode, struct file *file)
{
        struct xhwicap_instance *inst;

	MOD_INC_USE_COUNT;

	/* FIXME assume head of list is the only instance */
        inst = inst_list;

	inst->pendingWord=0;
	inst->pendingByteCount=0;
	
	return 0;
}

static int
xhwicap_release(struct inode *inode, struct file *file)
{
	MOD_DEC_USE_COUNT;

	return 0;
}

#if 0
static xhwicap_instance *
ioctl_getinstance(unsigned long arg,
	void **userdata, 
	struct xhwicap_instance **match)
{
        struct xhwicap_instance *inst;

        if (copy_from_user(ioctl_data, (void *) arg, sizeof (*ioctl_data)))
                return -EFAULT;

        inst = inst_list;
        while (inst && inst->index != ioctl_data->device)
                inst = inst->next_inst;

        *match = inst;
        return inst ? 0 : -ENODEV;
}
#endif


static int
xhwicap_ioctl(struct inode *inode, struct file *file,
	    unsigned int cmd, unsigned long arg)
{
	struct xhwicap_instance *inst;
	XStatus status;

	/* FIXME! Assume instance is first in list - this is bogus */
        inst = inst_list;
	if(!inst)
		return -ENODEV;

		switch (cmd) {
			case XHWICAP_IOCCMDDESYNC:
				DBPRINTK("desynch()\n");
				status = XHwIcap_CommandDesync(&(inst->HwIcap));
				if(status==XST_DEVICE_BUSY)
					return -EBUSY;
				break;

			case XHWICAP_IOCCMDCAPTURE:
				DBPRINTK("capture()\n");
				status = XHwIcap_CommandCapture(&(inst->HwIcap));
				if(status==XST_DEVICE_BUSY)
					return -EBUSY;
				break;
		
			case XHWICAP_IOCGETINST:
				DBPRINTK("getinst()\n");
				copy_to_user((void *)arg, (void *)(&(inst->HwIcap)), sizeof(XHwIcap));

		default:
			return -ENOIOCTLCMD;

	}
	return 0;
}

/* Method to read data from hwicap device and copy into user buffer */
/* Do the read in fixed size chunks */

static ssize_t 
xhwicap_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
        struct xhwicap_instance *inst;
	size_t read_count=count;
	XStatus status;
	int wordCount=0;
	char *user_ptr=buf;

	DBPRINTK("xhwicap_read\n");

	/* FIXME! Assume instance is first in list - this is bogus */
        inst = inst_list;
	if(!inst)
		return -ENODEV;

	DBPRINTK("reading %i words from device\n",(count+3)>>2);

	while(read_count)
	{
		int chunk_word_count;
		size_t chunk_size, chunk_left;

		/* Work out how big this chunk should be */
		chunk_size=min(read_count, (size_t)XHI_MAX_BUFFER_BYTES);
		chunk_word_count=0;

		DBPRINTK("chunk size %lX\n",chunk_size);

		/* Read the chunk from the device */
		status = XHwIcap_DeviceRead(&(inst->HwIcap), 0, (chunk_size+3)>>2);
		/* Check return status */
		if(status==XST_BUFFER_TOO_SMALL)
			return -ENOSPC;
		else if(status==XST_DEVICE_BUSY)
			return -EBUSY;
		else if(status==XST_INVALID_PARAM)
			return -EINVAL;

		DBPRINTK("read chunk from device\n");

		/* Copy this chunk from private buffer into user space */
		chunk_left = chunk_size;

		/* Copy from device private buffer into user buffer */
		DBPRINTK("copying chunk to user buffer\n");
		while(chunk_left)
		{
			size_t bytesToWrite=min((size_t)4, chunk_left);
			u32 tmp=XHwIcap_StorageBufferRead(&(inst->HwIcap),chunk_word_count);

			copy_to_user(user_ptr,&tmp, bytesToWrite);
			user_ptr += bytesToWrite;
			chunk_left -=bytesToWrite;
			chunk_word_count++;
		}
		read_count-=chunk_size;
	}
	DBPRINTK("done\n");

	return count-read_count;
}

static ssize_t 
xhwicap_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
        struct xhwicap_instance *inst;
	size_t write_count=count;
	XStatus status;
	const char *user_buf=buf;

	DBPRINTK("write\n");
	DBPRINTK("count is %lX\n",count);

	/* FIXME! Assume instance is first in list - this is bogus */
        inst = inst_list;
	if(!inst)
		return -ENODEV;

	/* Deal with any left overs from last time */
	DBPRINTK("PREAMBLE: %i bytes pending\n",inst->pendingByteCount);

	ASSERT(inst->pendingByteCount<4);

	/* Only enter the preamble if there is stuff left from last time */
	while(inst->pendingByteCount && inst->pendingByteCount<4 
		&& write_count)
	{
		unsigned char tmp;
		/* Accumulate into the pendingWord */
		copy_from_user(&tmp, user_buf++, 1);
		inst->pendingWord=(inst->pendingWord) << 8 | (tmp & 0xFF);
		inst->pendingByteCount++;
		write_count--;
	}

	/* Did we accumulated a full word? */
	if(inst->pendingByteCount==4)
	{
		DBPRINTK("Writing pendingword\n");
		/* Write the word to the buffer */
		XHwIcap_StorageBufferWrite(&(inst->HwIcap),0,inst->pendingWord);
		XHwIcap_DeviceWrite(&(inst->HwIcap), 0, 1);
		inst->pendingByteCount=0;
		inst->pendingWord=0;
	}

	ASSERT(write_count==0 || inst->pendingByteCount==0);

	/* Minimum possible chunk size is 4 bytes */
	while(write_count>3)
	{
		int chunk_word_count=0;
		size_t chunk_size, chunk_left;

		/* don't use full limit of BUFFER_BYTES, seems to
		   break the hardware.. 
		   Round down to nearest multiple of 4 */
		chunk_size=chunk_left=min(write_count, 
					(size_t)XHI_MAX_BUFFER_BYTES-8) & ~0x3;

		DBPRINTK("chunk size %lX\n",chunk_size);
		DBPRINTK("copying chunk to buffer\n");

		while(chunk_left)
		{
			u32 tmp;
			copy_from_user(&tmp, user_buf, 4);
			XHwIcap_StorageBufferWrite(&(inst->HwIcap),
							chunk_word_count++,
							tmp);
			user_buf+=4;
			chunk_left-=4;
		}

		ASSERT(chunk_word_count);

		DBPRINTK("done\nWriting chunk to device..");

		/* initiate write of private buffer to device */
		DBPRINTK("chunk_word_count:%X\n",chunk_word_count);

		status = XHwIcap_DeviceWrite(&(inst->HwIcap), 
						0, chunk_word_count);

		if(status==XST_BUFFER_TOO_SMALL)
			return -ENOSPC;
		else if(status==XST_DEVICE_BUSY)
			return -EBUSY;
		else if(status==XST_INVALID_PARAM)
			return -EINVAL;

		write_count-=chunk_word_count*4;
	}


	DBPRINTK("POSTAMBLE: %i bytes remaining\n",write_count);

	ASSERT(write_count<4);  /* No more than 3 bytes remaining */
	/*ASSERT(inst->pendingByteCount==0);   *//* pending buffer is empty */

	/* Put remaining bytes into the pending buffer */
	while(write_count)
	{
		unsigned char tmp;
		copy_from_user(&tmp, user_buf++, 1);
		inst->pendingWord=(inst->pendingWord) << 8 | (tmp & 0xFF);
		write_count--;
		inst->pendingByteCount++;
	}

	DBPRINTK("finished\n");
	return count-write_count;
}


static void
remove_head_inst(void)
{
	struct xhwicap_instance *inst;
	XHwIcap_Config *cfg;

	/* Pull the head off of inst_list. */
	inst = inst_list;
	inst_list = inst->next_inst;

	cfg = XHwIcap_LookupConfig(inst->index);
	iounmap((void *) cfg->BaseAddress);
	cfg->BaseAddress = inst->save_BaseAddress;
}

static struct file_operations xfops = {
	owner:THIS_MODULE,
	ioctl:xhwicap_ioctl,
	open:xhwicap_open,
	release:xhwicap_release,
	read:xhwicap_read,
	write:xhwicap_write
};
/*
 * We get to the HWICAP through one minor number.  Here's the
 * miscdevice that gets registered for that minor number.
 */
#define HWICAP_MINOR 200

static struct miscdevice miscdev = {
	minor:HWICAP_MINOR,
	name:"xhwicap",
	fops:&xfops
};

static int __init
probe(int index)
{
	static const unsigned long remap_size
	    = CONFIG_XILINX_HWICAP_0_HIGHADDR - CONFIG_XILINX_HWICAP_0_BASEADDR + 1;
	struct xhwicap_instance *inst;
	XHwIcap_Config *cfg;

	/* Find the config for our instance. */
	cfg = XHwIcap_LookupConfig(index);
	if (!cfg)
		return -ENODEV;

	/* Allocate the inst and zero it out. */
	inst = (struct xhwicap_instance *) kmalloc(sizeof (struct xhwicap_instance),
						 GFP_KERNEL);
	if (!inst) {
		printk(KERN_ERR "%s #%d: Could not allocate instance.\n",
		       miscdev.name, index);
		return -ENOMEM;
	}
	memset(inst, 0, sizeof (struct xhwicap_instance));
	inst->index = index;

	/* Make it the head of inst_list. */
	inst->next_inst = inst_list;
	inst_list = inst;

	/* Change the addresses to be virtual; save the old ones to restore. */
	inst->save_BaseAddress = cfg->BaseAddress;
	cfg->BaseAddress = (u32) ioremap(inst->save_BaseAddress, remap_size);

	/* Tell the Xilinx code to bring this HWICAP interface up. */
	/* REALLY FIXME - XC2V1000 is hard coded - bleugh! */
	if (XHwIcap_Initialize(&inst->HwIcap, cfg->DeviceId, 
				XHI_XC2V1000) != XST_SUCCESS) {
		printk(KERN_ERR "%s #%d: Could not initialize instance.\n",
		       miscdev.name, inst->index);
		remove_head_inst();
		return -ENODEV;
	}

	printk(KERN_INFO "%s #%d at 0x%08X mapped to 0x%08X\n",
	       miscdev.name, inst->index,
	       inst->save_BaseAddress, cfg->BaseAddress);

	return 0;
}

static int __init
xhwicap_init(void)
{
	int rtn, index = 0;

	printk("probing...\n");
	while (probe(index++) == 0) ;

	if (index > 1) {
		/* We found at least one instance. */

		/* Register the driver with misc and report success. */
		rtn = misc_register(&miscdev);
		if (rtn) {
			printk(KERN_ERR "%s: Could not register driver.\n",
			       miscdev.name);
			while (inst_list)
				remove_head_inst();
			return rtn;
		}

		/* Report success. */
		printk("Xilinx HWICAP registered\n");
		return 0;
	} else {
		/* No instances found. */
		printk("Probe failed\n");
		return -ENODEV;
	}
}

static void __exit
xhwicap_cleanup(void)
{
	while (inst_list)
		remove_head_inst();

	misc_deregister(&miscdev);
}

EXPORT_NO_SYMBOLS;

module_init(xhwicap_init);
module_exit(xhwicap_cleanup);
