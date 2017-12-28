/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2004 Cavium Networks
 */
#ifndef __ASM_MACH_CAVIUM_OCTEON_CPU_FEATURE_OVERRIDES_H
#define __ASM_MACH_CAVIUM_OCTEON_CPU_FEATURE_OVERRIDES_H

#include <linux/types.h>
#include <asm/mipsregs.h>

/*
 * Cavium Octeons are MIPS64v2 processors
 */
#define cpu_dcache_line_size()	128
#define cpu_icache_line_size()	128

#ifdef CONFIG_SMP
#define cpu_has_llsc		1
#else
/* Disable LL/SC on non SMP systems. It is faster to disable interrupts for
   atomic access than a LL/SC */
#define cpu_has_llsc		0
#endif
#define cpu_has_prefetch  	1
#define cpu_has_dc_aliases      0
#define cpu_has_fpu             0
#define cpu_has_64bits          1
#define cpu_has_octeon_cache    1
#define cpu_has_4k_cache	0
#define cpu_has_saa             octeon_has_saa()
#define cpu_has_mips64r2        1
#define cpu_has_counter         1
#define ARCH_HAS_READ_CURRENT_TIMER 1
#define ARCH_HAS_IRQ_PER_CPU 1
#define ARCH_HAS_SPINLOCK_PREFETCH 1
#define spin_lock_prefetch(x) prefetch(x)
#define PREFETCH_STRIDE 128

static inline int read_current_timer(unsigned long *result)
{
	asm volatile ("rdhwr %0,$31\n"
#ifndef CONFIG_64BIT
		      "sll %0, 0\n"
#endif
		      : "=r" (*result));
	return 0;
}

static inline int octeon_has_saa(void)
{
#ifdef CONFIG_CPU_CAVIUM_OCTEON
	int id;
	asm volatile ("mfc0 %0, $15,0" : "=r" (id));
	return id >= 0x000d0300;
#else
	return 0;
#endif
}

#endif
