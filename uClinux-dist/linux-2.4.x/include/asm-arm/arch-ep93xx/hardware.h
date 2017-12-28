/*
 *  File:   linux/include/asm-arm/arch-ep93xx/hardware.h
 *
 *  Copyright (C) 2003 Cirrus Logic, Inc
 *  
 *  Copyright (C) 1999 ARM Limited.
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

#ifndef __ASM_ARCH_HARDWARE_H
#define __ASM_ARCH_HARDWARE_H

#include <asm/arch/memory.h>
#include <asm/arch/regmap.h>

/*
 * When adding your regs-*.h file here, please be careful to not have any
 * macros being doubly defined.  You may need to comment out a section of
 * regmap.h to prevent that.
 */
#include <asm/arch/regs_ac97.h>
#include <asm/arch/regs_dma.h>
#include <asm/arch/regs_gpio.h>
#include <asm/arch/regs_ide.h>
#include <asm/arch/regs_i2s.h>
#include <asm/arch/regs_irda.h>
#include <asm/arch/regs_raster.h>
#include <asm/arch/regs_smc.h>
#include <asm/arch/regs_spi.h>
#include <asm/arch/regs_syscon.h>
#include <asm/arch/regs_touch.h>
#include <asm/arch/regs_uart.h>


#include <asm/arch/cx25871.h>

/*
 * Here's a safe way for calculating jiffies that won't break if the
 * value of HZ changes.
 */
#ifndef	MSECS_TO_JIFFIES
#define MSECS_TO_JIFFIES(ms) (((ms)*HZ+999)/1000)
#endif


#endif /* __ASM_ARCH_HARDWARE_H */
