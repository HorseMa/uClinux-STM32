/*
 *  linux/include/asm-arm/arch-realview/memory.h
 *
 *  Copyright (C) 2003 ARM Limited
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
#ifndef __ASM_ARCH_MEMORY_H
#define __ASM_ARCH_MEMORY_H

#define TASK_SIZE	(0x81a00000)	
#define TASK_SIZE_26	TASK_SIZE
#define TASK_UNMAPPED_BASE	(0x64000000)	

#define PHYS_OFFSET	(CONFIG_DRAM_BASE)
#define PAGE_OFFSET 	(PHYS_OFFSET)

#define __virt_to_phys(vpage) ((unsigned long) (vpage))
#define __phys_to_virt(ppage) ((void *) (ppage))
#define __virt_to_bus(vpage) ((unsigned long) (vpage))
#define __bus_to_virt(ppage) ((void *) (ppage))

#if 0
/*
 * Given a kernel address, find the home node of the underlying memory.
 */

#define KVADDR_TO_NID(addr) \
    ((((unsigned long) (addr) - 0x20000000) >> 16)&& 0xffff)
    
    
/*
 * Given a page frame number, convert it to a node id.
 */

#  define PFN_TO_NID(pfn) \
  (((((pfn) - PHYS_PFN_OFFSET) >> (16 - PAGE_SHIFT)) >= 4))

/*
 * Given a kaddr, LOCAL_MEM_MAP finds the owning node of the memory
 * and returns the index corresponding to the appropriate page in the
 * node's mem_map.
 */

#define LOCAL_MAP_NR(addr) \
       (((unsigned long)(addr) & 0x000fffff) >> PAGE_SHIFT)

#endif
#endif /*  __ASM_ARCH_MEMORY_H */
