/*
 * include/linux/asm-frv/hrtime.h
 *
 * Based on asm-i386/hrtime.h
 *
 *    2003-7-7  Posix Clocks & timers 
 *                           by George Anzinger george@mvista.com
 *
 *			     Copyright (C) 2002 2003, 2004 by MontaVista Software.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * MontaVista Software | 1237 East Arques Avenue | Sunnyvale | CA 94085 | USA 
 */
#ifndef _FRV_HRTIME_H
#define _FRV_HRTIME_H
#ifdef __KERNEL__

#include <linux/config.h>	/* for CONFIG_APM etc... */
#include <asm/types.h>		/* for u16s */
#include <linux/init.h>
#include <asm/io.h>
#include <linux/sc_math.h>	/* scaling math routines */
#include <asm/delay.h>
#include <asm/smp.h>
#include <linux/timex.h>
/*
 * We always want the timer, if not touched otherwise, to give periodic
 * 1/HZ interrupts.  This is done by programing the interrupt we want
 * and, once it it loaded, (in the case of the PIT) dropping a 1/HZ
 * program on top of it.  For other timers, other strategies are used,
 * such as programming a 1/HZ interval on interrupt.  The The PIT will
 * give us the desired interrupt and, at interrupt time, load the 1/HZ
 * program.  So...

 * If no sub 1/HZ ticks are needed AND we are aligned with the 1/HZ 
 * boundry, we don't need to touch the PIT.  Otherwise we do the above.

 * There are two reasons to keep this:
 * 1. The NMI watchdog uses the timer interrupt to generate the NMI interrupts.
 * 2. We don't have to touch the PIT unless we have a sub jiffie event in
 *    the next 1/HZ interval (unless we drift away from the 1/HZ boundry).
 */

/*
 * The high-res-timers option is set up to self configure with different 
 * platforms.  It is up to the platform to provide certian macros which
 * override the default macros defined in system without (or with disabled)
 * high-res-timers.
 *
 * To do high-res-timers at some fundamental level the timer interrupt must
 * be seperated from the time keeping tick.  A tick can still be generated
 * by the timer interrupt, but it may be surrounded by non-tick interrupts.
 * It is up to the platform to determine if a particular interrupt is a tick,
 * and up to the timer code (in timer.c) to determine what time events have
 * expired.
 *
 * Details on the functions and macros can be found in the file:
 * include/hrtime.h
 */
struct timer_conversion_bits {
	unsigned long _arch_to_usec;
	unsigned long _arch_to_nsec;
	unsigned long _usec_to_arch;
	unsigned long _nsec_to_arch;
	long _arch_cycles_per_jiffy;
	unsigned long _arch_to_latch;
	unsigned long volatile _last_update;
};
extern struct timer_conversion_bits timer_conversion_bits;
/*
 * The following four values are not used for machines 
 * without a TSC.  For machines with a TSC they
 * are caculated at boot time. They are used to 
 * calculate "cycles" to jiffies or usec.  Don't get
 * confused into thinking they are simple multiples or
 * divisors, however.  
 */
#define arch_to_usec timer_conversion_bits._arch_to_usec
#define arch_to_nsec timer_conversion_bits._arch_to_nsec
#define usec_to_arch timer_conversion_bits._usec_to_arch
#define nsec_to_arch timer_conversion_bits._nsec_to_arch

#define arch_cycles_per_jiffy timer_conversion_bits._arch_cycles_per_jiffy
/*#define arch_to_latch timer_conversion_bits._arch_to_latch*/
#define last_update timer_conversion_bits._last_update

/*
 * no of usecs less than which events cannot be scheduled
 */
#define TIMER_DELTA  50
#ifdef _INCLUDED_FROM_TIME_C
// EXPORT_SYMBOL(timer_conversion_bits);
#define EXTERN
int timer_delta = TIMER_DELTA;
int hr_time_resolution;
#else
#define EXTERN  extern
extern int timer_delta;
extern int hr_time_resolution;
#endif

/*
 * Interrupt generators need to be disciplined to generate the interrupt
 * on the 1/HZ boundry (assuming we don't need sub_jiffie interrupts) if
 * the timer clock is other than the interrupt generator clock.  In the
 * FR-V case we use the same clock source so no discipine is needed.
 */
#define IF_DISCIPLINE(x)

EXTERN int _last_was_long;
#define __last_was_long  _last_was_long

#define USEC_PER_JIFFIES  (1000000/HZ)

#define schedule_hr_timer_int(a,b)  _schedule_next_int(a,b)
#define schedule_jiffies_int(a) _schedule_jiffies_int(a)

extern spinlock_t tm0_lock;
extern rwlock_t xtime_lock;
extern volatile unsigned long __attribute__((section(".sdata"))) jiffies;
extern u64 __attribute__((section(".sdata"))) jiffies_64;

extern int _schedule_next_int(unsigned long jiffie_f, long sub_jiffie_in);
extern int _schedule_jiffies_int(unsigned long jiffie_f);

extern unsigned int   __arch_latch;
extern unsigned long  __arch_timer_clock_hz;

#define cpuctr_pre   5
#define cpuctr_hz    (__arch_timer_clock_hz)
#define cpuctr_shift (cpuctr_pre - 4)

/*
 * The spec says the counter can be either 32 or 24 bits wide.
 */
#define SIZE_MASK 0x1ffff

extern inline unsigned long
quick_get_cpuctr(void)
{
    	unsigned cur;

	__set_TCTR(TCTR_SC_CTR1 | TCTR_RL_LATCH);
	cur = __get_TCSR_DATA(1);
	cur |= __get_TCSR_DATA(1) << 8;
	cur = 0xffff - cur;

	return ((cur << cpuctr_shift) - last_update) & SIZE_MASK;
}

/*
 * This function moves the last_update value to the closest to the
 * current 1/HZ boundry.  This MUST be called under the write xtime_lock.
 */
extern inline unsigned long
stake_cpuctr(void)
{

	if ((unsigned)quick_get_cpuctr() > arch_cycles_per_jiffy) {
		last_update = (last_update + arch_cycles_per_jiffy) & SIZE_MASK;
	}
}

#define arch_hrtime_init(x) (x)

/* 
 * We use various scaling.  The sc32 scales by 2**32, sc_n by the first
 * parm.  When working with constants, choose a scale such that
 * x/n->(32-scale)< 1/2.  So for 1/3 <1/2 so scale of 32, where as 3/1
 * must be shifted 3 times (3/8) to be less than 1/2 so scale should be
 * 29
 *
 */
#define HR_SCALE_ARCH_NSEC 22
#define HR_SCALE_NSEC_ARCH 32
#define HR_SCALE_USEC_ARCH 29

extern inline int
arch_cycle_to_usec(unsigned long update)
{
	return (mpy_sc32(update, arch_to_usec));
}

extern inline int
arch_cycle_to_latch(unsigned long update)
{
	return update >> cpuctr_shift;
}

extern inline int
arch_cycle_to_nsec(long update)
{
	return mpy_sc_n(HR_SCALE_ARCH_NSEC, update, arch_to_nsec);
}
/* 
 * And the other way...
 */
extern inline int
usec_to_arch_cycle(unsigned long usec)
{
	return mpy_sc_n(HR_SCALE_USEC_ARCH, usec, usec_to_arch);
}
extern inline int
nsec_to_arch_cycle(unsigned long nsec)
{
	return mpy_sc32(nsec, nsec_to_arch);
}

#ifdef _INCLUDED_FROM_TIME_C
struct timer_conversion_bits timer_conversion_bits;

/*
 * After we get the address, we set last_update to the current timer value
 */
static inline __init void  init_hrtimers(void)
{
	arch_cycles_per_jiffy = (cpuctr_hz + HZ / 2) / HZ;
	hr_time_resolution = 10000;
	arch_to_usec = SC_32(100000000)/(long long)(cpuctr_hz * 100);
	arch_to_nsec = SC_n(HR_SCALE_ARCH_NSEC,100000000000LL)/ 
	    		(long long)(cpuctr_hz * 100);
	usec_to_arch = SC_n( HR_SCALE_USEC_ARCH,cpuctr_hz * 100)/
			(long long)100000000;
	nsec_to_arch = SC_n( HR_SCALE_NSEC_ARCH,cpuctr_hz)/ \
			(long long)1000000000;
	
	__set_TxCKSL_DATA(1, cpuctr_pre);
	__set_TCTR(TCTR_SC_CTR1 | TCTR_RL_RW_LH8 | TCTR_MODE_2);
	__set_TCSR_DATA(1, 0);
	__set_TCSR_DATA(1, 0);
	last_update = 0;
}

#endif  /* _INCLUDED_FROM_TIME_C_ */


	/*
	 * We program the PIT to give periodic interrupts and, if no
	 * sub_jiffie timers are due, leave it alone.  This means that
	 * it can drift WRT the clock (TSC or pm timer).  What we are
	 * trying to do is to program the next interrupt to occure on
	 * exactly the requested time.  If we are not doing sub HZ
	 * interrupts we expect to find a small excess of time beyond
	 * the 1/HZ, i.e. _sub_jiffie will have some small value.  This
	 * value will drift AND may jump upward from time to time.  The
	 * drift is due to not having precise tracking between the two
	 * timers (the PIT and either the TSC or the PM timer) and the
	 * jump is caused by interrupt delays, cache misses etc.  We
	 * need to correct for the drift.  To correct all we need to do
	 * is to set "last_was_long" to zero and a new timer program
	 * will be started to "do the right thing".

	 * Detecting the need to do this correction is another issue.
	 * Here is what we do:
	 *
	 * Each interrupt where last_was_long is !=0 (indicates the
	 * interrupt should be on a 1/HZ boundry) we check the resulting
	 * _sub_jiffie.  If it is smaller than some MIN value, we do the
	 * correction.  (Note that drift that makes the value smaller is
	 * the easy one.)  We also require that _sub_jiffie <= some max
	 * at least once over a period of 1 second.  I.e.  with HZ =
	 * 100, we will allow up to 99 "late" interrupts before we do a
	 * correction.

	 * The values we use for min_hz_sub_jiffie and max_hz_sub_jiffie
	 * depend on the units and we will start by, during boot,
	 * observing what MIN appears to be.  We will set
	 * max_hz_sub_jiffie to be about 100 machine cycles more than
	 * this.

	 * Note that with min_hz_sub_jiffie and max_hz_sub_jiffie set to
	 * 0, this code will reset the PIT every HZ.
	 */
#define discipline_timer(a)

/*
 * This routine is always called under the write_lockirq(xtime_lock)
 * We provide something like a sequence lock for local use.  We need to
 * keep cycles, sub_jiffie and jiffie aligned (be nice to get 
 * wall_to_monotonic, but, sigh, another day).
 */

extern inline long
get_arch_cycles(unsigned long ref) 
{
	return (long)(jiffies - ref) * arch_cycles_per_jiffy + quick_get_cpuctr();
}

extern inline void
reload_timer_chip(int new_latch_value)
{
	unsigned char pit_status;

	/*
	 * The input value is in arch cycles
	 * We must be called with irq disabled.
	 */
	new_latch_value = arch_cycle_to_latch(new_latch_value);
	if (new_latch_value < TIMER_DELTA) {
		new_latch_value = TIMER_DELTA;
	}
	spin_lock(&tm0_lock);
	__set_TCTR(TCTR_SC_CTR0 | TCTR_RL_RW_LH8 | TCTR_MODE_2);
	__set_TCSR_DATA(0, new_latch_value & 0xff);
	__set_TCSR_DATA(0, new_latch_value >> 8);
	do {
		__set_TCTR(TCTR_SC_READBACK | TCTR_CNT0);
		pit_status = __get_TCSR_DATA(0);
	} while (pit_status & TCSRx_NULLCOUNT);
	__set_TCSR_DATA(0, __arch_latch & 0xff);
	__set_TCSR_DATA(0, __arch_latch >> 8);
	spin_unlock(&tm0_lock);
	return;
}

#define final_clock_init()
#endif				/* __KERNEL__ */
#endif				/* _I386_HRTIME_H */
