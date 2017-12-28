/*
 * include/asm-armnommu/arch-S3C44B0X/hardware.h
 *
 * Copyright (c) 2003 by Thomas Eschenbacher <thomas.eschenbacher@gmx.de>
 *
 * Hardware definitions of MBA44 board, currently only
 * the CPUs registers and definitions are included from
 * s3c44b0x.h.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#ifndef __ASM_ARCH_HARDWARE_H
#define __ASM_ARCH_HARDWARE_H

#include <linux/config.h>
#include <asm/arch/s3c44b0x.h>

#ifndef __ASSEMBLER__

/* forward declaration to arch/armnommu/mach-S3C44B0X/arch.h */
extern void arch_hard_reset(void);

#define HARD_RESET_NOW()  { arch_hard_reset(); }

#define BOARD_ARM_CLOCK		arch_cpu_clock()

/* Returns the CPU clock in Hz */
unsigned int arch_cpu_clock(void);

#endif /* ! __ASSEMBLER__ */

#endif /* __ASM_ARCH_HARDWARE_H */
