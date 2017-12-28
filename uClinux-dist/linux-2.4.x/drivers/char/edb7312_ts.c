/*
 *	Copyright (c) 2003 Petko Manolov <petkan@nucleusys.com>
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version 2 as published
 *  by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/interrupt.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/arch/irqs.h>
#include <asm/hardware/clps7111.h>


#define DRV_AUTHOR	"Petko Manolov <petkan@nucleusys.com>"
#define	DRV_DESC	"touchscreen"
MODULE_AUTHOR( DRV_AUTHOR );
MODULE_DESCRIPTION( DRV_DESC );
MODULE_LICENSE("GPL");

#undef	DEBUG
#undef	EXTENDED_MODE

#define	TS_MAJOR	16
#define	TS_IRQ		IRQ_EINT2
/* must be power of 2 */
#define	EVT_BUF_SIZE	(1<<6)
#define	EVT_BUF_MASK	(EVT_BUF_SIZE - 1)

#define	SSIBUSY		(1 << 26)
#define	ADCCON		(1 << 0)
#define	ADCCKNSEN	(1 << 4)

/* these assume 18-74Mhz mode */
#define	SSI_SPD_4K	0
#define	SSI_SPD_16K	(1 << 16)
#define	SSI_SPD_64K	(1 << 17)
#define	SSI_SPD_128K	((1 << 17) | (1 << 16))
#define	SSI_SPD_MASK	~((1 << 17) | (1 << 16))

#define	TXFRAMEN	(1 << 14)

#ifdef	EXTENDED_MODE
#define	FLEN		(24 << 7)
#define	CED(x)		((x) << 24)
#define	CMD_GET_X	(CED(0xd3) | TXFRAMEN | FLEN | 16)
#define	CMD_GET_Y	(CED(0x93) | TXFRAMEN | FLEN | 16)
#define	CMD_GET_Z1	(CED(0xb3) | TXFRAMEN | FLEN | 16)
#define	CMD_GET_Z2	(CED(0xc3) | TXFRAMEN | FLEN | 16)
#else
#define	FLEN		(24 << 8)
#define	CMD_GET_X	(TXFRAMEN | FLEN | 0xd3)
#define	CMD_GET_Y	(TXFRAMEN | FLEN | 0x93)
#define	CMD_GET_Z1	(TXFRAMEN | FLEN | 0xb3)
#define	CMD_GET_Z2	(TXFRAMEN | FLEN | 0xc3)
#endif
/* do sampling every 50ms */
#define	TS_SR		(HZ/20)

//#define TOUCH_DEBUG
#ifdef	DEBUG
#define	dbg(x, arg...)	printk(x, ##arg)
#else
#define	dbg(x, ...)
#endif	/* DEBUG */
#define	bptr_inc(x)	((x) = ((x + 1) & EVT_BUF_MASK))
#define	wait_ssibusy()	({ int t;	\
			do {t = clps_readl(SYSFLG1);} while (t & SSIBUSY);})

typedef struct {
	unsigned short pressure;
	unsigned short x;
	unsigned short y;
	unsigned short ms;
} ts_evt_t;

typedef struct {
	int state;
	int head, tail;
	ts_evt_t ev_buf[EVT_BUF_SIZE];
	int raw_x, raw_y, raw_z1, raw_z2;
	struct timer_list timer;
	struct tasklet_struct tl;
	spinlock_t lock;
	wait_queue_head_t buff_wait;
	struct semaphore open_sem;
} tss_t;

enum {
	TS_IDLE = 0,
	TS_GET_X,
	TS_GET_Y,
	TS_GET_Z1,
	TS_GET_Z2
};

static tss_t ts;
static struct fasync_struct *fasync;


static void edb7312_ts_setup(void)
{
	int tmp;

	wait_ssibusy();
	tmp = clps_readl(SYSCON3);
#ifdef EXTENDED_MODE	
	tmp |= ADCCON;
#endif
	tmp &= ~ADCCKNSEN;
	clps_writel(tmp, SYSCON3);
	/* set SSI clock speed */
	tmp = clps_readl(SYSCON1) & SSI_SPD_MASK;
	tmp |= SSI_SPD_128K;
	clps_writel(tmp, SYSCON1);
	
	/*
	 *  note: EINT2 is used by the IDE interface too, we don't
	 *  clear this bit on exit since it might be still in use;
	 */
	tmp = clps_readl(INTMR1);
	tmp |= (1 << IRQ_EINT2);
	clps_writel(tmp, INTMR1);
	
	/*
	 *  init ads7846 here... ?!?
	 */
}

/*
 *  enable the interrupts from SSI, EINT2 should also be enabled
 */
static void ssi_enable_irq(void)
{
	int tmp;
	
	tmp = clps_readl(INTMR1);
	tmp |= 1 << IRQ_SSEOTI;
	clps_writel(tmp, INTMR1);
}


static void ssi_disable_irq(void)
{
	int tmp;
	
	tmp = clps_readl(INTMR1);
	tmp &= ~(1 << IRQ_SSEOTI);
	clps_writel(tmp, INTMR1);
}

/*
 *  shit-shit-shit is what represents very accurate my feelings at the moment
 */
static void ssi_ts_irq(int irq, void *dev_id, struct pt_regs *regs)
{
	int res;
	tss_t *tsp = (tss_t *)dev_id;

//	dbg("%s: state=%d\n", __FUNCTION__, tsp->state);

	/* this read should also be an EOI */
	res = clps_readl(SYNCIO);
	
	switch (tsp->state) {
	case TS_IDLE:
		break;
	case TS_GET_X:
		tsp->raw_x = res;
		clps_writel(CMD_GET_Y, SYNCIO);
		tsp->state = TS_GET_Y;
		break;
	case TS_GET_Y:
		tsp->raw_y = res;
		clps_writel(CMD_GET_Z1, SYNCIO);
		tsp->state = TS_GET_Z1;
		break;
	case TS_GET_Z1:
		tsp->raw_z1 = res;
		clps_writel(CMD_GET_Z2, SYNCIO);
		tsp->state = TS_GET_Z2;
		break;
	case TS_GET_Z2:
		tsp->raw_z2 = res;
		tsp->state = TS_IDLE;
		tasklet_schedule(&tsp->tl);
		break;
	default:
#ifdef TOUCH_DEBUG
		printk("%s: no such state %d\n", __FUNCTION__, tsp->state);
#endif
	}
//	dbg("res=%08x\n", res);
}


static void ts_timer(unsigned long ptr)
{
	tss_t *tsp = (tss_t *)ptr;

#ifdef TOUCH_DEBUG
	if (tsp->state != TS_IDLE)
		printk("%s: run while state is not TS_IDLE\n", __FUNCTION__);
#endif
	
	clps_writel(CMD_GET_X, SYNCIO);
	tsp->state = TS_GET_X;
	
	tsp->timer.expires = jiffies + TS_SR;
	add_timer(&tsp->timer);
}


static void ts_calc(unsigned long ptr)
{
	tss_t *tsp = (tss_t *)ptr;
	int Rt;

#ifdef TOUCH_DEBUG
	if (tsp->state != TS_IDLE) {
		printk("%s: in the middle of data acquisition\n", __FUNCTION__);
	}
#endif
	
	tsp->raw_x = (tsp->raw_x >> 4) & 0xfff;
	tsp->raw_y = (tsp->raw_y >> 4) & 0xfff;
	tsp->raw_z1 = (tsp->raw_z1 >> 4) & 0xfff;
	tsp->raw_z2 = (tsp->raw_z2 >> 4) & 0xfff;
	/* put the event in buffer only if it's PENDOWN */
	spin_lock_bh(&tsp->lock);
	if (!tsp->raw_z1) {
		goto out;
	}
	Rt = (580 * tsp->raw_x * (tsp->raw_z2 - tsp->raw_z1) /
	         tsp->raw_z1) >> 12;
	if ((Rt < 0) || (Rt < 200) || (Rt > 4200))
		goto out;
	tsp->ev_buf[tsp->head].pressure = Rt;
	tsp->ev_buf[tsp->head].x = 320 - (tsp->raw_x/6);
	tsp->ev_buf[tsp->head].y = 240 - (tsp->raw_y/8);
#ifdef TOUCH_DEBUG
	printk("%s: Rt is %d, X=%d, Y=%d\n", __FUNCTION__, Rt,
	       tsp->ev_buf[tsp->head].x, tsp->ev_buf[tsp->head].y);
#endif
	bptr_inc(tsp->head);
	/* event buffer full, drop the last entry */
	if (tsp->head == tsp->tail)
		bptr_inc(tsp->tail);
out:
	spin_unlock_bh(&tsp->lock);
	wake_up_interruptible(&tsp->buff_wait);
}


static int edb7312ts_fasync(int fd, struct file *filp, int on)
{
	dbg("%s: blah\n", __FUNCTION__);
	return fasync_helper(fd, filp, on, &fasync);
}


static int edb7312ts_ioctl(struct inode * inode, struct file *filp,
                           unsigned int cmd, unsigned long arg)
{
	dbg("%s: #%d\n", __FUNCTION__, cmd);
	return 0;
}


static unsigned int edb7312ts_poll(struct file *filp, poll_table *wait)
{
	dbg("%s: blah\n", __FUNCTION__);
	poll_wait(filp, &ts.buff_wait, wait);
	if (ts.head == ts.tail)
		return POLLIN | POLLRDNORM;
	return 0;
}


static ssize_t edb7312ts_read(struct file *filp, char *buf,
                              size_t count, loff_t *l)
{
	DECLARE_WAITQUEUE(wait, current);
	size_t tmp = count;
	
	dbg("%s: requested %d bytes to read\n", __FUNCTION__, count);
	if (ts.head == ts.tail) {	/* buffer empty*/
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		add_wait_queue(&ts.buff_wait, &wait);
repeat:
		set_current_state(TASK_INTERRUPTIBLE);;
		if (ts.head == ts.tail && !signal_pending(current)) {
			schedule();
			goto repeat;
		}
		current->state = TASK_RUNNING;
		remove_wait_queue(&ts.buff_wait, &wait);
	}
	while ((ts.tail != ts.head) && (count >= sizeof(ts_evt_t))) {
		copy_to_user(buf, &ts.ev_buf[ts.tail], sizeof(ts_evt_t));
		spin_lock_bh(&ts.lock);
		bptr_inc(ts.tail);
		spin_unlock_bh(&ts.lock);
		count -= sizeof(ts_evt_t);
	}
	
	return tmp - count;
}


static int edb7312ts_open(struct inode * inode, struct file * filp)
{
	if (down_trylock(&ts.open_sem)) {
#ifdef TOUCH_DEBUG
		printk("%s: device is busy\n", __FUNCTION__);
#endif
		return -EBUSY;
	}
	/* prep the ts structure */
	ts.head = ts.tail = 0;
	/*
	 *  there is no real spin locks on ARM.  this is a bit subtle:
	 *  spin_lock_bh() resolves as local_bh_disable() which is what
	 *  we need.  also helps if one day this get ported to a smp arch...
	 */
	ts.lock = SPIN_LOCK_UNLOCKED;
		
	/* clear any pending interrupt.. */
	clps_readl(SYNCIO);
	ssi_enable_irq();
	
	init_timer(&ts.timer);
	ts.timer.function = ts_timer;
	ts.timer.data = (unsigned long)&ts;
	ts.timer.expires = jiffies + TS_SR;
	add_timer(&ts.timer);
	
	return 0;
}


static int edb7312ts_release(struct inode * inode, struct file * filp)
{
	del_timer_sync(&ts.timer);
	ssi_disable_irq();
	edb7312ts_fasync(-1, filp, 0);
	tasklet_kill(&ts.tl);
	up(&ts.open_sem);

	return 0;
}


static struct file_operations edb7312ts_fops = {
	owner:		THIS_MODULE,
	read:		edb7312ts_read,
	poll:		edb7312ts_poll,
	ioctl:		edb7312ts_ioctl,
	fasync:		edb7312ts_fasync,
	open:		edb7312ts_open,
	release:	edb7312ts_release,
};


int __init edb7312ts_init(void)
{
	int ret=0;

	/* set up everything here but enable SSEOTI in open() */
	edb7312_ts_setup();
	
	if ((ret = register_chrdev(TS_MAJOR, DRV_DESC, &edb7312ts_fops)) < 0) {
		printk("can't register char driver, major #%d\n", TS_MAJOR);
		goto out;
	}
	
	ret = request_irq(IRQ_SSEOTI, ssi_ts_irq, SA_INTERRUPT, DRV_DESC, &ts);
	if (ret) {
		printk("can't get irq #%d, ret=%d\n", IRQ_SSEOTI, ret);
		goto bad_ssi_irq;
	}
	
	memset(&ts, 0, sizeof(tss_t));
	init_MUTEX(&ts.open_sem);
	init_waitqueue_head(&ts.buff_wait);
	tasklet_init(&ts.tl, ts_calc, (unsigned long)&ts);
	printk("Registered touch-screen driver\n");	
	goto out;
	
bad_ssi_irq:
	unregister_chrdev(TS_MAJOR, DRV_DESC);
out:
	return ret;
}


void __exit edb7312ts_exit(void)
{
	free_irq(IRQ_SSEOTI, &ts);
	unregister_chrdev(TS_MAJOR, DRV_DESC);
	printk("Deregistered touch-screen driver\n");
}


module_init(edb7312ts_init);
module_exit(edb7312ts_exit);
