/*
 * linux/include/asm-armnommu/arch-isl3893/system.h
 *
 *
 */
#ifndef __ASM_ARCH_SYSTEM_H
#define __ASM_ARCH_SYSTEM_H

extern int hlt_counter;

static inline void arch_idle(void)
{
	while (!current->need_resched && !hlt_counter);
}

#endif

