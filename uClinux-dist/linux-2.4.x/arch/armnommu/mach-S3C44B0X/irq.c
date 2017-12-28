/*
 * arch/armnommu/mach-S3C44B0X/irq.c
 *
 * S3C44B0X interrupt flag and mask handling
 *
 * Copyright (c) 2003 by Thomas Eschenbacher <thomas.eschenbacher@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/arch/fiq.h>  /* for FIQ_START   */
#include <asm/arch/irqs.h> /* for IRQ numbers */
#include <linux/module.h>

#define _DISABLE_INTERRUPTS(_old_)              \
    asm volatile (                              \
	"mrs %0,cpsr;"                          \
	"mrs r4,cpsr;"                          \
	"orr r4,r4,#0x80;"                      \
	"msr cpsr,r4;"                          \
	"orr r4,r4,#0x40;"                      \
	"msr cpsr,r4"                           \
	: "=r"(_old_)                           \
	:                                       \
	: "r4"                                  \
	);

#define _RESTORE_INTERRUPTS(_old_)              \
    asm volatile (                              \
	"mrs r3,cpsr;"                          \
	"and r4,%0,#0xC0;"                      \
	"bic r3,r3,#0xC0;"                      \
	"orr r3,r3,r4;"                         \
	"msr cpsr,r3;"                          \
	:                                       \
	: "r"(_old_)                            \
	: "r3", "r4"                            \
	);

/* convert a FIQ/IRQ number to a raw IRQ number */
#define RAW_IRQ(x) ((x) & (FIQ_START-1))

/* create a bitmask from a valid raw IRQ number, for pendig/ack ... */
#define IRQ_MASK(x) (1 << RAW_IRQ(x))

void s3c44b0x_mask_irq(unsigned int irq)
{
	unsigned int _interrupts = 0;
	_DISABLE_INTERRUPTS(_interrupts);
	irq = RAW_IRQ(irq);
	if (irq >= S3C44B0X_INTERRUPT_EINT4) {
		irq = S3C44B0X_INTERRUPT_EINT4567;
	}
	
	outl( inl(S3C44B0X_INTMSK) | IRQ_MASK(irq), S3C44B0X_INTMSK);
	_RESTORE_INTERRUPTS(_interrupts);
}

void s3c44b0x_unmask_irq(unsigned int irq)
{
	unsigned int _interrupts = 0;
	_DISABLE_INTERRUPTS(_interrupts);
	irq = RAW_IRQ(irq);
	if (irq >= S3C44B0X_INTERRUPT_EINT4) {
		irq = S3C44B0X_INTERRUPT_EINT4567;
	}
	outl( inl(S3C44B0X_INTMSK) & ~IRQ_MASK(irq), S3C44B0X_INTMSK);
	_RESTORE_INTERRUPTS(_interrupts);
}

/* Clear pending bit */
void s3c44b0x_clear_pb(unsigned int irq)
{
	unsigned int raw_irq = RAW_IRQ(irq);

	if (raw_irq >= S3C44B0X_INTERRUPT_EINT4) {
		outl( inl(S3C44B0X_EXTINPND) |
			(1 << (raw_irq - S3C44B0X_INTERRUPT_EINT4)),
			S3C44B0X_EXTINPND);
		raw_irq = S3C44B0X_INTERRUPT_EINT4567;
	}
	if (irq <= FIQ_START)
		outl(1 << raw_irq, S3C44B0X_I_ISPC); /* IRQ mode */
	else
		outl(1 << raw_irq, S3C44B0X_F_ISPC); /* FIQ mode */
}

void s3c44b0x_mask_ack_irq(unsigned int irq)
{
	s3c44b0x_mask_irq(irq);
	s3c44b0x_clear_pb(irq);
}

EXPORT_SYMBOL(s3c44b0x_mask_irq);
EXPORT_SYMBOL(s3c44b0x_unmask_irq);
EXPORT_SYMBOL(s3c44b0x_clear_pb);
EXPORT_SYMBOL(s3c44b0x_mask_ack_irq);
