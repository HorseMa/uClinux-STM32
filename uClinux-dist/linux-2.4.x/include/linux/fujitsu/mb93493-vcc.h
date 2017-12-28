/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */
#ifndef _LINUX_FUJITSU_MB93493_VCC_H
#define _LINUX_FUJITSU_MB93493_VCC_H

#include <linux/types.h>
#include <linux/version.h>
#include <asm/ioctl.h>

#define FR400CC_VCC_DEBUG_IOCTL

#ifdef __KERNEL__
#define FR400CC_VCC_CAPTURE_MINOR	240
#define FR400CC_VCC_VSYNC_MINOR		241
#endif	/* __KERNEL__ */

struct fr400cc_vcc_read {
#define FR400CC_VCC_RTYPE_CAPTURE 0
#define FR400CC_VCC_RTYPE_VSYNC   1
	int		type;
	int		count;
	struct timeval	captime;
};

struct fr400cc_vcc_config {
	int		encoding;
#define FR400CC_VCC_ENCODE_YC8		1
#define FR400CC_VCC_ENCODE_YC16		2
#define FR400CC_VCC_ENCODE_RGB24	3
	int		interlace;	/* 0:off / 1:on */
	int		skipbf;		/* Skip bottom field(0:not skip / 1:skip) */
	int		fw, fh;		/* Frame size (in pixel) */

	__u16		rhtcc;
	__u16		rhcc;
	__u16		rhbc;
	__u16		rvcc;
	__u16		rvbc;
	__u16		rhr;
	__u16		rvr;
	__u16		rssp;		/* dummy */
	__u16		rsep;		/* dummy */
	__u16		rcc_to;
	__u16		rcc_fdts;
	__u16		rcc_ifi;
	__u16		rcc_es;
  	__u16		rcc_csm;
  	__u16		rcc_cfp;
  	__u16		rcc_vsip;
  	__u16		rcc_hsip;
  	__u16		rcc_cpf;
	__u16		rhyf0;
	__u16		rhyf1;
	__u16		rhyf2;
	__u16		rhcf0;
	__u16		rhcf1;
	__u16		rhcf2;
	__u16		rvf0;
	__u16		rvf1;
};


#define FR400CC_VCC_MAX_FRAME		32
struct fr400cc_vcc_mbuf {
	int		size;		/* mmap size */
	int		frames;
	int		offsets[FR400CC_VCC_MAX_FRAME];
};

struct fr400cc_vcc_wait {
	int		num;		/* W */
	int		captured_num;	/* R */
	int		first_frame;	/* R */
	unsigned	count;		/* R */	/* not used */
};

struct fr400cc_vcc_info {
	int		status;
#define FR400CC_VCC_ST_STOP		1
#define FR400CC_VCC_ST_FRAMEFULL	2
#define FR400CC_VCC_ST_CAPTURING	3
#define FR400CC_VCC_ST_VSYNC		9
	int		count;
	int		field;
	int		frame_num_k;
	int		frame_num_u;
	int		rest;
	int		total_lost;
	int		total_lost_bf;
	int		total_ov;
	int		total_err;
};

struct fr400cc_vcc_frame_info {
	int		frame;			/* W */
	int		status;			/* R */
#define FR400CC_VCC_FI_FREE		0
#define FR400CC_VCC_FI_CAPTURING	1
#define FR400CC_VCC_FI_CAPTURED_K	2
#define FR400CC_VCC_FI_CAPTURED_U	3
	int		count;			/* R */
	int		lost_count;		/* R */
	struct timeval	captime;		/* R */
};

struct fr400cc_vcc_regio{
	unsigned	reg_offset;
	unsigned	value;
};

#define VCCIOCGCFG	_IOR('c',1,struct fr400cc_vcc_config)
#define VCCIOCSCFG	_IOW('c',2,struct fr400cc_vcc_config)
#define VCCIOCGMBUF	_IOR('c',3,struct fr400cc_vcc_mbuf)
#define VCCIOCSTART	_IOW('c',4,int)
#define VCCIOCSTOP	_IO('c',5)
#define VCCIOCWAIT	_IOWR('c',6,struct fr400cc_vcc_wait)
#define VCCIOCFREE	_IOW('c',7,int)

#define VCCIOCGINF	_IOR('c',8,struct fr400cc_vcc_info)
#define VCCIOCGFINF	_IOR('c',9,struct fr400cc_vcc_frame_info)

#define VCCIOCWAITNB	_IOWR('c',10,struct fr400cc_vcc_wait)

#define VCCIOCGSTTIME	_IOR('c',11,struct timeval)
#define VCCIOCSCFINF	_IO('c',12)

#define VCCIOCVSSTART	_IO('c',0x20)
#define VCCIOCVSSTOP	_IO('c',0x21)
#define VCCIOCGREGIO	_IOR('c',0x28,struct fr400cc_vcc_regio)
#define VCCIOCSREGIO	_IOR('c',0x29,struct fr400cc_vcc_regio)

#ifdef FR400CC_VCC_DEBUG_IOCTL
struct __fr400cc_vcc_regio {
	/* reg_no: (VCC_RHR | VCC_RHTCC |
	 * VCC_RHBC | VCC_RVCC |VCC_RVBC
	 * VCC_RDTS | VCC_RCC | VCC_RIS
	 * VCC_TPI)
	 */
	unsigned	reg_no;		/* W */
	unsigned	value;		/* R/W */
};
#define __VCCIOCGREGIO	_IOR('c',0x80,struct __fr400cc_vcc_regio)
#define __VCCIOCSREGIO	_IOR('c',0x81,struct __fr400cc_vcc_regio)
#endif

#endif	/* _LINUX_FUJITSU_MB93493_VCC_H */
