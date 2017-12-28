/*
 * i2c-algo-xilinx.c
 *
 * Xilinx IIC Adapter component to interface IIC component to Linux
 *
 * Author: MontaVista Software, Inc.
 *         source@mvista.com
 *
 * 2002 (c) MontaVista, Software, Inc.  This file is licensed under the terms
 * of the GNU General Public License version 2.  This program is licensed
 * "as is" without any warranty of any kind, whether express or implied.
 */

/*
 * I2C drivers are split into two pieces: the adapter and the algorithm.
 * The adapter is responsible for actually manipulating the hardware and
 * the algorithm is the layer above that that handles the higher level
 * tasks such as transmitting or receiving a buffer.  The best example
 * (in my opinion) of this is the bit banging algorithm has a number of
 * different adapters that can plug in under it to actually wiggle the
 * SDA and SCL.
 *
 * The interesting part is that the drivers Xilinx provides with their
 * IP are also split into two pieces where one part is the OS
 * independent code and the other part is the OS dependent code.  All of
 * the other sources in this directory are the OS independent files as
 * provided by Xilinx with no changes made to them.
 *
 * As it turns out, this maps quite well into the I2C driver philosophy.
 * This file is the I2C algorithm that communicates with the Xilinx OS
 * independent function that will serve as our I2C adapter.  The
 * unfortunate part is that the term "adapter" is overloaded in our
 * context.  Xilinx refers to the OS dependent part of a driver as an
 * adapter.  So from an I2C driver perspective, this file is not an
 * adapter; that role is filled by the Xilinx OS independent files.
 * From a Xilinx perspective, this file is an adapter; it adapts their
 * OS independent code to Linux.
 *
 * Another thing to consider is that the Xilinx OS dependent code knows
 * nothing about Linux I2C adapters, so even though this file is billed
 * as the I2C algorithm, it takes care of the i2c_adapter structure.
 *
 * Fortunately, naming conventions will give you a clue as to what comes
 * from where.  Functions beginning with XIic_ are provided by the
 * Xilinx OS independent files.  Functions beginning with i2c_ are
 * provided by the I2C Linux core.  All functions in this file that are
 * called by Linux have names that begin with xiic_.  The functions in
 * this file that have Handler in their name are registered as callbacks
 * with the underlying Xilinx OS independent layer.  Any other functions
 * are static helper functions.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <asm/io.h>
#include <asm/irq.h>

#include <linux/i2c.h>

#include <xbasic_types.h>
#include "xiic.h"
#include "xiic_i.h"

MODULE_AUTHOR("MontaVista Software, Inc. <source@mvista.com>");
MODULE_DESCRIPTION("Xilinx IIC driver");
MODULE_LICENSE("GPL");
MODULE_PARM(scan, "i");
MODULE_PARM_DESC(scan, "Scan for active chips on the bus");
static int scan = 0;		/* have a look at what's hanging 'round */

/* SAATODO: actually use these? */
#define XIIC_TIMEOUT		100
#define XIIC_RETRY		3

/* Our private per device data. */
struct xiic_dev {
	struct i2c_adapter adap;	/* The Linux I2C core data  */
	struct xiic_dev *next_dev;	/* The next device in dev_list */
	struct completion complete;	/* for waiting for interrupts */
	int index;		/* Which interface is this */
	u32 save_BaseAddress;	/* Saved physical base address */
	unsigned int irq;	/* device IRQ number    */
	/*
	 * The underlying OS independent code needs space as well.  A
	 * pointer to the following XIic structure will be passed to
	 * any XIic_ function that requires it.  However, we treat the
	 * data as an opaque object in this file (meaning that we never
	 * reference any of the fields inside of the structure).
	 */
	XIic Iic;
	XStatus interrupt_status;
	/*
	 * The following bit fields are used to keep track of what
	 * all has been done to initialize the xiic_dev to make
	 * error handling out of probe() easier.
	 */
	unsigned int reqirq:1;	/* Has request_irq() been called? */
	unsigned int remapped:1;	/* Has ioremap() been called? */
	unsigned int started:1;	/* Has XIic_Start() been called? */
	unsigned int added:1;	/* Has i2c_add_adapter() been called? */
};

/* List of devices we're handling and a lock to give us atomic access. */
static struct xiic_dev *dev_list = NULL;
static spinlock_t dev_lock = SPIN_LOCK_UNLOCKED;

/* SAATODO: This function will be moved into the Xilinx code. */
/*****************************************************************************/
/**
*
* Lookup the device configuration based on the iic instance.  The table
* XIic_ConfigTable contains the configuration info for each device in the system.
*
* @param Instance is the index of the interface being looked up.
*
* @return
*
* A pointer to the configuration table entry corresponding to the given
* device ID, or NULL if no match is found.
*
* @note
*
* None.
*
******************************************************************************/
XIic_Config *XIic_GetConfig(int Instance)
{
	if (Instance < 0 || Instance >= CONFIG_XILINX_IIC_NUM_INSTANCES)
	{
		return NULL;
	}

	return &XIic_ConfigTable[Instance];
}

static int
xiic_xfer(struct i2c_adapter *i2c_adap, struct i2c_msg msgs[], int num)
{
	struct xiic_dev *dev = (struct xiic_dev *) i2c_adap;
	struct i2c_msg *pmsg;
	u32 options;
	int i, retries;
	XStatus Status;

	for (i = 0; i < num; i++) {
		pmsg = &msgs[i];

		if (!pmsg->len)	/* If length is zero */
		     continue;	/* on to the next request. */

		options = 0;
		if (pmsg->flags & I2C_M_TEN)
			options |= XII_SEND_10_BIT_OPTION;
		if (i != num-1)
			options |= XII_REPEATED_START_OPTION;
		XIic_SetOptions(&dev->Iic, options);

		if (XIic_SetAddress(&dev->Iic, XII_ADDR_TO_SEND_TYPE,
				    pmsg->addr) != XST_SUCCESS) {
			printk(KERN_WARNING
			       "%s #%d: Could not set address to 0x%2x.\n",
			       dev->adap.name, dev->index, pmsg->addr);
			return -EIO;
		}
		dev->interrupt_status = ~(XStatus) 0;
		/*
		 * The Xilinx layer does not handle bus busy conditions yet
		 * so this code retries a request up to 16 times if it
		 * receives a bus busy condition.  If and when the underlying
		 * code is enhanced, the retry code can be removed.
		 */
		retries = 16;
		if (pmsg->flags & I2C_M_RD) {
			while ((Status = XIic_MasterRecv(&dev->Iic,
							 pmsg->buf, pmsg->len))
			       == XST_IIC_BUS_BUSY && retries--) {
				set_current_state(TASK_INTERRUPTIBLE);
				schedule_timeout(HZ/10);
			}
		} else {
			while ((Status = XIic_MasterSend(&dev->Iic,
							 pmsg->buf, pmsg->len))
			       == XST_IIC_BUS_BUSY && retries--) {
				set_current_state(TASK_INTERRUPTIBLE);
				schedule_timeout(HZ/10);
			}
		}
		if (Status != XST_SUCCESS) {
			printk(KERN_WARNING
			       "%s #%d: Unexpected error %d.\n",
			       dev->adap.name, dev->index, Status);
			return -EIO;
		}
		printk("calling wait_for_completion\n");
		wait_for_completion(&dev->complete);
		if (dev->interrupt_status != XST_SUCCESS) {
			printk(KERN_WARNING
			       "%s #%d: Could not talk to device 0x%2x (%d).\n",
			       dev->adap.name, dev->index, pmsg->addr,
			       dev->interrupt_status);
			return -EIO;
		}
	}
	return num;
}

static int
xiic_algo_control(struct i2c_adapter *adapter,
		  unsigned int cmd, unsigned long arg)
{
	return 0;
}

static u32
xiic_bit_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_SMBUS_EMUL | I2C_FUNC_10BIT_ADDR |
	    I2C_FUNC_PROTOCOL_MANGLING;
}

static struct i2c_algorithm xiic_algo = {
	"Xilinx IIC algorithm",	/* name                 */
	/*
	 * SAATODO: Get a real ID (perhaps I2C_ALGO_XILINX) after
	 * initial release.  Will need to email lm78@stimpy.netroedge.com
	 * per http://www2.lm-sensors.nu/~lm78/support.html
	 */
	I2C_ALGO_EXP,		/* id                   */
	xiic_xfer,		/* master_xfer          */
	NULL,			/* smbus_xfer           */
	NULL,			/* slave_send           */
	NULL,			/* slave_recv           */
	xiic_algo_control,	/* algo_control         */
	xiic_bit_func,		/* functionality        */
};

/*
 * This routine is registered with the OS as the function to call when
 * the IIC interrupts.  It in turn, calls the Xilinx OS independent
 * interrupt function.  The Xilinx OS independent interrupt function
 * will in turn call any callbacks that we have registered for various
 * conditions.
 */
static void
xiic_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct xiic_dev *dev = dev_id;
	printk("+");
	XIic_InterruptHandler(&dev->Iic);
}

static void
RecvHandler(void *CallbackRef, int ByteCount)
{
	struct xiic_dev *dev = (struct xiic_dev *) CallbackRef;

	printk("r");
	if (ByteCount == 0) {
		dev->interrupt_status = XST_SUCCESS;
		printk("R");
		complete(&dev->complete);
	}
}
static void
SendHandler(void *CallbackRef, int ByteCount)
{
	struct xiic_dev *dev = (struct xiic_dev *) CallbackRef;

	printk("s");
	if (ByteCount == 0) {
		dev->interrupt_status = XST_SUCCESS;
		printk("S");
		complete(&dev->complete);
	}
}
static void
StatusHandler(void *CallbackRef, XStatus Status)
{
	struct xiic_dev *dev = (struct xiic_dev *) CallbackRef;

	printk("H");
	dev->interrupt_status = Status;
	complete(&dev->complete);
}

static void
xiic_inc_use(struct i2c_adapter *adap)
{
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
}
static void
xiic_dec_use(struct i2c_adapter *adap)
{
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}

static void
remove_head_dev(void)
{
	struct xiic_dev *dev;
	XIic_Config *cfg;

	/* Pull the head off of dev_list. */
	spin_lock(&dev_lock);
	dev = dev_list;
	dev_list = dev->next_dev;
	spin_unlock(&dev_lock);

	/*
	 * If we've told the core I2C code about this dev, tell
	 * the core I2C code to forget the dev.
	 */
	if (dev->added) {
		/*
		 * If an error is returned, there's not a whole lot we can
		 * do.  An error has already been printed out so we'll
		 * just keep trundling along.
		 */
		(void) i2c_del_adapter(&dev->adap);
	}

	/* Tell the Xilinx code to take this IIC interface down. */
	if (dev->started) {
		while (XIic_Stop(&dev->Iic) != XST_SUCCESS) {
			/* The bus was busy.  Retry. */
			printk(KERN_WARNING
			       "%s #%d: Could not stop device.  Will retry.\n",
			       dev->adap.name, dev->index);
			set_current_state(TASK_INTERRUPTIBLE);
			schedule_timeout(HZ / 2);
		}
	}

	/*
	 * Now that the Xilinx code isn't using the IRQ or registers,
	 * unmap the registers and free the IRQ.
	 */
	if (dev->remapped) {
		cfg = XIic_GetConfig(dev->index);
		iounmap((void *) cfg->BaseAddress);
		cfg->BaseAddress = dev->save_BaseAddress;
	};
	if (dev->reqirq) {
		disable_irq(dev->irq);
		free_irq(dev->irq, dev);
	}

	kfree(dev);
}

static int __init
probe(int index)
{
	static const unsigned long remap_size
	    = CONFIG_XILINX_IIC_0_HIGHADDR - CONFIG_XILINX_IIC_0_BASEADDR + 1;
	struct xiic_dev *dev;
	XIic_Config *cfg;
	unsigned int irq;
	int retval;

	switch (index) {
#if defined(CONFIG_XILINX_IIC_0_INSTANCE)
	case 0:
		irq = CONFIG_XILINX_IIC_0_IRQ;
		break;
#endif
#if defined(CONFIG_XILINX_IIC_1_INSTANCE)
	case 1:
		irq = CONFIG_XILINX_IIC_1_IRQ;
		break;
#endif
#if defined(CONFIG_XILINX_IIC_2_INSTANCE)
	case 2:
		irq = CONFIG_XILINX_IIC_2_IRQ;
		break;
#endif
#if defined(CONFIG_XILINX_IIC_3_INSTANCE)
	case 3:
		irq = CONFIG_XILINX_IIC_3_IRQ;
		break;
#endif
	default:
		return -ENODEV;
	}

	/* Find the config for our device. */
	cfg = XIic_GetConfig(index);
	if (!cfg)
		return -ENODEV;

	/* Allocate the dev and zero it out. */
	dev = (struct xiic_dev *) kmalloc(sizeof (struct xiic_dev), GFP_KERNEL);
	if (!dev) {
		printk(KERN_ERR "%s #%d: Could not allocate device.\n",
		       "Xilinx IIC adapter", index);
		return -ENOMEM;
	}
	memset(dev, 0, sizeof (struct xiic_dev));
	strcpy(dev->adap.name, "Xilinx IIC adapter");
	dev->index = index;

	/* Make it the head of dev_list. */
	spin_lock(&dev_lock);
	dev->next_dev = dev_list;
	dev_list = dev;
	spin_unlock(&dev_lock);

	init_completion(&dev->complete);

	/* Change the addresses to be virtual; save the old ones to restore. */
	dev->save_BaseAddress = cfg->BaseAddress;
	cfg->BaseAddress = (u32) ioremap(dev->save_BaseAddress, remap_size);
	dev->remapped = 1;

	/* Tell the Xilinx code to bring this IIC interface up. */
	if (XIic_Initialize(&dev->Iic, cfg->DeviceId) != XST_SUCCESS) {
		printk(KERN_ERR "%s #%d: Could not initialize device.\n",
		       dev->adap.name, dev->index);
		remove_head_dev();
		return -ENODEV;
	}
	XIic_SetRecvHandler(&dev->Iic, (void *) dev, RecvHandler);
	XIic_SetSendHandler(&dev->Iic, (void *) dev, SendHandler);
	XIic_SetStatusHandler(&dev->Iic, (void *) dev, StatusHandler);

	/* Grab the IRQ */
	dev->irq = irq;
	retval = request_irq(dev->irq, xiic_interrupt, 0, dev->adap.name, dev);
	if (retval) {
		printk(KERN_ERR "%s #%d: Could not allocate interrupt %d.\n",
		       dev->adap.name, dev->index, dev->irq);
		remove_head_dev();
		return retval;
	}
	dev->reqirq = 1;

	if (XIic_Start(&dev->Iic) != XST_SUCCESS) {
		printk(KERN_ERR "%s #%d: Could not start device.\n",
		       dev->adap.name, dev->index);
		remove_head_dev();
		return -ENODEV;
	}
	dev->started = 1;

	/* Now tell the core I2C code about our new device. */
	/*
	 * SAATODO: Get a real ID (perhaps I2C_HW_XILINX) after
	 * initial release.  Will need to email lm78@stimpy.netroedge.com
	 * per http://www2.lm-sensors.nu/~lm78/support.html
	 */
	dev->adap.id = xiic_algo.id | I2C_DRIVERID_EXP0;
	dev->adap.algo = &xiic_algo;
	dev->adap.algo_data = NULL;
	dev->adap.inc_use = xiic_inc_use;
	dev->adap.dec_use = xiic_dec_use;
	dev->adap.timeout = XIIC_TIMEOUT;
	dev->adap.retries = XIIC_RETRY;
	retval = i2c_add_adapter(&dev->adap);
	if (retval) {
		printk(KERN_ERR "%s #%d: Could not add I2C adapter.\n",
		       dev->adap.name, dev->index);
		remove_head_dev();
		return retval;
	}
	dev->added = 1;

	printk(KERN_INFO "%s #%d at 0x%08X mapped to 0x%08X, irq=%d\n",
	       dev->adap.name, dev->index,
	       dev->save_BaseAddress, cfg->BaseAddress, dev->irq);

	/* scan bus */
	if (scan) {
		int i, anyfound;

		printk(KERN_INFO "%s #%d Bus scan:",
		       dev->adap.name, dev->index);
		/*
		 * The Philips I2C-bus specification defines special addresses:
		 *      0x00       General Call
		 *      0x01       CBUS Address
		 *      0x02       Reserved for different bus format
		 *      0x03       Reserved for future purposes
		 *   0x04 to 0x07  Hs-mode master code
		 *   0x78 to 0x7B  10-bit slave addressing
		 *   0x7C to 0x7F  Reserved for future purposes
		 *
		 * We don't bother scanning any of these addresses.
		 */
		anyfound = 0;
		for (i = 0x08; i < 0x78; i++) {
			u8 data;
			XStatus Status;

			if (XIic_SetAddress(&dev->Iic, XII_ADDR_TO_SEND_TYPE,
					    i) != XST_SUCCESS) {
				printk(KERN_WARNING
				       "%s #%d: Could not set address to %2x.\n",
				       dev->adap.name, dev->index, i);
				continue;
			}
			dev->interrupt_status = ~(XStatus) 0;
			Status = XIic_MasterRecv(&dev->Iic, &data, 1);
			if (Status != XST_SUCCESS) {
				printk(KERN_WARNING
				       "%s #%d: Unexpected error %d.\n",
				       dev->adap.name, dev->index, Status);
				continue;
			}
			printk("calling wait for completion\n");
			wait_for_completion(&dev->complete);
			if (dev->interrupt_status == XST_SUCCESS) {
				printk(" 0x%02x", i);
				anyfound = 1;
			}
		}
		printk(anyfound ? " responded.\n" : " Nothing responded.\n");
	}

	return 0;
}
static int __init
xiic_init(void)
{
	int index = 0;

	while (probe(index++) == 0) ;
	/* If we found at least one, report success. */
	return (index > 1) ? 0 : -ENODEV;
}

static void __exit
xiic_cleanup(void)
{
	while (dev_list)
		remove_head_dev();
}

EXPORT_NO_SYMBOLS;

module_init(xiic_init);
module_exit(xiic_cleanup);
