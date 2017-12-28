/*
 * linux/include/linux/s3c3410ser.h
 *
 * Copyright (C) 2004 Michael Frommberger <michael.frommberger@sympat.de>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */
 
#ifndef __LINUX_S3C3410SER_H__
#define __LINUX_S3C3410SER_H__

#include <asm/ioctl.h>
#include <asm/types.h>
 
/* possible values for the ioctl */
typedef enum {
	SERIAL_MODE_NONE,          /**< disable serial-mode */
	SERIAL_MODE_RS422,         /**< RS422 full-duplex (standard mode) */
	SERIAL_MODE_RS422_LISTEN,  /**< RS422 with echo on RxD */
	SERIAL_MODE_RS485_ECHO,    /**< RS485 with echo (standard mode) */
	SERIAL_MODE_RS485_NO_ECHO  /**< RS485 without echo */
} serial_mode_t;

 /* IOCTL definitions */
#define TIOSERMODE _IOW('T', 0x01, serial_mode_t)
 
#endif
