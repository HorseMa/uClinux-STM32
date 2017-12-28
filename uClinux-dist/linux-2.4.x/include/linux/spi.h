/**
*******************************************************************************
* \brief	spi.h - SPI device driver header
*
* (C) Copyright 2004, Josef Baumgartner (josef.baumgartner@telex.de)
*
* Source code based on mcfqspi.h
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the Free
* Software Foundation; either version 2 of the License, or (at your option)
* any later version.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 675 Mass Ave, Cambridge, MA 02139, USA.
*
******************************************************************************/

#ifndef _LINUX_SPI_H
#define _LINUX_SPI_H

#include <linux/config.h>
#include <linux/types.h>


/* Motorola coldfire specific ioctls - used for compatibility with oldstyle mcfqspi driver*/
#define QSPIIOCS_DOUT_HIZ       1       /* QMR[DOHIE] set hi-z dout between transfers */
#define QSPIIOCS_BITS           2       /* QMR[BITS] set transfer size */
#define QSPIIOCG_BITS           3       /* QMR[BITS] get transfer size */
#define QSPIIOCS_CPOL           4       /* QMR[CPOL] set SCK inactive state */
#define QSPIIOCS_CPHA           5       /* QMR[CPHA] set SCK phase, 1=rising edge */
#define QSPIIOCS_BAUD           6       /* QMR[BAUD] set SCK baud rate divider */
#define QSPIIOCS_QCD            7       /* QDLYR[QCD] set start delay */
#define QSPIIOCS_DTL            8       /* QDLYR[DTL] set after delay */
#define QSPIIOCS_CONT           9       /* continuous CS asserted during transfer */
#define QSPIIOCS_READDATA       10      /* set data send during read */
#define QSPIIOCS_ODD_MOD        11      /* if length of buffer is a odd number, 16-bit transfers
                                           are finalized with a 8-bit transfer */
#define QSPIIOCS_DSP_MOD        12      /* transfers are bounded to 15/30 bytes (a multiple of 3 bytes = 1 DSP word) */
#define QSPIIOCS_POLL_MOD       13      /* driver uses polling instead of interrupts */
#define QSPIIOCS_READKDATA      14      /* set data send during read from kernel memory */

/* common ioctls */
/* TODO */


typedef struct qspi_read_data {
        __u32 length;
        __u8 buf[32];                   /* data to send during read */
        unsigned int loop : 1;
} qspi_read_data;


#endif  /* #ifndef _LINUX_SPI_H */

