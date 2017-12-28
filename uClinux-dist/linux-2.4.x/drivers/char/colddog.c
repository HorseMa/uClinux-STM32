/*
 *    colddog: A watchdog driver for Motorola Coldfire, using
 *             the linux watchdog API
 *
 *    (c) Copyright 2004 Michael Leslie <mleslie@arcturusnetworks.com>
 *        for Arcturus Networks Inc.
 *
 *     colddog is based on softdog by Alan Cox.
 */
 
#include <linux/module.h>
#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>
#include <linux/reboot.h>
#include <linux/smp_lock.h>
#include <linux/init.h>
#include <asm/uaccess.h>

#include <asm/coldfire.h>
#include <asm/mcfsim.h>



/* watchdog register access macros: */
#define WRRR   *(volatile unsigned short *)(MCF_MBAR + MCFSIM_WRRR)
#define WIRR   *(volatile unsigned short *)(MCF_MBAR + MCFSIM_WIRR)
#define WCR    *(volatile unsigned short *)(MCF_MBAR + MCFSIM_WCR)
#define WER    *(volatile unsigned short *)(MCF_MBAR + MCFSIM_WER)


/*
 * set the default MCF watchdog timeout to the maximum time permitted
 * by the Coldfire hardware. This depends on the clock speed,
 * for example:
 *
 * MCF_CLK=40e6: timeout(max) = 20.84s
 * MCF_CLK=66e6: timeout(max) = 16.27s
 */
#define TIMER_MARGIN        ((32768 * 32768)/MCF_CLK)       /* seconds */
#define DEFAULT_IRQ_MARGIN  1
static int irq_margin  = DEFAULT_IRQ_MARGIN;                /* seconds */
static int soft_margin = TIMER_MARGIN - DEFAULT_IRQ_MARGIN; /* in seconds */

static int expect_close = 0;



MODULE_PARM(soft_margin,"i");
MODULE_LICENSE("GPL");

#ifdef CONFIG_WATCHDOG_NOWAYOUT
static int nowayout = 1;
#else
static int nowayout = 0;
#endif

MODULE_PARM(nowayout,"i");
MODULE_PARM_DESC(nowayout, "Watchdog cannot be stopped once started (default=CONFIG_WATCHDOG_NOWAYOUT)");


/*
 * Function declarations
 */
 
static void colddog_expire(int irq, void *dev_id, struct pt_regs *regs);
static int  colddog_setup(int seconds);
static void colddog_refresh(void);
static void colddog_halt(void);

extern void	dump(struct pt_regs *regs);

static unsigned long watchdog_awake;


/*
 *	If the watchdog expires, dump the stack frame
 */
 
static void colddog_expire(int irq, void *dev_id, struct pt_regs *regs)
{
	int i, jiffprev;

	cli();
	printk(KERN_CRIT "WATCHDOG: Interrupt triggered. Hardware reset pending.\n");
	while (1) {};
}

/*
 *	Allow only one person to hold it open
 */
 
static int colddog_open(struct inode *inode, struct file *file)
{
	if (test_and_set_bit(0, &watchdog_awake))
		return -EBUSY;

	if (nowayout) {
		MOD_INC_USE_COUNT;
	}
	/*
	 *	Activate timer
	 */
	if (colddog_setup(soft_margin))
		return -EINVAL;

	/* mleslie debug: */
	printk ("WATCHDOG: started. Interrupt at %d seconds, reset at %d seconds\n",
			soft_margin, soft_margin + irq_margin);

	return 0;
}

static int colddog_release(struct inode *inode, struct file *file)
{
	/*
	 *	Shut off the timer.
	 * 	Lock it in if it's a module and we set nowayout
	 */
	if (expect_close && nowayout == 0) {
		colddog_halt();
	} else {
		printk(KERN_CRIT 
			   "WATCHDOG: WDT device closed unexpectedly.  WDT will not stop!\n");
	}

	/* mleslie debug: */
	printk ("WATCHDOG: released.\n");

	clear_bit(0, &watchdog_awake);
	return 0;
}

static ssize_t colddog_write(struct file *file, const char *data,
							 size_t len, loff_t *ppos)
{
	/*  Can't seek (pwrite) on this device  */
	if (ppos != &file->f_pos)
		return -ESPIPE;

	/*
	 *	Refresh the timer.
	 */
	if(len) {
		if (!nowayout) {
			size_t i;

			/* In case it was set long ago */
			expect_close = 0;

			for (i = 0; i != len; i++) {
				char c;
				if (get_user(c, data + i))
					return -EFAULT;
				if (c == 'V') {
					expect_close = 1;

					/* mleslie debug: */
					printk ("WATCHDOG: 'expect_close' asserted\n");
				}
			}
		}
		/* mod_timer(&watchdog_ticktock, jiffies+(soft_margin*HZ)); */
		colddog_refresh();
		return 1;
	}
	return 0;
}

static int colddog_ioctl(struct inode *inode, struct file *file,
	unsigned int cmd, unsigned long arg)
{
	int new_margin;
	static struct watchdog_info ident = {
		WDIOF_SETTIMEOUT | WDIOF_MAGICCLOSE,
		0,
		"Coldfire Watchdog"
	};

	switch (cmd) {
		default:
			return -ENOTTY;
		case WDIOC_GETSUPPORT:
			if(copy_to_user((struct watchdog_info *)arg, &ident, sizeof(ident)))
				return -EFAULT;
			return 0;
		case WDIOC_GETSTATUS:
		case WDIOC_GETBOOTSTATUS:
			return put_user(0,(int *)arg);
		case WDIOC_KEEPALIVE:
			colddog_refresh();
			return 0;
		case WDIOC_SETTIMEOUT:
			if (get_user(new_margin, (int *)arg))
				return -EFAULT;
			if (new_margin < 1)
				return -EINVAL;
			if (colddog_setup(new_margin))
				return -EINVAL;

			soft_margin = new_margin;
			/* Fall through */
		case WDIOC_GETTIMEOUT:
			return put_user(soft_margin, (int *)arg);
	}
}


/*
 * set up the MCF watchdog timer to expire in the given number of
 * seconds. Note that the maximum differs depending on the clock
 * speed. See the definition of TIMER_MARGIN, above.
 */
static int colddog_setup(int seconds)
{
	unsigned int ref;

	ref = (seconds + irq_margin) * (MCF_CLK/32768) - 1;

	if (ref > 32767) {
		printk ("WATCHDOG: Invalid timeout request of %d seconds.\n", seconds);
		printk ("          Maximum timeout is %ds including a %ds irq margin.\n",
				(32768*32768)/MCF_CLK, irq_margin);
		return (1);
	}
	WRRR = ((unsigned short)ref << 1) | 0x0001; /* set ref + enable bit */

	/* trigger the interrupt roughly one frame dump to the console earlier */
	ref = seconds * (MCF_CLK/32768);
	WIRR = ((unsigned short)ref << 1) | 0x0001; /* set ref + enable bit */

	
	/* mleslie debug: */
	printk ("WATCHDOG setup: ");
	/* { int i = jiffies; while (i+50 > jiffies); } */
	printk ("WRRR = 0x%04x  WIRR = 0x%04x\n", WRRR, WIRR);

	return (0);
}

static void colddog_halt()
{
	WRRR &= 0xfffe; /* mask enable bit */
	WIRR &= 0xfffe; /* mask enable bit */

	/* mleslie debug: */
	printk ("WATCHDOG halt: WRRR = 0x%04x  WIRR = 0x%04x\n", WRRR, WIRR);
}


/*
 * Reset the Watchdog Counter Register
 */
static void colddog_refresh()
{
	/* Write any value to the WCR to reset the count: */
	WCR = 0;
}


static struct file_operations colddog_fops = {
	owner:		THIS_MODULE,
	write:		colddog_write,
	ioctl:		colddog_ioctl,
	open:		colddog_open,
	release:	colddog_release,
};

static struct miscdevice colddog_miscdev = {
	minor:		WATCHDOG_MINOR,
	name:		"watchdog",
	fops:		&colddog_fops,
};

static char banner[] __initdata = KERN_INFO "MCF Watchdog Timer: timer margin: %d sec\n";


static int __init watchdog_init(void)
{
	int ret, irq;
	volatile unsigned long *icrp;

	ret = misc_register(&colddog_miscdev);

	if (ret)
		return ret;

	/* Register interrupt handler: */

#if defined(CONFIG_M5272)
	irq = MCF_INT_INT1;

	/* Enable SWTO irq and give it priority 4 */
	icrp = (volatile unsigned long *) (MCF_MBAR + MCFSIM_ICR4);
	*icrp = (*icrp & 0x77707777) | 0x000F0000;
#endif

	if (request_irq(MCF_INT_SWTO, colddog_expire, 0, "watchdog", NULL))
		panic("Unable to attach watchdog interrupt\n");

	printk(banner, soft_margin);

	if (WRRR & 0x0001) {
		printk ("WARNING: Watchdog has already been enabled.\n");
		printk ("         Be sure to service it by writing to /dev/watchdog.\n");
		colddog_refresh();
	}

	return 0;
}

static void __exit watchdog_exit(void)
{
	misc_deregister(&colddog_miscdev);
}

module_init(watchdog_init);
module_exit(watchdog_exit);

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 */
