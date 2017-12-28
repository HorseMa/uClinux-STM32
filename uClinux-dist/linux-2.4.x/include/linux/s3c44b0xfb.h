/*
 * linux/include/linux/s3c44b0xfb.h
 *
 * Copyright (C) 2003 Stefan Macher <macher@sympat.de>
 *                    Alexander Assel <assel@sympat.de>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */
 
#ifndef __LINUX_S3C44B0XFB_H__
#define __LINUX_S3C44B0XFB_H__

#include <asm/ioctl.h>
#include <asm/types.h>
 
/** struct to get the contrast-range and the default contrast */
struct lcdcontrast {
	int min; /**< minimum contrast */
	int max; /**< maximum contrast */
	int def; /**< default contrast */
};
 
 /* IOCTL definitions */
#define FBIO_S3C44B0X_SET_CONTRAST     _IOWR('F', 0x01, __u32)
#define FBIO_S3C44B0X_GET_CONTRAST     _IOR('F', 0x02, __u32)
#define FBIO_S3C44B0X_GET_CONTRAST_LIM _IOWR('F', 0x03, struct lcdcontrast)
 
#endif
