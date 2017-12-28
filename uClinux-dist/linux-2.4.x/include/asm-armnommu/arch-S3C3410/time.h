/*
 * include/asm-armnommu/arch-S3C3410/time.h
 *
 * Copyright (c) 2003 by Thomas Eschenbacher <thomas.eschenbacher@gmx.de>
 *
 * Setup for 16 bit timer 1, used as system timer.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#ifndef __ASM_ARCH_TIME_H__
#define __ASM_ARCH_TIME_H__

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/hardware.h>
#include <asm/arch/timex.h>

extern struct irqaction timer_irq;

extern unsigned long s3c3410_gettimeoffset(void);
extern void s3c3410_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs);
extern void s3c3410_unmask_irq(unsigned int irq);

#define S3C3410X_TIMER1_PRESCALER 100

extern __inline__ void setup_timer (void)
{
	u_int8_t tmod;
	u_int16_t period;

	/*
	 * disable and clear timer 1, set to
	 * internal clock and interval mode
	 */
	tmod = S3C3410X_T16_OMS_INTRV | S3C3410X_T16_CL;
	outb(tmod, S3C3410X_TCON1);

	/* initialize the timer period and prescaler */
	period = (CONFIG_ARM_CLK/S3C3410X_TIMER1_PRESCALER)/HZ;
	outw(period, S3C3410X_TDAT1);
	outb(S3C3410X_TIMER1_PRESCALER-1, S3C3410X_TPRE1);

	/*
	 * @todo do those really need to be function pointers ?
	 */
	gettimeoffset     = s3c3410_gettimeoffset;
	timer_irq.handler = s3c3410_timer_interrupt;

	/* set up the interrupt vevtor for timer 1 match */
	setup_arm_irq(S3C3410X_INTERRUPT_TMC1, &timer_irq);

	/* enable the timer IRQ */
	s3c3410_unmask_irq(S3C3410X_INTERRUPT_TMC1);

	/* let timer 1 run... */
	tmod |= S3C3410X_T16_TEN;
	tmod &= ~S3C3410X_T16_CL;
	outb(tmod, S3C3410X_TCON1);
}

#endif /* __ASM_ARCH_TIME_H__ */
