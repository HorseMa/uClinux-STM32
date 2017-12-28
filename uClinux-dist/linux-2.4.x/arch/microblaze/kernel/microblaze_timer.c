/*
 * arch/microblaze/kernel/microblaze_timer.c
 *
 *  Copyright (C) 2001,2002  NEC Corporation
 *  Copyright (C) 2001,2002  Miles Bader <miles@gnu.org>
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License.  See the file COPYING in the main directory of this
 * archive for more details.
 *
 * Written by Miles Bader <miles@gnu.org>
 */

#include <linux/kernel.h>
#include <linux/config.h>
#include <asm/machdep.h>		/* Get timer base addr etc */
#include <asm/microblaze_timer.h>
#include <asm/autoconfig.h>		/* Back compatability supprt for old targets */

/* Start interval timer TIMER (0-3).  The timer will issue the
   corresponding INTCMD interrupt RATE times per second.
   This function does not enable the interrupt.  */
void microblaze_timer_configure (unsigned timer, unsigned rate)
{
	unsigned count;

	/* 
	 * Work out timer compare value for a given clock freq 
	 * and interrupt rate (see opb_timer datasheet page 3)
	 */
	count = ((1ULL << CONFIG_XILINX_TIMER_0_COUNT_WIDTH)-1ULL) - \
			CONFIG_XILINX_CPU_CLOCK_FREQ / rate + 2;

	/* Do the actual hardware timer initialization:  */

	/* Enable timer, enable interrupt generation, and put into count up mode */
	/* Set the compare counter */
	timer_set_compare(CONFIG_XILINX_TIMER_0_BASEADDR, timer, count);

	/* Reset timer and clear interrupt */
	timer_set_csr(CONFIG_XILINX_TIMER_0_BASEADDR, timer, 
			TIMER_INTERRUPT | TIMER_RESET);

	/* start the timer */
	timer_set_csr(CONFIG_XILINX_TIMER_0_BASEADDR, timer, 
			TIMER_ENABLE | TIMER_ENABLE_INTR | TIMER_RELOAD);

}
