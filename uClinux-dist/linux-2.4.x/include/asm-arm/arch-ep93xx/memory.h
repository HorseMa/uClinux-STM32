/*
 *  linux/include/asm-arm/arch-ep93xx/memory.h
 *
 *  Copyright (C) 1999 ARM Limited
 *  Copyright (C) 2002-2003 Cirrus Logic Corp.
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
#ifndef __ASM_ARCH_MMU_H
#define __ASM_ARCH_MMU_H

#include <linux/config.h>

#ifdef CONFIG_MACH_IPD
#define SDRAM_START                         0xc0000000
#define PHYS_OFFSET                         (0xc0000000UL)
#define SDRAM_NUMBER_OF_BLOCKS              1
#define SDRAM_BLOCK_SIZE                    0x01000000
#define SDRAM_BLOCK_START_BOUNDARY          0x00000000
#endif

/*
 * Task size: 2GB (from 0 to base of IO in virtual space)
 */
#define TASK_SIZE	(0xc0000000UL)
#define TASK_SIZE_26	(0x04000000UL)

/*
 * This decides where the kernel will search for a free chunk of vm
 * space during mmap's.
 */
#define TASK_UNMAPPED_BASE (TASK_SIZE / 3)

/*
 * Page offset: 3GB (start of kernel memory in virtual space)
 * Phys offset: 0   (start of kernel memory in physical space)
 */
#define PAGE_OFFSET	(0xc0000000UL)
#ifndef PHYS_OFFSET
#define PHYS_OFFSET	(0x00000000UL)
#endif

/*
 * We take advantage of the fact that physical and virtual address can be the
 * same.  The NUMA code is handling the large holes that might exist between
 * all memory banks.
 */
#define __virt_to_phys__is_a_macro
#define __virt_to_phys(vpage) ((vpage) - PAGE_OFFSET + PHYS_OFFSET)

#define __phys_to_virt__is_a_macro
#define __phys_to_virt(ppage) ((ppage) + PAGE_OFFSET - PHYS_OFFSET) 

/*
 * Virtual view <-> DMA view memory address translations
 * virt_to_bus: Used to translate the virtual address to an
 *              address suitable to be passed to set_dma_addr
 * bus_to_virt: Used to convert an address for DMA operations
 *              to an address that the kernel can use.
 */
#define __virt_to_bus__is_a_macro
#define __virt_to_bus(x)	 __virt_to_phys(x)

#define __bus_to_virt__is_a_macro
#define __bus_to_virt(x)	 __phys_to_virt(x)

#endif
