/*
 *  linux/arch/m68k/kernel/time.c
 *
 *  Copyright (C) 1991, 1992, 1995  Linus Torvalds
 *
 * This file contains the m68k-specific time handling details.
 * Most of the stuff is located in the machine specific files.
 *
 * 1997-09-10	Updated NTP code according to technical memorandum Jan '96
 *		"A Kernel Model for Precision Timekeeping" by Dave Mills
 */

/* 2002-8-13 George Anzinger  Modified for High res timers: 
 *                            Copyright (C) 2002 MontaVista Software
 */
#define _INCLUDED_FROM_TIME_C
#include <linux/config.h> /* CONFIG_HEARTBEAT */
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/mm.h>

#include <asm/io.h>
#include <asm/timer-regs.h>
#include <asm/mb-regs.h>
#include <asm/irc-regs.h>
#include <asm/mb86943a.h>
#include <asm/irq-routing.h>

#include <linux/timex.h>

unsigned long __nongprelbss __clkin_clock_speed_HZ;
unsigned long __nongprelbss __ext_bus_clock_speed_HZ;
unsigned long __nongprelbss __res_bus_clock_speed_HZ;
unsigned long __nongprelbss __sdram_clock_speed_HZ;
unsigned long __nongprelbss __core_bus_clock_speed_HZ;
unsigned long __nongprelbss __core_clock_speed_HZ;
unsigned long __nongprelbss __dsu_clock_speed_HZ;
unsigned long __nongprelbss __serial_clock_speed_HZ;
unsigned long __delay_loops_MHz;

unsigned int   __arch_latch;
unsigned long  __arch_timer_clock_hz;

static void timer_interrupt(int irq, void *dummy, struct pt_regs * regs);

static struct irqaction timer_irq  = {
	timer_interrupt, SA_INTERRUPT, 0, "timer", NULL, NULL
};

static inline int set_rtc_mmss(unsigned long nowtime)
{
	return -1;
}

static inline void do_profile (unsigned long pc)
{
	if (prof_buffer && current->pid) {
		pc -= (unsigned long) &_stext;
		pc >>= prof_shift;
		if (pc < prof_len)
			++prof_buffer[pc];
		else
		/*
		 * Don't ignore out-of-bounds PC values silently,
		 * put them into the last histogram slot, so if
		 * present, they will show up as a sharp peak.
		 */
			++prof_buffer[prof_len-1];
	}
}

/*
 * timer_interrupt() needs to keep up the real-time clock,
 * as well as call the "do_timer()" routine every clocktick
 */
static void timer_interrupt(int irq, void *dummy, struct pt_regs * regs)
{
	/* last time the cmos clock got updated */
	static long last_rtc_update = 0;

	/* may need to kick the hardware timer */
	do_timer(regs);

	if (!user_mode(regs))
		do_profile(regs->pc);

	/*
	 * If we have an externally synchronized Linux clock, then update
	 * CMOS clock accordingly every ~11 minutes. Set_rtc_mmss() has to be
	 * called as close as possible to 500 ms before the new second starts.
	 */
	if ((time_status & STA_UNSYNC) == 0 &&
	    xtime.tv_sec > last_rtc_update + 660 &&
	    xtime.tv_usec >= 500000 - ((unsigned) tick) / 2 &&
	    xtime.tv_usec <= 500000 + ((unsigned) tick) / 2) {
		if (set_rtc_mmss(xtime.tv_sec) == 0)
			last_rtc_update = xtime.tv_sec;
		else
			last_rtc_update = xtime.tv_sec - 600; /* do it again in 60 s */
	}

#ifdef CONFIG_HEARTBEAT
	static unsigned short n;
	n++;
	__set_LEDS(n);
#endif /* CONFIG_HEARTBEAT */
}

void time_divisor_init(void)
{
	unsigned short pre, prediv;

	/* set the scheduling timer going */
	pre = 1;
	prediv = 4;

	__arch_timer_clock_hz = __res_bus_clock_speed_HZ / pre / (1 << prediv);
	__arch_latch = __arch_timer_clock_hz / HZ;

	__set_TPRV(pre);
	__set_TxCKSL_DATA(0, prediv);
	__set_TCTR(TCTR_SC_CTR0 | TCTR_RL_RW_LH8 | TCTR_MODE_2);
	__set_TCSR_DATA(0, __arch_latch & 0xff);
	__set_TCSR_DATA(0, __arch_latch >> 8);
}

void time_init(void)
{
	unsigned int year, mon, day, hour, min, sec;

	extern void arch_gettod(int *year, int *mon, int *day, int *hour, int *min, int *sec);

	/* FIX by dqg : Set to zero for platforms that don't have tod */
	/* without this time is undefined and can overflow time_t, causing  */
	/* very stange errors */
	year = 1980;
	mon = day = 1;
	hour = min = sec = 0;
	arch_gettod (&year, &mon, &day, &hour, &min, &sec);

	if ((year += 1900) < 1970)
		year += 100;
	xtime.tv_sec = mktime(year, mon, day, hour, min, sec);
	xtime.tv_usec = 0;

	time_divisor_init();
	/* install scheduling interrupt handler */
	setup_irq(IRQ_CPU_TIMER0, &timer_irq);
}

extern rwlock_t xtime_lock;
extern u64 __attribute__((section(".sdata"))) jiffies_64;

static unsigned long do_gettimeoffset(void)
{
	int count;
	static int first_time = 1;
	static int count_p;
	static unsigned long jiffies_p = 0;

	if (first_time) {
	    first_time = 0;
	    count_p = __arch_latch;    /* for the first call after boot */
	}

	/*
	 * cache volatile jiffies temporarily; we have IRQs turned off. 
	 */
	unsigned long jiffies_t;

	/* timer count may underflow right here */
	__set_TCTR(TCTR_SC_CTR0 | TCTR_RL_LATCH);  /* latch the count ASAP */

	count = __get_TCSR_DATA(0);	/* read the latched count */

 	jiffies_t = jiffies;

	count |= __get_TCSR_DATA(0) << 8;
	
	/*
	 * avoiding timer inconsistencies (they are rare, but they happen)...
	 */

	if( jiffies_t == jiffies_p ) {
		if( count > count_p ) {
			/* the nutcase */

			if (__get_IRL() == IRQ_TIMER0_LEVEL) {
				/*
				 * We cannot detect lost timer interrupts ... 
				 * well, that's why we call them lost, don't we? :)
				 * [hmm, on the Pentium and Alpha we can ... sort of]
				 */
				count -= __arch_latch;
			} else {
				printk("do_gettimeoffset(): hardware timer problem?\n");
			}
		}
	} else
		jiffies_p = jiffies_t;

	count_p = count;

	count = ((__arch_latch-1) - count) * tick;
	count = (count + __arch_latch/2) / __arch_latch;

	return count;
}

/*
 * This version of gettimeofday has near microsecond resolution.
 */
void do_gettimeofday(struct timeval *tv)
{
	extern volatile unsigned long wall_jiffies;
	unsigned long flags;
	unsigned long usec, sec, lost;

	read_lock_irqsave(&xtime_lock, flags);
	usec = do_gettimeoffset();
	lost = jiffies - wall_jiffies;
	if (lost)
		usec += lost * (1000000/HZ);
	sec = xtime.tv_sec;
	usec += xtime.tv_usec;
	read_unlock_irqrestore(&xtime_lock, flags);

	while (usec >= 1000000) {
		usec -= 1000000;
		sec++;
	}

	tv->tv_sec = sec;
	tv->tv_usec = usec;
}

void do_settimeofday(struct timeval *tv)
{
	write_lock_irq(&xtime_lock);
	/* This is revolting. We need to set the xtime.tv_usec
	 * correctly. However, the value in this location is
	 * is value at the last tick.
	 * Discover what correction gettimeofday
	 * would have done, and then undo it!
	 */
	while (tv->tv_usec < 0) {
		tv->tv_usec += 1000000;
		tv->tv_sec--;
	}

	xtime = *tv;
	time_adjust = 0;		/* stop active adjtime() */
	time_status |= STA_UNSYNC;
	time_maxerror = NTP_PHASE_LIMIT;
	time_esterror = NTP_PHASE_LIMIT;
	write_unlock_irq(&xtime_lock);
}
