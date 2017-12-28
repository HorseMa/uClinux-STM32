/*
 * include/asm-armnommu/arch-S3C44B0X/memory.h
 *
 * Copyright (c) 2003 by Thomas Eschenbacher <thomas.eschenbacher@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#ifndef __ASM_ARCH_MEMORY_H
#define __ASM_ARCH_MEMORY_H

#define TASK_SIZE	(0x01a00000UL)
#define TASK_SIZE_26	TASK_SIZE

#define PHYS_OFFSET     (DRAM_BASE) /*(DRAM_BASE + 2*1024*1024)*/
#define PAGE_OFFSET 	(PHYS_OFFSET)
#define END_MEM		(DRAM_BASE + DRAM_SIZE) /*(DRAM_BASE + DRAM_SIZE -2*1024*1024)*/

#define __virt_to_phys__is_a_macro
#define __virt_to_phys(vpage) ((unsigned long) (vpage))
#define __phys_to_virt__is_a_macro
#define __phys_to_virt(ppage) ((void *) (ppage))

#define __virt_to_bus__is_a_macro
#define __virt_to_bus(vpage) ((unsigned long) (vpage))
#define __bus_to_virt__is_a_macro
#define __bus_to_virt(ppage) ((void *) (ppage))

#endif /* __ASM_ARCH_MEMORY_H */
