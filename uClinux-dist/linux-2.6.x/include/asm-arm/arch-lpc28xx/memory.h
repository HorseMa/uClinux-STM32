/*
 * linux/include/asm-arm/arch-lpc22xx/memory.h
 *
 * Copyright (C) 2004 Philips semiconductors
 */

#ifndef __ASM_ARCH_MEMORY_H
#define __ASM_ARCH_MEMORY_H

#define __virt_to_phys(vpage) ((unsigned long) (vpage))
#define __phys_to_virt(ppage)	((unsigned long) (ppage))
#define __virt_to_bus(vpage) ((unsigned long) (vpage))
#define __bus_to_virt(ppage)	((unsigned long) (ppage))

#endif /*  __ASM_ARCH_MEMORY_H */
