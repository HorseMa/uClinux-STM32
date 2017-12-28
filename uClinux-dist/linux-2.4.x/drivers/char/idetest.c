/*****************************************************************************/
/*
 *	idetest.c -- A simple driver to exercise the SnapGear IDE test jig.
 *
 * 	(C) Copyright 2004, SnapGear <www.snapgear.com> 
 *  Code used from EP93xx IDE Linux driver, (C) 2003 Cirrus Logic
 */
/*****************************************************************************/

#include <linux/module.h>
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTK( fmt, arg... )  printk( fmt, ##arg )
#else
#define DPRINTK( fmt, arg... )
#endif

#define IDETEST_NAME	"idetest"
#define IDETEST_MAJOR	122
#define IDETEST_SIZE	16

/*****************************************************************************/

static int idetest_isopen;

/*****************************************************************************/

static void
set_pio(void)
{
	DPRINTK("set_pio()\n");

	/*
	 * Clear the MDMA and UDMA operation registers.
	 */
	outl(0, IDEMDMAOP);
	outl(0, IDEUDMAOP);

	/*
	 * Reset the UDMA state machine.
	 */
	outl(IDEUDMADebug_RWOE | IDEUDMADebug_RWPTR | IDEUDMADebug_RWDR |
	     IDEUDMADebug_RROE | IDEUDMADebug_RRPTR | IDEUDMADebug_RRDR,
	     IDEUDMADEBUG);
	outl(0, IDEUDMADEBUG);

	/*
	 * Enable PIO mode of operation.
	 */
	outl(IDECfg_PIO | IDECfg_IDEEN | (4 << IDECfg_MODE_SHIFT) |
	     (1 << IDECfg_WST_SHIFT), IDECFG);
}

/*****************************************************************************/

static void
ide_outb(u8 b, unsigned long addr)
{
	unsigned int uiIDECR;

	/*
	 * Write the address out.
	 */
	uiIDECR = IDECtrl_DIORn | IDECtrl_DIOWn |
		((addr & 7) << 2)/* | (addr >> 10)*/;
		
	if (addr & 8)
		uiIDECR |= IDECtrl_CS0n;
	else
		uiIDECR |= IDECtrl_CS1n;

	DPRINTK("%s: addr %lx uiIDECR %lx ", __FUNCTION__, addr, uiIDECR);
	outl(uiIDECR, IDECR);

	DPRINTK("data %x\n", b);

	/*
	 * Write the data out.
	 */

	if (addr & 8)
		outl(b << 8, IDEDATAOUT);
	else
		outl(b, IDEDATAOUT);

	/*
	 * Toggle the write signal.
	 */
	outl(uiIDECR & ~IDECtrl_DIOWn, IDECR);
	outl(uiIDECR, IDECR);
}

/*****************************************************************************/

static unsigned char
ide_inb(unsigned long addr)
{
	unsigned int uiIDECR;
	u8 b;

	/*
	 * Write the address out.
	 */
	uiIDECR = IDECtrl_DIORn | IDECtrl_DIOWn |
		((addr & 7) << 2)/* | (addr >> 10)*/;

	if (addr & 8)
		uiIDECR |= IDECtrl_CS0n;
	else
		uiIDECR |= IDECtrl_CS1n;
		
	DPRINTK("%s: addr %lx uiIDECR %lx ", __FUNCTION__, addr, uiIDECR);
	outl(uiIDECR, IDECR);

	/*
	 * Toggle the read signal.
	 */
	outl(uiIDECR & ~IDECtrl_DIORn, IDECR);
	outl(uiIDECR, IDECR);

	/*
	 * Read the data in.
	 */
	if (addr & 8)
		b = (inl(IDEDATAIN) >> 8) & 0xff;
	else
		b = inl(IDEDATAIN) & 0xff;

	DPRINTK("data %x\n", b);
	return b;
}

/*****************************************************************************/

static int
idetest_open(struct inode *inode, struct file *filp)
{
	DPRINTK("idetest_open()\n");

	if (idetest_isopen)
		return -EBUSY;

	idetest_isopen++;

	MOD_INC_USE_COUNT;

	return 0;
}

/*****************************************************************************/

static int
idetest_close(struct inode *inode, struct file *filp)
{
	DPRINTK("idetest_close()\n");

	idetest_isopen--;

	MOD_DEC_USE_COUNT;

	return 0;
}

/*****************************************************************************/

static ssize_t
idetest_write(struct file *filp, const char *buf, size_t count,loff_t *ppos)
{
	char p[IDETEST_SIZE];
	int i;

	DPRINTK("idetest_write(buf=%x,count=%d)\n",
		(int) buf, count);

	if (*ppos + count > IDETEST_SIZE)
		return -ENOSPC;
	copy_from_user(p, buf, count);
	for (i=0; i<count; i++)
		ide_outb(p[i], *ppos+i);
	*ppos += i;

	return i;
}

/*****************************************************************************/

static ssize_t
idetest_read(struct file *filp, char *buf, size_t count, loff_t *ppos)
{
	char p[IDETEST_SIZE];
	int i;

	DPRINTK("idetest_read(buf=%x,count=%d)\n",
		(int) buf, count);

	if (*ppos + count > IDETEST_SIZE)
		count = IDETEST_SIZE - *ppos;
	for (i=0; i<count; i++)
		p[i] = ide_inb(*ppos+i);
	copy_to_user(buf, p, i);
	*ppos += i;

	return i;
}

/*****************************************************************************/

static loff_t
idetest_lseek(struct file * file, loff_t offset, int orig)
{
	loff_t newpos;
	
	switch (orig) {
	case 0:
		newpos = offset;
		break;
	case 1:
		newpos = file->f_pos + offset;
		break;
	default:
		return -EINVAL;
	}

	if (newpos<0 || newpos>=IDETEST_SIZE)
		return -EINVAL;

	file->f_pos = newpos;
	return file->f_pos;
}

/*****************************************************************************/

static int
idetest_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
	int rc = 0;

	DPRINTK("idetest_ioctl(cmd=%x,arg=%x)\n", (int) cmd, (int) arg);

	switch(cmd) {
	default:
		rc = -EINVAL;
		break;
	}

	return rc;
}

/*****************************************************************************/

static struct file_operations idetest_fops = {
	llseek: idetest_lseek,
	read: idetest_read,
	write: idetest_write,
	ioctl: idetest_ioctl,
	open: idetest_open,
	release: idetest_close
};

/*****************************************************************************/

static const char banner[] __initdata =
    KERN_INFO IDETEST_NAME ": Copyright (C) 2004, SnapGear " \
	"(www.snapgear.com)\n";

static int __init idetest_init(void)
{
	DPRINTK("idetest_init()\n");

	set_pio();

	/* Register character device */
	if (register_chrdev(IDETEST_MAJOR, IDETEST_NAME, &idetest_fops) < 0) {
		printk(KERN_WARNING IDETEST_NAME": failed to allocate major %d\n",
			IDETEST_MAJOR);
		return -1;
	}

	idetest_isopen = 0;
	printk(banner);

	return 0;
}

/*****************************************************************************/

static void __exit idetest_exit(void)
{
	unregister_chrdev(IDETEST_MAJOR, IDETEST_NAME);
}

/*****************************************************************************/

module_init(idetest_init);
module_exit(idetest_exit);
MODULE_AUTHOR("Philip Craig <philipc@snapgear.com>, Robert Waldie <robertw@snapgear.com>");
MODULE_DESCRIPTION("A simple driver to exercise the SnapGear IDE test jig");
MODULE_LICENSE("GPL");

/*****************************************************************************/
