/*
 * Copyright (C) 2004 Red Hat, Inc. All Rights Reserved.
 * Written by Mark Salter (msalter@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/spinlock.h>

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-mb93493.h>

#include <asm/mb93493-regs.h>
#include <asm/mb93493-irqs.h>

static int own = 0x55;
static int i2c_debug = 0;

/* ----- global defines -----------------------------------------------	*/
#define DEB(x)	if (i2c_debug>=1) x
#define DEB2(x) if (i2c_debug>=2) x
#define DEB3(x) if (i2c_debug>=3) x
#define DEBE(x)	x	/* error messages 				*/

/* ----- local functions ----------------------------------------------	*/

static void mb93493_setreg(void *data, int reg, int val)
{
	void *addr = ((struct i2c_mb93493_priv *)data)->base + reg;

	udelay(500);
	__builtin_write32(addr, val);
	udelay(500);
}

static int mb93493_getreg(void *data, int reg)
{
	void *addr = ((struct i2c_mb93493_priv *)data)->base + reg;
	int val;

	udelay(500);
	val = __builtin_read32(addr);
	udelay(500);

	return val;
}

static int mb93493_getown(void *data)
{
	return (own);
}

static int mb93493_irqwait(void *data)
{
	struct i2c_mb93493_priv *priv = (struct i2c_mb93493_priv *)data;
	DECLARE_WAITQUEUE(wait, current);
	unsigned long flags;
	long timeout = 1 * HZ;
	int err = 0;

	priv->pending = 1;

	enable_irq(priv->bus ? IRQ_MB93493_I2C_1 : IRQ_MB93493_I2C_0);

	add_wait_queue(&priv->waitq, &wait);
	for (;;) {
		set_current_state(TASK_UNINTERRUPTIBLE);
		spin_lock_irqsave(&priv->slock, flags);
		if (priv->pending == 0) {
			spin_unlock_irqrestore(&priv->slock, flags);
			break;
		}
		spin_unlock_irqrestore(&priv->slock, flags);
		timeout = schedule_timeout(timeout);
		if (timeout == 0) {
			err = -1;
			break;
		}
	}
	priv->pending = 0;
	current->state = TASK_RUNNING;
	remove_wait_queue(&priv->waitq, &wait);

	return err;
}


static void mb93493_irq_handler(int this_irq, void *dev_id, struct pt_regs *regs)
{
	struct i2c_mb93493_priv *priv = (struct i2c_mb93493_priv *)dev_id;
	unsigned long flags;

	spin_lock_irqsave(&priv->slock, flags);

	priv->pending = 0;
	wake_up_interruptible(&priv->waitq);
	spin_unlock_irqrestore(&priv->slock, flags);

	disable_irq(this_irq);
}


static int mb93493_reg(struct i2c_client *client)
{
	return 0;
}


static int mb93493_unreg(struct i2c_client *client)
{
	return 0;
}

static void mb93493_inc_use(struct i2c_adapter *adap)
{
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
}

static void mb93493_dec_use(struct i2c_adapter *adap)
{
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}



/* ------------------------------------------------------------------------
 * Encapsulate the above functions in the correct operations structure.
 * This is only done when more than one hardware adapter is supported.
 */
static struct i2c_mb93493_priv mb93493_priv[2] = {
        { 0, (void *)(__region_CS3), 0, 0x1f },
        { 1, (void *)(__region_CS3 + 0x20), 0, 0x1f }
};

static struct i2c_algo_mb93493_data mb93493_data[2] = {
	{
		&mb93493_priv[0],
		mb93493_setreg,
		mb93493_getreg,
		mb93493_getown,
		mb93493_irqwait
	},
	{
		&mb93493_priv[1],
		mb93493_setreg,
		mb93493_getreg,
		mb93493_getown,
		mb93493_irqwait
	}
};

static struct i2c_adapter mb93493_board_adapter[2] = {
        {
                name:              "MB93493 I2C 0",
                id:                I2C_HW_MB_MB,
                algo:              NULL,
                algo_data:         &mb93493_data[0],
                inc_use:           mb93493_inc_use,
                dec_use:           mb93493_dec_use,
                client_register:   mb93493_reg,
                client_unregister: mb93493_unreg,
		client_count:      0
        } , 
        {
                name:              "MB93493 I2C 1",
                id:                I2C_HW_MB_MB,
                algo:              NULL,
                algo_data:         &mb93493_data[1],
                inc_use:           mb93493_inc_use,
                dec_use:           mb93493_dec_use,
                client_register:   mb93493_reg,
                client_unregister: mb93493_unreg,
		client_count:      0
        }
};


static int mb93493_init(int ctlr)
{
	static char *names[2] = { "MB93493 I2C 0", "MB93493 I2C 1" };
	unsigned v;

	if (ctlr < 0 || ctlr > 1)
		return -1;

	v = __get_MB93493_LBSER();
	if (ctlr == 0)
		v |= MB93493_LBSER_I2C_0;
	else if (ctlr == 1)
		v |= MB93493_LBSER_I2C_1;
	__set_MB93493_LBSER(v);

	if (request_irq((ctlr ? IRQ_MB93493_I2C_1 : IRQ_MB93493_I2C_0),
			mb93493_irq_handler, SA_SHIRQ, names[ctlr],
			&mb93493_priv[ctlr]) < 0) {
		return -1;
	}

	disable_irq(ctlr ? IRQ_MB93493_I2C_1 : IRQ_MB93493_I2C_0);

	init_waitqueue_head(&mb93493_priv[ctlr].waitq);
	mb93493_priv[ctlr].slock = (spinlock_t)SPIN_LOCK_UNLOCKED;
	mb93493_priv[ctlr].pending = 0;

	return 0;
}

static void __exit mb93493_exit(int ctlr)
{
	unsigned v;

	v = __get_MB93493_LBSER();
	if (ctlr == 0)
		v &= ~MB93493_LBSER_I2C_0;
	else if (ctlr == 1)
		v &= ~MB93493_LBSER_I2C_1;
	__set_MB93493_LBSER(v);

	if (ctlr == 0)
		free_irq(IRQ_MB93493_I2C_0, &mb93493_data[0]);
	else if (ctlr == 1)
		free_irq(IRQ_MB93493_I2C_1, &mb93493_data[1]);
}



int __init i2c_mb93493_init(void) 
{
	printk("i2c-mb93493.o: i2c adapter module for Fujitsu MB93493\n");

	if (mb93493_init(0) == 0) {
		if (i2c_mb93493_add_bus(&mb93493_board_adapter[0]) < 0)
			return -ENODEV;
	}
	if (mb93493_init(1) == 0) {
		if (i2c_mb93493_add_bus(&mb93493_board_adapter[1]) < 0)
			return -ENODEV;
	}
	
	return 0;
}


EXPORT_NO_SYMBOLS;

#ifdef MODULE
MODULE_AUTHOR("Mark Salter, Red Hat, Inc.");
MODULE_DESCRIPTION("I2C adapter routines for MB93493");
MODULE_LICENSE("GPL");

MODULE_PARM(own, "i");
MODULE_PARM(i2c_debug, "i");

int init_module(void) 
{
	return i2c_mb93493_init();
}

void cleanup_module(void) 
{
	i2c_mb93493_del_bus(&mb93493_board_adapter[0]);
	i2c_mb93493_del_bus(&mb93493_board_adapter[1]);
	mb93493_exit(0);
	mb93493_exit(1);
}

#endif
