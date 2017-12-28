/*
 * Copyright (C) 2004 Red Hat, Inc. All Rights Reserved.
 * Written by Mark Salter (msalter@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef I2C_ALGO_MB93493_H
#define I2C_ALGO_MB93493_H 1

#include <linux/i2c.h>

#define I2C_SET_CLOCK	0x880

#ifdef __KERNEL__

struct i2c_mb93493_priv {
	int  bus;
	void *base;
	int  pending;
	int  clock;
	spinlock_t slock;
        wait_queue_head_t waitq;
};

struct i2c_algo_mb93493_data {
	void *priv;
	void (*setreg)  (void *data, int reg, int val);
	int  (*getreg)  (void *data, int reg);
	int  (*getown)  (void *data);
	int  (*irqwait) (void *data);
};


int i2c_mb93493_add_bus(struct i2c_adapter *);
int i2c_mb93493_del_bus(struct i2c_adapter *);
#endif

#endif /* I2C_ALGO_MB93493_H */
