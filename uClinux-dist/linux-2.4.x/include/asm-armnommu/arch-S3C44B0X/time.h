/*
 * include/asm-armnommu/arch-S3C44B0X/time.h
 *
 * Copyright (c) 2003 by Thomas Eschenbacher <thomas.eschenbacher@gmx.de>
 *
 * Setup for 16 bit timer 5, used as system timer.
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

extern unsigned long s3c44b0x_gettimeoffset(void);
extern void s3c44b0x_timer_interrupt(int irq, void *dev_id,
                                     struct pt_regs *regs);
extern void s3c44b0x_unmask_irq(unsigned int irq);

/* prescaler divisor = 16 */
#define S3C44B0X_TIMER5_PRESCALER 16

/* multiplexer divisor = 2 */
#define S3C44B0X_TIMER5_MUX 2

extern __inline__ void setup_timer (void)
{
	u_int32_t tmod;
	u_int32_t period;

	period = ((CONFIG_ARM_CLK / S3C44B0X_TIMER5_PRESCALER) /
	           S3C44B0X_TIMER5_MUX) / HZ;
	outl(period, S3C44B0X_TCNTB5);

	tmod=inl(S3C44B0X_TCON);
	tmod &= ~(0x7 << 24);
	tmod |= S3C44B0X_TCON_T5_MAN_UPDATE;
	outl(tmod, S3C44B0X_TCON);

	/* Timer 4 & 5 prescaler */
	outl(inl(S3C44B0X_TCFG0) &~ (0xFF << 16), S3C44B0X_TCFG0);
	outl(inl(S3C44B0X_TCFG0) | ((S3C44B0X_TIMER5_PRESCALER-1) << 16),
	                           S3C44B0X_TCFG0);

	/* Timer 5 MUX = 1/2 */
	outl(inl(S3C44B0X_TCFG1) &~ (0xF << 20), S3C44B0X_TCFG1);

	/*
	 * @todo do those really need to be function pointers ?
	 */
	gettimeoffset     = s3c44b0x_gettimeoffset;
	timer_irq.handler = s3c44b0x_timer_interrupt;

	/* set up the interrupt vevtor for timer 5 match */
	setup_arm_irq(S3C44B0X_INTERRUPT_TIMER5, &timer_irq);

	/* enable the timer IRQ */
	s3c44b0x_unmask_irq(S3C44B0X_INTERRUPT_TIMER5);

	/* let timer 5 run... */
	tmod = inl(S3C44B0X_TCON);
	tmod &= ~(0x7 << 24);
	tmod |= S3C44B0X_TCON_T5_AUTO;
	tmod |= S3C44B0X_TCON_T5_START;

	outl(tmod, S3C44B0X_TCON);
}

#endif /* __ASM_ARCH_TIME_H__ */
