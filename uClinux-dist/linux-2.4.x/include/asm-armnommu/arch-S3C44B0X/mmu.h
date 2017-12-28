/*
 * include/asm-armnommu/arch-S3C44B0X/mmu.h
 *
 * Copyright (c) 2003 by Thomas Eschenbacher <thomas.eschenbacher@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#ifndef __ASM_ARCH_MMU_H
#define __ASM_ARCH_MMU_H

#define __virt_to_phys__is_a_macro
#define __virt_to_phys(vpage) vpage
#define __phys_to_virt__is_a_macro
#define __phys_to_virt(ppage) ppage

#define __virt_to_bus__is_a_macro
#define __virt_to_bus(vpage) vpage
#define __bus_to_virt__is_a_macro
#define __bus_to_virt(ppage) ppage

#define __flush_entry_to_

#endif /* __ASM_ARCH_MMU_H */
