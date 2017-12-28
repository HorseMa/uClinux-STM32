/*
 * include/asm-armnommu/arch-S3C44B0X/system.h
 *
 * Copyright (c) 2003 by Thomas Eschenbacher <thomas.eschenbacher@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#ifndef __ASM_ARCH_SYSTEM_H_
#define __ASM_ARCH_SYSTEM_H_

static inline void arch_idle(void)
{
	while (!current->need_resched && !hlt_counter)
		cpu_do_idle(IDLE_WAIT_SLOW);
}

/* forward declaration to arch/armnommu/mach-S3C44B0X/arch.h */
extern void arch_hard_reset(void);

void arch_reset(char mode)
{
	(void)mode; /* unused */

	/* we only support "hard" reset */
	arch_hard_reset();
}

#endif /* __ASM_ARCH_SYSTEM_H_ */
