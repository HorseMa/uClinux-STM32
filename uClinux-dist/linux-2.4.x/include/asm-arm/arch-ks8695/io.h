/*
 *  linux/include/asm-arm/arch-integrator/io.h
 *
 *  Copyright (C) 1999 ARM Limited
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
#ifndef __ASM_ARM_ARCH_IO_H
#define __ASM_ARM_ARCH_IO_H

#define IO_SPACE_LIMIT 0xffffffff

#ifdef CONFIG_PCMCIA_KS
extern void ks_out(void *bus, u32 val, u32 port, s32 sz);
extern u32 ks_in(void *bus, u32 port, s32 sz);
extern void ks_outs(void *bus, u32 port, void *buf, u32 count, s32 sz);
extern void ks_ins(void *bus, u32 port, void *buf, u32 count, s32 sz);
#define outb(v, p)      ks_out(NULL, v, p, 0)
#define outw(v, p)      ks_out(NULL, v, p, 1)
#define outl(v, p)      ks_out(NULL, v, p, 2)
#define inb(p)          ks_in(NULL, p, 0)
#define inw(p)          ks_in(NULL, p, 1)
#define inl(p)          ks_in(NULL, p, 2)
#define outsb(p, a, c)  ks_outs(NULL, p, a, c, 0)
#define outsw(p, a, c)  ks_outs(NULL, p, a, c, 1)
#define outsl(p, a, c)  ks_outs(NULL, p, a, c, 2)
#define insb(p, a, c)   ks_ins(NULL, p, a, c, 0)
#define insw(p, a, c)   ks_ins(NULL, p, a, c, 1)
#define insl(p, a, c)   ks_ins(NULL, p, a, c, 2)
#else
#define __io(a)                 (a)
#endif /* CONFIG_PCMCIA_KS */

#define __mem_pci(a)		((unsigned long)(a))
#define __mem_isa(a)		((unsigned long)(a))

/*
 * Generic virtual read/write
 */
#define __arch_getw(a)		(*(volatile unsigned short *)(a))
#define __arch_putw(v,a)	(*(volatile unsigned short *)(a) = (v))

/*
 * Validate the pci memory address for ioremap.
 */
#define iomem_valid_addr(iomem,size)	(1)

/*
 * Convert PCI memory space to a CPU physical address
 */
#define iomem_to_phys(iomem)	(iomem)

#endif
