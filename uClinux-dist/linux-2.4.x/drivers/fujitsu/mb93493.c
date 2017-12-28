/* mb93493.c: MB93493 companion chip manager
 *
 * Copyright (C) 2004 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/sched.h>
#include <asm/mb93493-regs.h>
#include <asm/dma.h>
#include <asm/io.h>
#include "mb93493.h"

#define __get_DQSR(C)	__get_MB93493(0x3e0 + (C) * 4)
#define __set_DQSR(C,V)	__set_MB93493(0x3e0 + (C) * 4, (V))

/*****************************************************************************/
/*
 * open a DMA channel through the MB93493
 */
int mb93493_dma_open(const char *devname,
		     enum mb93493_dma_source dmasource,
		     int dmacap,
		     dma_irq_handler_t handler,
		     unsigned long irq_flags,
		     void *data)
{
	uint32_t dqsr;
	int dma;

	dma = frv_dma_open(devname, 0xf, dmacap, handler, irq_flags, data);

	if (dma >= 0) {
		dqsr = __get_DQSR(dma);
		dqsr |= 1 << (dmasource + 16);
		__set_DQSR(dma, dqsr);
	}

	return dma;
} /* end mb93493_dma_open() */

/*****************************************************************************/
/*
 * close a DMA channel
 */
void mb93493_dma_close(int dma)
{
	__set_DQSR(dma, 0);
	frv_dma_close(dma);
} /* end mb93493_dma_close() */
