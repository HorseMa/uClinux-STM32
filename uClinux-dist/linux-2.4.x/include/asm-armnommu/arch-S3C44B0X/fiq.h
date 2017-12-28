/*
 * include/asm-armnommu/arch-S3C3410/fiq.h
 *
 * Copyright (c) 2004 by Thomas Eschenbacher <thomas.eschenbacher@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#ifndef __ASM_ARCH_FIQ_H__
#define __ASM_ARCH_FIQ_H__

/**
 * Install a handler for a FIQ
 *
 * @param interrupt number of the interrupt source
 *        [0...NR_IRQS-1]
 * @param priority the priority of the FIQ, from 0
 *        to NR_IRQS-1, 0 is the highest priority
 * @param handler pointer to the ISR that handles the FIQ
 */
void install_fiq_handler(unsigned int interrupt,
                         unsigned int priority,
                         void (*handler)(void));

/**
 * Uninstall a FIQ handler previously installed with
 * install_fiq_handler
 *
 * @param interrupt number of the interrupt source
 *        [0...NR_IRQS-1]
 * @param priority the priority of the FIQ, from 0
 *        to NR_IRQS-1, 0 is the highest priority
 */
void uninstall_fiq_handler(unsigned int interrupt,
                           unsigned int priority);

/* offset for IRQ numbers, above 32 are used as FIQ */
#define FIQ_START 32

#endif /* __ASM_ARCH_FIQ_H__ */
/***********************************************************/
/***********************************************************/
