/*
 * Copyright (C) 2004 Red Hat, Inc. All Rights Reserved.
 * Written by Mark Salter (msalter@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <linux/ioport.h>
#include <linux/errno.h>
#include <linux/sched.h>

#include <linux/i2c.h>
#include <linux/i2c-algo-mb93493.h>

#include <asm/mb93493-regs.h>


/* ----- global defines ----------------------------------------------- */
#define DEB(x) if (i2c_debug>=1) x
#define DEB2(x) if (i2c_debug>=2) x
#define DEB3(x) if (i2c_debug>=3) x /* print several statistical values*/
#define DEBPROTO(x) if (i2c_debug>=9) x;
 	/* debug the protocol by showing transferred bits */
#define DEF_TIMEOUT 16


#define BCR_BER   (1 << 7)
#define BCR_BEIE  (1 << 6)
#define BCR_SCC   (1 << 5)
#define BCR_MSS   (1 << 4)
#define BCR_ACK   (1 << 3)
#define BCR_GCAA  (1 << 2)
#define BCR_INTE  (1 << 1)
#define BCR_INT   (1 << 0)

#define BSR_BB    (1 << 7)
#define BSR_RSC   (1 << 6)
#define BSR_AL    (1 << 5)
#define BSR_LRB   (1 << 4)
#define BSR_TRX   (1 << 3)
#define BSR_AAS   (1 << 2)
#define BSR_GCA   (1 << 1)
#define BSR_FBT   (1 << 0)

#define CCR_HSM   (1 << 6)
#define CCR_EN    (1 << 5)


/* module parameters:
 */
static int i2c_debug=1;
static int i2c_scan=0;	/* have a look at what's hanging 'round		*/

/* --- setting states on the bus with the right timing: ---------------	*/

#define get_clock(adap) adap->getclock(adap->priv)
#define regw(adap, reg, val) adap->setreg(adap->priv, reg, val)
#define regr(adap, reg) adap->getreg(adap->priv, reg)


/* --- other auxiliary functions --------------------------------------	*/

static void iic_start(struct i2c_algo_mb93493_data *adap, int addr)
{
	struct i2c_mb93493_priv *priv = adap->priv;
	regw(adap, MB93493_I2C_CCR, CCR_EN | (priv->clock & 0x5f));
	regw(adap, MB93493_I2C_DTR, addr);
	regw(adap, MB93493_I2C_BCR, BCR_BER | BCR_BEIE | BCR_MSS | BCR_ACK | BCR_INTE);
}

static void iic_restart(struct i2c_algo_mb93493_data *adap, int addr)
{
	struct i2c_mb93493_priv *priv = adap->priv;
	regw(adap, MB93493_I2C_CCR, CCR_EN | (priv->clock & 0x5f));
	regw(adap, MB93493_I2C_DTR, addr);
	regw(adap, MB93493_I2C_BCR, BCR_BER | BCR_BEIE | BCR_SCC | BCR_MSS | BCR_ACK | BCR_INTE);
}

static void iic_stop(struct i2c_algo_mb93493_data *adap)
{
	regw(adap, MB93493_I2C_BCR, BCR_BER | BCR_BEIE | BCR_ACK | BCR_INTE | BCR_INT);
	udelay(500);
}

static void iic_err_stop(struct i2c_algo_mb93493_data *adap)
{
	regw(adap, MB93493_I2C_BCR, 0);
	regw(adap, MB93493_I2C_CCR, 0x1f);
	udelay(500);
}

static void iic_reset(struct i2c_algo_mb93493_data *adap)
{
	regw(adap, MB93493_I2C_CCR, 0x1f);
}


static int mb93493_sendbytes(struct i2c_adapter *i2c_adap, const char *buf,
			     int count, int last)
{
	struct i2c_algo_mb93493_data *adap = i2c_adap->algo_data;
	int wrcount, status, timeout;
	unsigned char bcr, bsr;
    
	for (wrcount=0; wrcount<count; ++wrcount) {
		DEB2(printk("i2c-algo-mb93493: %s i2c_write: writing %2.2X\n",
		      i2c_adap->name, buf[wrcount]&0xff));

		regw(adap, MB93493_I2C_DTR, buf[wrcount]);
		regw(adap, MB93493_I2C_BCR, BCR_BER | BCR_BEIE | BCR_MSS | BCR_ACK | BCR_INTE);

		if (adap->irqwait(adap->priv) == 0) {
			bcr = regr(adap, MB93493_I2C_BCR);
			bsr = regr(adap, MB93493_I2C_BSR);
			if ((bcr & BCR_BER) || !(bsr & BSR_BB) || (bsr & (BSR_AL|BSR_LRB))) {
				iic_err_stop(adap);
				return -EREMOTEIO;
			}
		} else {
			iic_err_stop(adap);
			return -EREMOTEIO;
		}
	}
	if (last)
		iic_stop(adap);

	return wrcount;
}


static int mb93493_readbytes(struct i2c_adapter *i2c_adap, char *buf,
                         int count, int last)
{
	int i;
	struct i2c_algo_mb93493_data *adap = i2c_adap->algo_data;
	unsigned char bcr, bsr;

	/* increment number of bytes to read by one -- read dummy byte */
	for (i = 0; i < count; i++) {

		/* clock in the data */
		regw(adap, MB93493_I2C_BCR,
		     ((last && i == (count-1)) ? BCR_ACK : 0) | BCR_BER | BCR_BEIE | BCR_MSS | BCR_INTE);

		if (adap->irqwait(adap->priv) == 0) {
			bcr = regr(adap, MB93493_I2C_BCR);
			bsr = regr(adap, MB93493_I2C_BSR);
			if ((bcr & BCR_BER) || !(bsr & BSR_BB) || (bsr & BSR_AL)) {
				iic_err_stop(adap);
				return -EREMOTEIO;
			}
		} else {
			iic_err_stop(adap);
			return -EREMOTEIO;
		}

		buf[i] = regr(adap, MB93493_I2C_DTR);
	}
	if (last)
		iic_stop(adap);

	return i;
}

/* Description: Prepares the controller for a transaction (clearing status
 * registers, data buffers, etc), and then calls either iic_readbytes or
 * iic_sendbytes to do the actual transaction.
 *
 * still to be done: Before we issue a transaction, we should
 * verify that the bus is not busy or in some unknown state.
 */
static int mb93493_xfer(struct i2c_adapter *i2c_adap,
			struct i2c_msg msgs[], 
			int num)
{
	struct i2c_algo_mb93493_data *iic_adap = i2c_adap->algo_data;
	struct i2c_msg *pmsg;
	int i;
	int ret=0, timeout, status;
	unsigned char addr, bcr, bsr;

	for (i = 0; ret >= 0 && i < num; i++) {
		pmsg = &msgs[i];

		DEB2(printk("i2c-algo-mb93493.o: Doing %s %d bytes to 0x%02x - %d of %d messages\n",
		     pmsg->flags & I2C_M_RD ? "read" : "write",
		     pmsg->len, pmsg->addr, i + 1, num););
    
		if (pmsg->flags & I2C_M_TEN) {
			printk("10-bit addresses not supported\n");
			return -EIO;
		} else {
			addr = pmsg->addr << 1;
			if (pmsg->flags & I2C_M_RD )
				addr |= 1;
			if (pmsg->flags & I2C_M_REV_DIR_ADDR )
				addr ^= 1;
		}

		/* Send START */
		if (i == 0)
			iic_start(iic_adap, addr);
		else
			iic_restart(iic_adap, addr);
    
		if (iic_adap->irqwait(iic_adap->priv) == 0) {
			bcr = regr(iic_adap, MB93493_I2C_BCR);
			bsr = regr(iic_adap, MB93493_I2C_BSR);
			if ((bcr & BCR_BER) || !(bsr & BSR_BB) || (bsr & (BSR_AL|BSR_LRB))) {
				iic_err_stop(iic_adap);
				return -EREMOTEIO;
			}
		} else {
			iic_err_stop(iic_adap);
			return -EREMOTEIO;
		}
		
		DEB3(printk("i2c-algo-mb93493: Msg %d, addr=0x%x, flags=0x%x, len=%d\n",
			    i, msgs[i].addr, msgs[i].flags, msgs[i].len);)
    
		if (pmsg->flags & I2C_M_RD) {

			/* read bytes into buffer*/
			ret = mb93493_readbytes(i2c_adap, pmsg->buf, pmsg->len,
                                            (i + 1 == num));
        
			if (ret != pmsg->len) {
				DEB2(printk("i2c-algo-mb93493: fail: only read %d bytes.\n",ret));
			} else {
				DEB2(printk("i2c-algo-mb93493: read %d bytes.\n",ret));
			}

		} else {
			ret = mb93493_sendbytes(i2c_adap, pmsg->buf, pmsg->len,
                                            (i + 1 == num));
        
			if (ret != pmsg->len) {
				DEB2(printk("i2c-algo-mb93493: fail: only wrote %d bytes.\n",ret));
			} else {
				DEB2(printk("i2c-algo-mb93493: wrote %d bytes.\n",ret));
			}
		}
	}
	return i;
}


/* Implements device specific ioctls.  Higher level ioctls can
 * be found in i2c-core.c and are typical of any i2c controller (specifying
 * slave address, timeouts, etc).  These ioctls take advantage of any hardware
 * features built into the controller for which this algorithm-adapter set
 * was written.  These ioctls allow you to take control of the data and clock
 * lines and set the either high or low,
 * similar to a GPIO pin.
 */
static int algo_control(struct i2c_adapter *adapter, 
	unsigned int cmd, unsigned long arg)
{
	struct i2c_algo_mb93493_data *d = adapter->algo_data;
	struct i2c_mb93493_priv *priv = d->priv;

	switch (cmd) {
	case I2C_SET_CLOCK:
		if (arg & ~0x5f)
			return -EINVAL;
		priv->clock = arg;
		break;
	}
	return 0;
}


static u32 mb93493_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C;
}

/* -----exported algorithm data: -------------------------------------	*/

static struct i2c_algorithm iic_algo = {
	"Fujitsu MB93493 algorithm",
	I2C_ALGO_MB93493,
	mb93493_xfer,			/* master_xfer	*/
	NULL,				/* smbus_xfer	*/
	NULL,				/* slave_xmit		*/
	NULL,				/* slave_recv		*/
	algo_control,			/* ioctl		*/
	mb93493_func,			/* functionality	*/
};


/* 
 * registering functions to load algorithms at runtime 
 */
int i2c_mb93493_add_bus(struct i2c_adapter *adap)
{
	int i;
	struct i2c_algo_mb93493_data *iic_adap = adap->algo_data;
	unsigned char bsr, bcr;

	DEB2(printk("i2c-algo-mb93493: hw routines for %s registered.\n",
	            adap->name));

	/* register new adapter to i2c module... */

	adap->id |= iic_algo.id;
	adap->algo = &iic_algo;

	adap->timeout = 100;	/* default values, should	*/
	adap->retries = 3;	/* be replaced by defines	*/
	adap->flags = 0;

#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif

	i2c_add_adapter(adap);
	iic_reset(iic_adap);

	/* scan bus */
	/* By default scanning the bus is turned off. */
	if (i2c_scan) {
		printk(KERN_INFO " i2c-algo-mb93493: scanning bus %s.\n",
		       adap->name);
		for (i = 0x00; i < 0xff; i+=2) {
			iic_start(iic_adap, i);
			if (iic_adap->irqwait(iic_adap->priv) == 0) {
			    
				bcr = regr(iic_adap, MB93493_I2C_BCR);
				bsr = regr(iic_adap, MB93493_I2C_BSR);
				if (bcr & BCR_BER)
					continue;
				if ((bsr & (BSR_BB|BSR_AL|BSR_LRB)) == BSR_BB)
				    printk(KERN_INFO "\n(%02x)\n",i>>1); 
			} else
				printk("."); 
			iic_err_stop(iic_adap);
		}
	}
	return 0;
}


int i2c_mb93493_del_bus(struct i2c_adapter *adap)
{
	int res;
	if ((res = i2c_del_adapter(adap)) < 0)
		return res;

	DEB2(printk("i2c-algo-mb93493: adapter unregistered: %s\n",adap->name));

#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
	return 0;
}


int __init i2c_algo_mb93493_init (void)
{
	printk(KERN_INFO "Fujitsu MB93493 I2C algorithm module\n");
	return 0;
}


void i2c_algo_mb93493_exit(void)
{
	return;
}


EXPORT_SYMBOL(i2c_mb93493_add_bus);
EXPORT_SYMBOL(i2c_mb93493_del_bus);

/* The MODULE_* macros resolve to nothing if MODULES is not defined
 * when this file is compiled.
 */
MODULE_AUTHOR("Mark Salter, Red Hat, Inc.");
MODULE_DESCRIPTION("MB93493 I2C algorithm");
MODULE_LICENSE("GPL");

MODULE_PARM(i2c_scan, "i");
MODULE_PARM(i2c_debug,"i");

MODULE_PARM_DESC(i2c_scan, "Scan for active chips on the bus");
MODULE_PARM_DESC(i2c_debug,
        "debug level - 0 off; 1 normal; 2,3 more verbose; 9 iic-protocol");


/* This function resolves to init_module (the function invoked when a module
 * is loaded via insmod) when this file is compiled with MODULES defined.
 * Otherwise (i.e. if you want this driver statically linked to the kernel),
 * a pointer to this function is stored in a table and called
 * during the intialization of the kernel (in do_basic_setup in /init/main.c) 
 *
 * All this functionality is complements of the macros defined in linux/init.h
 */
module_init(i2c_algo_mb93493_init);


/* If MODULES is defined when this file is compiled, then this function will
 * resolved to cleanup_module.
 */
module_exit(i2c_algo_mb93493_exit);
