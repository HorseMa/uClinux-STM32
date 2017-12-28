/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */
#ifndef __LINUX_FR400CC_I2C_H
#define __LINUX_FR400CC_I2C_H

#define I2C_MOD_N		2

#ifdef __KERNEL__
#define FR400CC_I2C_MINOR_0	252
#define FR400CC_I2C_MINOR_1	253
#else
#include <sys/ioctl.h>
#endif	/* __KERNEL__ */

struct fr400i2c_read{

#define FR400CC_I2C_RTYPE_TRANS_FIN	0
#define FR400CC_I2C_RTYPE_REQ_SBUF	1
#define FR400CC_I2C_RTYPE_ERR		2

#define FR400CC_I2C_RCOUNT_ERR_DETECT_BUSERR		0x10
#define FR400CC_I2C_RCOUNT_ERR_DETECT_STOP_COND		0x12
#define FR400CC_I2C_RCOUNT_ERR_DETECT_RSTART_COND	0x14
#define FR400CC_I2C_RCOUNT_ERR_DETECT_ARBITO_LOST	0x16
#define FR400CC_I2C_RCOUNT_ERR_DETECT_NACK		0x18

	int type;
	int count;
};

struct fr400i2c_config{
	int addr; /* ADR (7 bit) */
	int mss; /* BCR.MSS (0: slave , 1: master) */
	int hsm; /* CCR.HSM (0: standard speed , 1: high speed)*/
	int cs; /* CCR.CS (5 bit) */

	int bufsz; /* mmap size , set by driver */
};

struct fr400i2c_sbuf{
	int offset;
	int bytes;
};

struct fr400i2c_master{
	int addr; /* target slave */
	int bytes; /* total trans bytes */
};

struct fr400i2c_send_recv{
	struct fr400i2c_master mst_send;
	struct fr400i2c_sbuf sbuf_send;
	struct fr400i2c_master mst_recv;
	struct fr400i2c_sbuf sbuf_recv;
};

#define I2CIOCGCFG	_IOR('c',1,struct fr400i2c_config)
#define I2CIOCSCFG	_IOW('c',2,struct fr400i2c_config)
#define I2CIOCSBUF	_IOW('c',5,struct fr400i2c_sbuf)
#define I2CIOCSEND	_IOW('c',10,struct fr400i2c_master)
#define I2CIOCRECV	_IOW('c',11,struct fr400i2c_master)
#define I2CIOCSEND_RECV	_IOW('c',20,struct fr400i2c_send_recv)
#define I2CIOCSTOP	_IO('c',12)

#endif /* __LINUX_FR400CC_I2C_H */
