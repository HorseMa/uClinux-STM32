/*
 *  linux/arch/arm/mach-ep93xx/mm.c
 *
 *  Extra MM routines for the Cirrus EP93xx
 *
 *  Copyright (C) 1999,2000 Arm Limited
 *  Copyright (C) 2000 Deep Blue Solutions Ltd
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
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/init.h>

#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/page.h>
#include <asm/arch/bits.h> 
#include <asm/sizes.h> 
#include <asm/arch/platform.h> 
 
#include <asm/mach/map.h>

/*
 *  IO Map for EP93xx:
 *
 *  8000 0000 - 807f ffff = AHB peripherals  (8 Meg)
 *  8080 0000 - 809f ffff = APB peripherals  (2 Meg)
 *  a000 0000 - b000 0000 = PCMCIA space
 */ 
static struct map_desc ep93xx_io_desc[] __initdata = 
{
    //
    // Virtual Address                Physical Addresss    Size in Bytes  Domain     R  W  C  B Last
    { IO_BASE_VIRT,                   IO_BASE_PHYS,        IO_SIZE,       DOMAIN_IO, 0, 1, 0, 0 },
    { PCMCIA_BASE_VIRT,               PCMCIA_BASE_PHYS,    PCMCIA_SIZE,   DOMAIN_IO, 0, 1, 0, 0 },
    LAST_DESC
};

void __init ep93xx_map_io(void)
{
	iotable_init(ep93xx_io_desc);
}

