/*
 *  linux/arch/arm/mach-ep93xx/irq.c
 *
 *  Copyright (C) 1999 ARM Limited
 *
 * (c) Copyright 2001 LynuxWorks, Inc., San Jose, CA.  All rights reserved.
 *
 *  Copyright (C) 2002-2003 Cirrus Logic, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/init.h>

#include <asm/mach/irq.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/arch/hardware.h>

/********************************************************************
 *	Interrupt functions are defined as machine specific here.
 *
 *	Note:
 *		1.  Only normal interrupts are defined here.
 *		FIQs are a separate class of interrupts and would
 *		be slowed down if handled like normal interrupts.
 *
 *		2.  TBD Invalid interrupt numbers are not checked.
 *		Some interrupt inputs are tied to GND, which means 
 *		they will immediately activate when unmasked.
 *		Which may be useful for some devices.
 *
 *		3.  TBD Edge triggered interrupts are not specially
 *		handled.  The architecture should provide a way to
 *		set up the edge trigger features and then a way to
 *		control the acknowledge to the interrupt.
 *
 ************************************************************************/

static void ep93xx_mask_irq1(unsigned int irq)
{
	outl( (1 << irq), VIC0INTENCLEAR );
}

static void ep93xx_unmask_irq1(unsigned int irq)
{
	outl( (1 << irq), VIC0INTENABLE );
}
 
static void ep93xx_mask_irq2(unsigned int irq)
{
	outl( (1 << (irq - 32)), VIC1INTENCLEAR );
}

static void ep93xx_unmask_irq2(unsigned int irq)
{
	outl( (1 << (irq - 32)), VIC1INTENABLE );
}
 
void __init ep93xx_init_irq(void)
{
	unsigned int i;

	for (i = 0; i < NR_IRQS; i++) {
		if ((i < 32) && (INT1_IRQS & (1 << i))) {
			irq_desc[i].valid	= 1;
			irq_desc[i].probe_ok	= 1;
			irq_desc[i].mask_ack	= ep93xx_mask_irq1;
			irq_desc[i].mask	= ep93xx_mask_irq1;
			irq_desc[i].unmask	= ep93xx_unmask_irq1;
		}
		if ((i >= 32) && (INT2_IRQS) & (1 << (i - 32))) {
			irq_desc[i].valid	= 1;
			irq_desc[i].probe_ok	= 1;
			irq_desc[i].mask_ack	= ep93xx_mask_irq2;
			irq_desc[i].mask	= ep93xx_mask_irq2;
			irq_desc[i].unmask	= ep93xx_unmask_irq2;
		}
	}
}

