/*
 * uclinux/include/asm-armnommu/arch-S3C3410/fiq.h
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
void install_fiq_handler(unsigned int interrupt, 
                         void (*handler)(void), 
                         unsigned char new_prio);
unsigned char get_highest_irq_prio(void);


#endif /* __ASM_ARCH_FIQ_H__ */
