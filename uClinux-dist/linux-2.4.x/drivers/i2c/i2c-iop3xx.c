/* ------------------------------------------------------------------------- */
/* i2c-iop3xx.c i2c driver algorithms for Intel XScale IOP3xx                */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2003 Peter Milne, D-TACQ Solutions Ltd
 *                      <Peter dot Milne at D hyphen TACQ dot com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, version 2.


    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                */
/* ------------------------------------------------------------------------- */
/*
   With acknowledgements to i2c-algo-ibm_ocp.c by 
   Ian DaSilva, MontaVista Software, Inc. idasilva@mvista.com

   And i2c-algo-pcf.c, which was created by Simon G. Vogl and Hans Berglund:

     Copyright (C) 1995-1997 Simon G. Vogl, 1998-2000 Hans Berglund
   
   And which acknowledged Kyösti Mälkki <kmalkki@cc.hut.fi>,
   Frodo Looijaard <frodol@dds.nl>, Martin Bailey<mbailey@littlefeet-inc.com>

  ---------------------------------------------------------------------------*/


#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/ioport.h>
#include <linux/i2c.h>


#include <asm/arch-iop3xx/iop321.h>
#include <asm/arch-iop3xx/iop321-irqs.h>
#include <asm/io.h>
#include "i2c-iop3xx.h"

/* ----- global defines ----------------------------------------------- */
#define PASSERT(x) do { if (!(x) ) \
		printk(KERN_CRIT "PASSERT %s in %s:%d\n", #x, __FILE__, __LINE__ );\
	} while (0)


/* --------------------------------------------------------------------	*/

#ifndef wait_event_interruptible_timeout
#define __wait_event_interruptible_timeout(wq, condition, ret)		\
do {									\
	wait_queue_t __wait;						\
	init_waitqueue_entry(&__wait, current);				\
									\
	add_wait_queue(&wq, &__wait);					\
	for (;;) {							\
		set_current_state(TASK_INTERRUPTIBLE);			\
		if (condition)						\
			break;						\
		if (!signal_pending(current)) {				\
			ret = schedule_timeout(ret);			\
			if (!ret)					\
				break;					\
			continue;					\
		}							\
		ret = -ERESTARTSYS;					\
		break;							\
	}								\
	current->state = TASK_RUNNING;					\
	remove_wait_queue(&wq, &__wait);				\
} while (0)

#define wait_event_interruptible_timeout(wq, condition, timeout)	\
({									\
	long __ret = timeout;						\
	if (!(condition))						\
		__wait_event_interruptible_timeout(wq, condition, __ret); \
	__ret;								\
})
#endif

/* ----- global variables ---------------------------------------------	*/


static inline unsigned char iic_cook_addr(struct i2c_msg *msg) 
{
	unsigned char addr;

	addr = (msg->addr << 1);

	if (msg->flags & I2C_M_RD)
		addr |= 1;

	/* PGM: what is M_REV_DIR_ADDR - do we need it ?? */
	if (msg->flags & I2C_M_REV_DIR_ADDR)
		addr ^= 1;

	return addr;   
}


static inline void iop3xx_adap_reset(struct i2c_algo_iop3xx_data *iop3xx_adap)
{
	/* Follows devman 9.3 */
	__raw_writel(IOP321_ICR_UNIT_RESET, iop3xx_adap->biu->vaddr + CR_OFFSET);
	__raw_writel(IOP321_ISR_CLEARBITS, iop3xx_adap->biu->vaddr + SR_OFFSET);
	__raw_writel(0, iop3xx_adap->biu->vaddr + CR_OFFSET);
} 

static inline void iop3xx_adap_set_slave_addr(struct i2c_algo_iop3xx_data *iop3xx_adap)
{
	__raw_writel(MYSAR, iop3xx_adap->biu->vaddr + SAR_OFFSET);
}

static inline void iop3xx_adap_enable(struct i2c_algo_iop3xx_data *iop3xx_adap)
{
	u32 cr = IOP321_ICR_GCD|IOP321_ICR_SCLEN|IOP321_ICR_UE;

	/* NB SR bits not same position as CR IE bits :-( */
	iop3xx_adap->biu->SR_enabled = IOP321_ISR_ALD | IOP321_ISR_BERRD |
		IOP321_ISR_RXFULL | IOP321_ISR_TXEMPTY;

	cr |= IOP321_ICR_ALDIE | IOP321_ICR_BERRIE |
		IOP321_ICR_RXFULLIE | IOP321_ICR_TXEMPTYIE;

	__raw_writel(cr, iop3xx_adap->biu->vaddr + CR_OFFSET);
}

static void iop3xx_adap_transaction_cleanup(struct i2c_algo_iop3xx_data *iop3xx_adap)
{
	unsigned cr = __raw_readl(iop3xx_adap->biu->vaddr + CR_OFFSET);
	
	cr &= ~(IOP321_ICR_MSTART | IOP321_ICR_TBYTE | 
		IOP321_ICR_MSTOP | IOP321_ICR_SCLEN);
	__raw_writel(cr, iop3xx_adap->biu->vaddr + CR_OFFSET);
}

static void iop3xx_adap_final_cleanup(struct i2c_algo_iop3xx_data *iop3xx_adap)
{
	unsigned cr = __raw_readl(iop3xx_adap->biu->vaddr + CR_OFFSET);
	
	cr &= ~(IOP321_ICR_ALDIE | IOP321_ICR_BERRIE |
		IOP321_ICR_RXFULLIE | IOP321_ICR_TXEMPTYIE);
	iop3xx_adap->biu->SR_enabled = 0;
	__raw_writel(cr, iop3xx_adap->biu->vaddr + CR_OFFSET);
}

/* 
 * NB: the handler has to clear the source of the interrupt! 
 * Then it passes the SR flags of interest to BH via adap data
 */
static void iop3xx_i2c_handler(int this_irq, 
				void *dev_id, 
				struct pt_regs *regs) 
{
	struct i2c_algo_iop3xx_data *iop3xx_adap = dev_id;
	u32 sr = __raw_readl(iop3xx_adap->biu->vaddr + SR_OFFSET);

	if ((sr &= iop3xx_adap->biu->SR_enabled)) {
		__raw_writel(sr, iop3xx_adap->biu->vaddr + SR_OFFSET);
		iop3xx_adap->biu->SR_received |= sr;
		wake_up_interruptible(&iop3xx_adap->waitq);
	}
}

/* check all error conditions, clear them , report most important */
static int iop3xx_adap_error(u32 sr)
{
	int rc = 0;

	if ((sr&IOP321_ISR_BERRD)) {
		if ( !rc ) rc = -I2C_ERR_BERR;
	}
	if ((sr&IOP321_ISR_ALD)) {
		if ( !rc ) rc = -I2C_ERR_ALD;		
	}
	return rc;	
}

static inline u32 get_srstat(struct i2c_algo_iop3xx_data *iop3xx_adap)
{
	unsigned long flags;
	u32 sr;

	spin_lock_irqsave(&iop3xx_adap->lock, flags);
	sr = iop3xx_adap->biu->SR_received;
	iop3xx_adap->biu->SR_received = 0;
	spin_unlock_irqrestore(&iop3xx_adap->lock, flags);

	return sr;
}

/*
 * sleep until interrupted, then recover and analyse the SR
 * saved by handler
 */
typedef int (* compare_func)(unsigned test, unsigned mask);
/* returns 1 on correct comparison */

static int iop3xx_adap_wait_event(struct i2c_algo_iop3xx_data *iop3xx_adap, 
				  unsigned flags, unsigned* status,
				  compare_func compare)
{
	unsigned sr = 0;
	int interrupted;
	int done;
	int rc = 0;

	do {
		interrupted = wait_event_interruptible_timeout (
			iop3xx_adap->waitq,
			(done = compare( sr = get_srstat(iop3xx_adap),flags )),
			iop3xx_adap->timeout
			);
		if ((rc = iop3xx_adap_error(sr)) < 0) {
			*status = sr;
			return rc;
		}else if (!interrupted) {
			*status = sr;
			return -ETIMEDOUT;
		}
	} while(!done);

	*status = sr;

	return 0;
}

/*
 * Concrete compare_funcs 
 */
static int all_bits_clear(unsigned test, unsigned mask)
{
	return (test & mask) == 0;
}
static int any_bits_set(unsigned test, unsigned mask)
{
	return (test & mask) != 0;
}

static int iop3xx_adap_wait_tx_done(struct i2c_algo_iop3xx_data *iop3xx_adap, int *status)
{
	return iop3xx_adap_wait_event( 
		iop3xx_adap, 
	        IOP321_ISR_TXEMPTY|IOP321_ISR_ALD|IOP321_ISR_BERRD,
		status, any_bits_set);
}

static int iop3xx_adap_wait_rx_done(struct i2c_algo_iop3xx_data *iop3xx_adap, int *status)
{
	return iop3xx_adap_wait_event( 
		iop3xx_adap, 
		IOP321_ISR_RXFULL|IOP321_ISR_ALD|IOP321_ISR_BERRD,
		status,	any_bits_set);
}

static int iop3xx_adap_wait_idle(struct i2c_algo_iop3xx_data *iop3xx_adap, int *status)
{
	return iop3xx_adap_wait_event( 
		iop3xx_adap, IOP321_ISR_UNITBUSY, status, all_bits_clear);
}

/*
 * Description: This performs the IOP3xx initialization sequence
 * Valid for IOP321. Maybe valid for IOP310?.
 */
static int iop3xx_adap_init (struct i2c_algo_iop3xx_data *iop3xx_adap)
{
#ifdef CONFIG_ARCH_IOP321
	*IOP321_GPOD &= ~(iop3xx_adap->channel==0 ?
			  IOP321_GPOD_I2C0:
			  IOP321_GPOD_I2C1);
#endif

	iop3xx_adap_reset(iop3xx_adap);
	iop3xx_adap_set_slave_addr(iop3xx_adap);
	iop3xx_adap_enable(iop3xx_adap);
	
        return 0;
}

static int iop3xx_adap_send_target_slave_addr(struct i2c_algo_iop3xx_data *iop3xx_adap,
					      struct i2c_msg* msg)
{
	unsigned cr = __raw_readl(iop3xx_adap->biu->vaddr + CR_OFFSET);
	int status;
	int rc;

	__raw_writel(iic_cook_addr(msg), iop3xx_adap->biu->vaddr + DBR_OFFSET);
	
	cr &= ~(IOP321_ICR_MSTOP | IOP321_ICR_NACK);
	cr |= IOP321_ICR_MSTART | IOP321_ICR_TBYTE;

	__raw_writel(cr, iop3xx_adap->biu->vaddr + CR_OFFSET);
	rc = iop3xx_adap_wait_tx_done(iop3xx_adap, &status);
	/* this assert fires every time, contrary to IOP manual	
	PASSERT((status&IOP321_ISR_UNITBUSY)!=0);
	*/
	PASSERT((status&IOP321_ISR_RXREAD)==0);
	     
	return rc;
}

static int iop3xx_adap_write_byte(struct i2c_algo_iop3xx_data *iop3xx_adap, char byte, int stop)
{
	unsigned cr = __raw_readl(iop3xx_adap->biu->vaddr + CR_OFFSET);
	int status;
	int rc = 0;

	__raw_writel(byte, iop3xx_adap->biu->vaddr + DBR_OFFSET);
	cr &= ~IOP321_ICR_MSTART;
	if (stop) {
		cr |= IOP321_ICR_MSTOP;
	} else {
		cr &= ~IOP321_ICR_MSTOP;
	}
	cr |= IOP321_ICR_TBYTE;
	__raw_writel(cr, iop3xx_adap->biu->vaddr + CR_OFFSET);
	rc = iop3xx_adap_wait_tx_done(iop3xx_adap, &status);

	return rc;
} 

static int iop3xx_adap_read_byte(struct i2c_algo_iop3xx_data *iop3xx_adap,
				 char* byte, int stop)
{
	unsigned cr = __raw_readl(iop3xx_adap->biu->vaddr + CR_OFFSET);
	int status;
	int rc = 0;

	cr &= ~IOP321_ICR_MSTART;

	if (stop) {
		cr |= IOP321_ICR_MSTOP|IOP321_ICR_NACK;
	} else {
		cr &= ~(IOP321_ICR_MSTOP|IOP321_ICR_NACK);
	}
	cr |= IOP321_ICR_TBYTE;
	__raw_writel(cr, iop3xx_adap->biu->vaddr + CR_OFFSET);

	rc = iop3xx_adap_wait_rx_done(iop3xx_adap, &status);

	*byte = __raw_readl(iop3xx_adap->biu->vaddr + DBR_OFFSET);

    
	return rc;
}

static int iop3xx_i2c_writebytes(struct i2c_adapter *i2c_adap, 
				 const char *buf, int count)
{
	struct i2c_algo_iop3xx_data *iop3xx_adap = i2c_adap->algo_data;
	int ii;
	int rc = 0;

	for (ii = 0; rc == 0 && ii != count; ++ii) {
		rc = iop3xx_adap_write_byte(iop3xx_adap, buf[ii], ii==count-1);
	}
	return rc;
}

static int iop3xx_i2c_readbytes(struct i2c_adapter *i2c_adap, 
				char *buf, int count)
{
	struct i2c_algo_iop3xx_data *iop3xx_adap = i2c_adap->algo_data;
	int ii;
	int rc = 0;

	for (ii = 0; rc == 0 && ii != count; ++ii) {
		rc = iop3xx_adap_read_byte(iop3xx_adap, &buf[ii], ii==count-1);
	}
	return rc;
}

/*
 * Description:  This function implements combined transactions.  Combined
 * transactions consist of combinations of reading and writing blocks of data.
 * FROM THE SAME ADDRESS
 * Each transfer (i.e. a read or a write) is separated by a repeated start
 * condition.
 */
static int iop3xx_handle_msg(struct i2c_adapter *i2c_adap, struct i2c_msg* pmsg) 
{
	struct i2c_algo_iop3xx_data *iop3xx_adap = i2c_adap->algo_data;
	int rc;

	rc = iop3xx_adap_send_target_slave_addr(iop3xx_adap, pmsg);
	if (rc < 0) {
		return rc;
	}

	if ((pmsg->flags&I2C_M_RD)) {
		return iop3xx_i2c_readbytes(i2c_adap, pmsg->buf, pmsg->len);
	} else {
		return iop3xx_i2c_writebytes(i2c_adap, pmsg->buf, pmsg->len);
	}
}

/*
 * master_xfer() - main read/write entry
 */
static int iop3xx_master_xfer(struct i2c_adapter *i2c_adap, struct i2c_msg msgs[], int num)
{
	struct i2c_algo_iop3xx_data *iop3xx_adap = i2c_adap->algo_data;
	int im = 0;
	int ret = 0;
	int status;

	iop3xx_adap_wait_idle(iop3xx_adap, &status);
	iop3xx_adap_reset(iop3xx_adap);
	iop3xx_adap_enable(iop3xx_adap);

	for (im = 0; ret == 0 && im != num; im++) {
		ret = iop3xx_handle_msg(i2c_adap, &msgs[im]);
	}

	iop3xx_adap_transaction_cleanup(iop3xx_adap);

	if(ret)
		return ret;

	return im;   
}

static int algo_control(struct i2c_adapter *adapter, unsigned int cmd,
			unsigned long arg)
{
	return 0;
}

static u32 iic_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}


/* -----exported algorithm data: -------------------------------------	*/

static struct i2c_algorithm iic_algo = {
	.name		= "IOP3xx I2C algorithm",
	.id		= 0x14,
	.master_xfer	= iop3xx_master_xfer,
	.algo_control	= algo_control,
	.functionality	= iic_func,
};

/* 
 * registering functions to load algorithms at runtime 
 */
static int i2c_iop3xx_add_bus(struct i2c_adapter *iic_adap)
{
	struct i2c_algo_iop3xx_data *iop3xx_adap = iic_adap->algo_data;

	if (!request_region( iop3xx_adap->biu->paddr, 
			      IOP3XX_I2C_IO_SIZE,
			      iic_adap->name)) {
		return -ENODEV;
	}

	init_waitqueue_head(&iop3xx_adap->waitq);
	spin_lock_init(&iop3xx_adap->lock);

	if (iop3xx_adap->biu->vaddr == 0) {
		iop3xx_adap->biu->vaddr = (u32)
					  ioremap( iop3xx_adap->biu->paddr,
						   IOP3XX_I2C_IO_SIZE);
		if (iop3xx_adap->biu->vaddr == 0)
			return -ENOMEM;
	}

	if (request_irq( 
		     iop3xx_adap->biu->irq,
		     iop3xx_i2c_handler,
		     /* SA_SAMPLE_RANDOM */ 0,
		     iic_adap->name,
		     iop3xx_adap)) {
		return -ENODEV;
	}			  

	/* register new iic_adapter to i2c module... */
	iic_adap->id |= iic_algo.id;
	iic_adap->algo = &iic_algo;

	iic_adap->timeout = 100;	/* default values, should */
	iic_adap->retries = 3;		/* be replaced by defines */

	iop3xx_adap_init(iic_adap->algo_data);
	i2c_add_adapter(iic_adap);
	return 0;
}

static int i2c_iop3xx_del_bus(struct i2c_adapter *iic_adap)
{
	struct i2c_algo_iop3xx_data *iop3xx_adap = iic_adap->algo_data;

	iop3xx_adap_final_cleanup(iop3xx_adap);
	free_irq(iop3xx_adap->biu->irq, iop3xx_adap);

	release_region(iop3xx_adap->biu->paddr, IOP3XX_I2C_IO_SIZE);

	return i2c_del_adapter(iic_adap);
}

#ifdef CONFIG_ARCH_IOP321

#define	NUMPORTS	2

static struct iop3xx_biu biu0 = {
	.paddr	= 0xfffff680,
	.vaddr	= 0xfffff680,
	.irq	= IRQ_IOP321_I2C_0,
};

static struct iop3xx_biu biu1 = {
	.paddr	= 0xfffff6a0,
	.vaddr	= 0xfffff6a0,
	.irq	= IRQ_IOP321_I2C_1,
};

#define ADAPTER_NAME_ROOT "IOP321 i2c biu adapter "

#elif defined(CONFIG_CPU_IXP46X)

#define	NUMPORTS	1

static struct iop3xx_biu biu0 = {
	.paddr	= 0xc8000000 + 0x11000,
	.irq	= 33,
};

#define ADAPTER_NAME_ROOT "IOP321/IXP46X i2c biu adapter "

#else
#error Please define the BIU struct iop3xx_biu for your processor arch
#endif

static struct i2c_algo_iop3xx_data algo_iop3xx_data0 = {
	.channel		= 0,
	.biu			= &biu0,
	.timeout		= 1*HZ,
};
static struct i2c_adapter iop3xx_ops0 = {
	.name			= ADAPTER_NAME_ROOT "0",
	.id			= I2C_HW_IOP321,
	.algo			= NULL,
	.algo_data		= &algo_iop3xx_data0,
};

#if NUMPORTS >= 2
static struct i2c_algo_iop3xx_data algo_iop3xx_data1 = {
	.channel		= 1,
	.biu			= &biu1,
	.timeout		= 1*HZ,
};
static struct i2c_adapter iop3xx_ops1 = {
	.name			= ADAPTER_NAME_ROOT "1",
	.id			= I2C_HW_IOP321,
	.algo			= NULL,
	.algo_data		= &algo_iop3xx_data1,
};
#endif

static int __init i2c_iop3xx_init (void)
{
	int rc;
	rc = i2c_iop3xx_add_bus(&iop3xx_ops0);
#if NUMPORTS >= 2
	rc = rc || i2c_iop3xx_add_bus(&iop3xx_ops1);
#endif
	return rc;
}

static void __exit i2c_iop3xx_exit (void)
{
	i2c_iop3xx_del_bus(&iop3xx_ops0);
#if NUMPORTS >= 2
	i2c_iop3xx_del_bus(&iop3xx_ops1);
#endif
}

module_init (i2c_iop3xx_init);
module_exit (i2c_iop3xx_exit);

MODULE_AUTHOR("D-TACQ Solutions Ltd <www.d-tacq.com>");
MODULE_DESCRIPTION("IOP3xx iic algorithm and driver");
MODULE_LICENSE("GPL");

MODULE_PARM(i2c_debug,"i");

MODULE_PARM_DESC(i2c_debug, "debug level - 0 off; 1 normal; 2,3 more verbose; 9 iic-protocol");

