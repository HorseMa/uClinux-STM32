/**
*******************************************************************************
*
* \brief	spi_core.h - SPI device driver header
*
* (C) Copyright 2004, Josef Baumgartner (josef.baumgartner@telex.de)
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

#ifndef __INC_SPI_CORE_H
#define __INC_SPI_CORE_H

#include <linux/types.h>

/* defines */
#define SPI_MAJOR		126
#define DEVICE_NAME		"qspi"

#define	MEM_TYPE_USER		0
#define MEM_TYPE_KERNEL		1


/* function declarations */
extern int spi_dev_release(void *dev);
extern void *spi_dev_open(int num);
extern ssize_t spi_dev_read(void *dev, char *buffer, size_t length, int memtype);
extern ssize_t spi_dev_write(void *dev, const char *buffer, size_t length, int memtype);
extern int spi_dev_ioctl(void *dev, unsigned int cmd, unsigned long arg);


#endif  /* #ifndef __INC_SPI_CORE_H */

