/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */
#ifndef _LINUX_FUJITSU_MB93493_I2S_H
#define _LINUX_FUJITSU_MB93493_I2S_H

#include <linux/types.h>
#include <linux/version.h>
#include <asm/ioctl.h>

#ifdef __KERNEL__
#define FR400I2S_MINOR	250
#endif	/* __KERNEL__ */

struct fr400i2s_read {
	int stat;
#define I2S_READ_STAT_IN	0
#define I2S_READ_STAT_OUT	0x10000

	int count;
	struct timeval captime;
};

struct fr400i2s_config {
	int channel_n;		/* 1: mono, 2: stereo */
	int bit;		/* 8,16,32 */
	int freq;		/* ex. 44100 - UNUSED */
	int exch_lr;		/* L <--> R */
	/* big endian */
	/* signed */

	int buf_unit_sz;	/* (bytes/sample) * 4096 * N */
	int buf_num;

	int out_buf_offset;	/* in buff + offset (bytes) - UNUSED */
	int out_channel_n;	/* 1: mono, 2: stereo */
	int out_bit;		/* 8,16,32 */
	int out_freq;		/* ex. 44100 - UNUSED */
	int out_exch_lr;	/* L <--> R */
	int out_buf_unit_sz;	/* (bytes/sample) * 4096 * N */
	int out_buf_num;
	int out_swap_bytes;	/* swap data bytes of output data */
	int fs, fl, div;
	int sdmo, amo;
	int sdmi;
	int ami;
};

#define I2SIOCGCFG		_IOR('c',1,struct fr400i2s_config)
#define I2SIOCSCFG		_IOW('c',2,struct fr400i2s_config)
#define I2SIOCSTART		_IOW('c',3,int)
#define I2SIOCSTOP		_IO ('c',4)
#define I2SIOCGSTATIME		_IOR('c',5,struct timeval)
#define I2SIOCGCOUNT		_IOR('c',6,int)
#define I2SIOCTEST		_IOW('c',10,int)

#define I2SIOC_OUT_SDAT		_IOW('c',20,int)
#define I2SIOC_OUT_START	_IOW('c',21,int)
#define I2SIOC_OUT_STOP		_IO ('c',22)
#define I2SIOC_OUT_GSTATIME	_IOR('c',23,struct timeval)
#define I2SIOC_OUT_GCOUNT	_IOR('c',24,int)

#define	I2SIOC_OUT_PAUSE	_IO ('c',32)
#define	I2SIOC_IN_TRANSFER	_IOW('c',33, int)
#define	I2SIOC_OUT_DMAMODE	_IOR('c',34, int)
#define	I2SIOC_IN_ERRINFO	_IOR('c',35, int)
#define	I2SIOC_OUT_RPLAY	_IO ('c',36)
#define	I2SIOC_IN_GETSTS	_IOR('c',37, int)
#define	I2SIOC_OUT_GETSTS	_IOR('c',38, int)
#define	I2SIOC_IN_BLOCKTIME	_IOR('c',39, int)
#define	I2SIOC_OUT_GCHKBUF	_IOR('c',40, int)
#define	I2SIOC_OUT_DRAIN	_IOR('c',41, int)


#define	I2S_DRV_STS_STOP	0x1
#define	I2S_DRV_STS_READY	0x2
#define	I2S_DRV_STS_PLAY	0x4
#define	I2S_DRV_STS_PAUSE	0x10
#define	I2S_DRV_STS_ERROR	0x20

#endif /* _LINUX_FUJITSU_MB93493_I2S_H */
