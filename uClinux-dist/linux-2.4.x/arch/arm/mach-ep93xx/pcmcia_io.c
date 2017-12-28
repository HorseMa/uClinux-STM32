/*
 *  linux/arch/arm/mach-ep93xx/pcmcia_io.c
 *
 * (c) Copyright 2004 Cirrus Logic, Inc., Austin, Tx 
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
#include <linux/config.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/utsname.h>

#include <asm/irq.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/memory.h>
#include <asm/arch/platform.h>

#include <asm/mach/arch.h>

#ifndef __EP93XX_PCMCIA_IDE_HACK__
#define __EP93XX_PCMCIA_IDE_HACK__

/*
 * These routines are used to hack the default ide i/o routines in
 * drivers/ide/ide-iops.h so they work with PCMCIA IDE.  Sad, but
 * necessary.
 */
#define SET_PCMCIA_WIDTH_16_BIT \
	unsigned long ulSMC_PCIO = inl( SMC_PCIO );	 \
	if ( (ulSMC_PCIO & PCCONFIG_MW_16BIT)	== 0 ) {   \
		outl( ulSMC_PCIO | PCCONFIG_MW_16BIT, SMC_PCIO ); \
		ulSMC_PCIO = inl( SMC_PCIO ); \
	}
	
#define SET_PCMCIA_WIDTH_8_BIT \
	unsigned long ulSMC_PCIO = inl( SMC_PCIO );	\
	if ( ulSMC_PCIO & PCCONFIG_MW_16BIT ) {		\
		outl( ulSMC_PCIO & ~PCCONFIG_MW_16BIT, SMC_PCIO ); \
		ulSMC_PCIO = inl( SMC_PCIO ); \
	}


u8 ep93xx_pcmcia_ide_inb (unsigned long port)
{
	SET_PCMCIA_WIDTH_8_BIT
	return (u8) inb(port);
}

u16 ep93xx_pcmcia_ide_inw (unsigned long port)
{
	SET_PCMCIA_WIDTH_16_BIT
	return (u16) inw(port);
}

void ep93xx_pcmcia_ide_insw (unsigned long port, void *addr, u32 count)
{
	SET_PCMCIA_WIDTH_16_BIT
	return insw(port, addr, count);
}

void ep93xx_pcmcia_ide_outb (u8 addr, unsigned long port)
{
	SET_PCMCIA_WIDTH_8_BIT
	outb(addr, port);
	ulSMC_PCIO = inl( SMC_PCIO );
}

void ep93xx_pcmcia_ide_outw (u16 addr, unsigned long port)
{
	SET_PCMCIA_WIDTH_16_BIT
	outw(addr, port);
	ulSMC_PCIO = inl( SMC_PCIO );
}

void ep93xx_pcmcia_ide_outsw (unsigned long port, void *addr, u32 count)
{
	SET_PCMCIA_WIDTH_16_BIT
	outsw(port, addr, count);
	ulSMC_PCIO = inl( SMC_PCIO );
}
#endif /* __EP93XX_PCMCIA_IDE_HACK__ */

