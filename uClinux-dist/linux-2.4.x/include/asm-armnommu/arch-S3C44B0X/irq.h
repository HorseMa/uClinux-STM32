/*
 * include/asm-armnommu/arch-S3C44B0X/irq.h
 *
 * Copyright (c) 2003 Thomas Eschenbacher <thomas.eschenbacher@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#ifndef __ASM_ARCH_IRQ_H__
#define __ASM_ARCH_IRQ_H__

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/mach/irq.h>
#include <asm/arch/irqs.h>

extern void s3c44b0x_mask_irq(unsigned int irq);
extern void s3c44b0x_unmask_irq(unsigned int irq);
extern void s3c44b0x_mask_ack_irq(unsigned int irq);
extern void s3c44b0x_clear_pb(unsigned int irq);
extern void s3c44b0x_init_FIQ_handlers(void);

static __inline__ unsigned int fixup_irq (unsigned int irq)
{
	s3c44b0x_clear_pb(irq);
	return (irq);
}

static __inline__ void irq_init_irq(void)
{
	int irq;
	for (irq = 0; irq < NR_IRQS; irq++) {
		irq_desc[irq].valid	= 1;
		irq_desc[irq].probe_ok	= 1;
		irq_desc[irq].mask_ack	= s3c44b0x_mask_ack_irq;
		irq_desc[irq].mask	= s3c44b0x_mask_irq;
		irq_desc[irq].unmask	= s3c44b0x_unmask_irq;
	}

	/* initialize the whole FIQ subsystem */
	s3c44b0x_init_FIQ_handlers();

	/*set the priorities of the interrupts*/
	/* master priorities:
	   UART / SIO
	   DMA
	   Timer
	   EINT
	 */
	outl(0x1FD8, S3C44B0X_PMST);
	/*UART / SIO slave priorities
	 RxD0,RxD1, IIC, SIO
	 DMA slave priorities
	 BDMA0, BDMA1, ZDMA0, ZDMA1
	 Timer slave priorities
	 Timer2, Timer1, Timer3, Timer0
	 EINT slave priorities
	 default
	*/
	outl(0x1BB1D21B, S3C44B0X_PSLV);

	/* mask and disable all further interrupts */
	/* but globaly enable the INT-controller */
	outl(0x03FFFFFF, S3C44B0X_INTMSK);

	/* set all to IRQ mode, not FIQ */
	outl(0x00000000, S3C44B0X_INTMOD);

	/* clear all pending IRQ and FIQ bits */
	outl(0x07FFFFFF, S3C44B0X_I_ISPC);
	outl(0x07FFFFFF, S3C44B0X_F_ISPC);

	/* globally enable FIQs */
	asm("\
		mrs	r0, cpsr			\n\
		bic	r0, r0, #0x40	/* F_BIT */	\n\
		msr	cpsr_c, r0			\n\
	" : : : "r0");

}

#endif /* __ASM_ARCH_IRQ_H__ */
