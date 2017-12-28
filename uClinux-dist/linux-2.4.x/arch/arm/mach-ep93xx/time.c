/*
 *  linux/arch/arm/mach-ep93xx/time.c
 *
 *  Copyright (C) 2000-2001 Deep Blue Solutions
 *
 * (c) Copyright 2001 LynuxWorks, Inc., San Jose, CA.  All rights reserved.
 *  Copyright (C) 2002-2003 Cirrus Logic, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>

#include <asm/hardware.h>
#include <asm/io.h>

extern unsigned long (*gettimeoffset)(void);

/*
 * gettimeoffset() returns time since last timer tick, in usecs.
 *
 * 'LATCH' is hwclock ticks (see CLOCK_TICK_RATE in timex.h) per jiffy.
 * 'tick' is usecs per jiffy.
 */
static unsigned long ep93xx_gettimeoffset(void)
{
	unsigned long hwticks;
	hwticks = LATCH - (inl(TIMER1VALUE) & 0xffff);
	return ((hwticks * tick) / LATCH);
}

void __init ep93xx_setup_timer(void)
{
	gettimeoffset = ep93xx_gettimeoffset;

	outl(0, TIMER1CONTROL);
	outl(LATCH - 1, TIMER1LOAD);
	outl(0xc8, TIMER1CONTROL);

	xtime.tv_sec = inl(RTCDR);
}
