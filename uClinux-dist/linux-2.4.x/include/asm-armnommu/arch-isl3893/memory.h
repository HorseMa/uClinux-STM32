/*  $Header$
 *
 * linux/include/asm-armnommu/arch-isl3893/memory.h
 *
 * Copyright (c) 1999 Nicolas Pitre <nico@cam.org>
 * Copyright (c) 2001 RidgeRun <www.ridgerun.com>
 * Copyright (C) 2002 Intersil Americas Inc.
 *
 * 02/19/2001  Gordon McNutt  Leveraged for the dsc21.
 */

#ifndef __ASM_ARCH_MEMORY_H
#define __ASM_ARCH_MEMORY_H


/*
 * TASK_SIZE is the maximum size of a user process in bytes.
 * In case of an MMU, user space always starts at zero, so this is
 * the maximum address that a user process can access+1. Any virtual
 * address below TASK_SIZE is deemed to be user process	area, and
 * anything above TASK_SIZE is the kernel segment.
 * In the NO_MM case, this cannot be said, because kernel and user space
 * can be anywhere in physical memory. Therefore, we define TASK_SIZE as
 * the highest possible memory address. It might be better to remove this
 * check in the source for the NO_MM case.
 */
#define TASK_SIZE		0xFFFFFFFF
#define TASK_SIZE_26	TASK_SIZE


/*
 * The start of physical memory. This is the first SDRAM address.
 * This value is used in the arch-specific bootup code like setup_arch &
 * bootmem_init. By default we assume that we have one block of contiguous
 * memory (a node) that starts here and runs MEM_SIZE long (see below). By
 * default the ARM bootup code sets MEM_SIZE to be 16MB  which is just right for
 * us (see arch/armnommu/kernel/setup.c).
 *
 */
#define PHYS_OFFSET	DRAM_BASE

/*
 * PAGE_OFFSET -- Virtual start address of the first bank of RAM. For archs with
 * no MMU this corresponds to the first free page in physical memory (aligned
 * on a page boundary).
 */
#define PAGE_OFFSET	PHYS_OFFSET

#endif
