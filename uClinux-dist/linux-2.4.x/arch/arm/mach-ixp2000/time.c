/*
 * arch/arm/mach-ixp2000/time.c
 *
 * Original Authors: Naeem M Afzal <naeem.m.afzal@intel.com>
 *                   Jeff Daly <jeffrey.daly@intel.com
 *
 * IXP2000 timer code
 *
 * Maintainer: Deepak Saxena <dsaxena@mvista.com>
 *
 * Copyright (c) 2002-2003 Intel Corp.
 * Copyright (c) 2003 MontaVista Software, Inc.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your 
 *  option) any later version.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/smp.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>

#include <linux/timex.h>
#include <asm/hardware.h>

#include <asm/mach-types.h>


#ifdef CONFIG_LEDS_TIMER
extern void do_leds(void);
#endif

static unsigned ticks_per_jiffy;
static unsigned ticks_per_usec;

static inline unsigned long ixdp2400_ext_oscillator(void)
{
	int numerator, denominator;
	int denom_array[] = {2, 4, 8, 16, 1, 2, 4, 8};

	numerator = (*(IXDP2400_CPLD_SYS_CLK_M) & 0xFF) *2;
	denominator = denom_array[(*(IXDP2400_CPLD_SYS_CLK_N) & 0x7)];

	return   ((3125000 * numerator) / (denominator));
}

static unsigned long ixp2000_gettimeoffset (void)
{
	unsigned long elapsed;

	/* Get ticks since last perfect jiffy */
	elapsed = ticks_per_jiffy - *IXP2000_T1_CSR;

	return elapsed / ticks_per_usec;
}
static void ixp2000_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned long flags;

	/* clear timer 1 */
	ixp_reg_write(IXP2000_T1_CLR, 1);

#ifdef CONFIG_LEDS_TIMER
	do_leds();
#endif
	do_timer(regs);
}

static struct irqaction ixp2000_timer_irq = {

	handler:ixp2000_timer_interrupt,
	name:"timer tick",
};

extern unsigned long (*gettimeoffset)(void);
extern int setup_arm_irq(int, struct irqaction *);

void setup_timer (void)
{
	gettimeoffset = ixp2000_gettimeoffset;

	ixp_reg_write(IXP2000_T1_CLR, 0);

	if(machine_is_ixdp2800()) {		// Default 50MhZ APB
		ticks_per_jiffy = LATCH;
		ticks_per_usec = CLOCK_TICK_RATE / 1000000;
	} else if (machine_is_ixdp2400()) {
		ticks_per_jiffy = ((ixdp2400_ext_oscillator()/2) + HZ/2) / HZ;
		ticks_per_usec = (ixdp2400_ext_oscillator()/2) / 1000000;
	}

	ixp_reg_write(IXP2000_T1_CLD, ticks_per_jiffy);
	ixp_reg_write(IXP2000_T1_CTL, (1 << 7));

	/* register for interrupt */
	setup_arm_irq(IRQ_IXP2000_TIMER1, &ixp2000_timer_irq);
}

