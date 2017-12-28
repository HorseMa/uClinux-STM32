/*--------------------------------------------------------------------
 *
 * arch/nios2nommu/kernel/time.c
 *
 * Architecture specific time handling details.
 *
 * Derived from various works, Alpha, ix86, M68K, Sparc, ...et al
 *
 * Most of the stuff is located in the machine specific files.
 *
 * Copyright (C) 2004   Microtronix Datacom Ltd
 *  Copyright (C) 1998-2000  D. Jeff Dionne <jeff@lineo.ca>,
 *                           Kenneth Albanowski <kjahds@kjahds.com>,
 *  Copyright (C) 1991, 1992, 1995  Linus Torvalds
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
 * Jan/20/2004		dgt	    NiosII
 *
 ---------------------------------------------------------------------*/


#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/timex.h>
#include <linux/profile.h>
#include <linux/module.h>
#include <linux/irq.h>

#include <asm/segment.h>
#include <asm/io.h>
#include <asm/nios.h>

#define	TICK_SIZE (tick_nsec / 1000)

unsigned long cpu_khz;
static inline int set_rtc_mmss(unsigned long nowtime)
{
  return 0;
}

/* Timer timeout status */
#define nios2_timer_TO	(inw(&na_timer0->np_timerstatus) & np_timerstatus_to_mask)

/* Timer snapshot */
static inline unsigned long nios2_read_timercount(void)
{
	unsigned long count;

	outw(0, &na_timer0->np_timersnapl);
	count = inw(&na_timer0->np_timersnaph) << 16 | inw(&na_timer0->np_timersnapl);

	return count;
}

/*
 * Should return useconds since last timer tick
 */
static unsigned long gettimeoffset(void)
{
	unsigned long offset;
	unsigned long count;

	count = nios2_read_timercount();
	offset = ((nasys_clock_freq/HZ)-1 - nios2_read_timercount()) \
		 / (nasys_clock_freq / USEC_PER_SEC);

	/* Check if we just wrapped the counters and maybe missed a tick */
	if (nios2_timer_TO  && (offset < (100000 / HZ / 2)))
		offset += (USEC_PER_SEC / HZ);

	return offset;
}

/*
 * timer_interrupt() needs to keep up the real-time clock,
 * as well as call the "do_timer()" routine every clocktick
 */
irqreturn_t timer_interrupt(int irq, void *dummy)
{
	/* last time the cmos clock got updated */
	static long last_rtc_update=0;
	
	write_seqlock(&xtime_lock);
	na_timer0->np_timerstatus = 0; /* Clear the interrupt condition */

	do_timer(1);
	profile_tick(CPU_PROFILING);
	/*
	 * If we have an externally synchronized Linux clock, then update
	 * CMOS clock accordingly every ~11 minutes. Set_rtc_mmss() has to be
	 * called as close as possible to 500 ms before the new second starts.
	 */
	if (ntp_synced() &&
	    xtime.tv_sec > last_rtc_update + 660 &&
	    (xtime.tv_nsec / 1000) >= 500000 - ((unsigned) TICK_SIZE) / 2 &&
	    (xtime.tv_nsec  / 1000) <= 500000 + ((unsigned) TICK_SIZE) / 2) {
	  if (set_rtc_mmss(xtime.tv_sec) == 0)
	    last_rtc_update = xtime.tv_sec;
	  else
	    last_rtc_update = xtime.tv_sec - 600; /* do it again in 60 s */
	}

	write_sequnlock(&xtime_lock);

#ifndef CONFIG_SMP
	update_process_times(user_mode(get_irq_regs()));
#endif

	return(IRQ_HANDLED);
}

void __init time_init(void)
{
	unsigned int year, mon, day, hour, min, sec;
	int err;

	extern void arch_gettod(int *year, int *mon, int *day, int *hour,
				int *min, int *sec);
	
	cpu_khz=nasys_clock_freq_1000;
	arch_gettod(&year, &mon, &day, &hour, &min, &sec);

	if ((year += 1900) < 1970)
		year += 100;
	xtime.tv_sec = mktime(year, mon, day, hour, min, sec);
	xtime.tv_nsec = 0;
	wall_to_monotonic.tv_sec = -xtime.tv_sec;

	err = request_irq(na_timer0_irq, timer_interrupt, IRQ_FLG_LOCK, "timer", NULL);
	if(err)
		printk(KERN_ERR "%s() failed - errno = %d\n", __FUNCTION__, -err);
	na_timer0->np_timerperiodl = (nasys_clock_freq/HZ)-1;
	na_timer0->np_timerperiodh = ((nasys_clock_freq/HZ)-1) >> 16;

	/* interrupt enable + continuous + start */
	na_timer0->np_timercontrol = np_timercontrol_start_mask
				   + np_timercontrol_cont_mask
				   + np_timercontrol_ito_mask;
}
