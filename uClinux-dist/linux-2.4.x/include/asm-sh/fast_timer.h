#ifndef __ASM_SH_FAST_TIMER_H
#define __ASM_SH_FAST_TIMER_H

#include <linux/sched.h>
#include <asm/io.h>

#define FAST_POLL_INTR

#define FASTTIMER_IRQ   17
#define FASTTIMER_IPR_ADDR  INTC_IPRA
#define FASTTIMER_IPR_POS    2
#define FASTTIMER_PRIORITY   3

#ifdef FAST_POLL_INTR
#define TMU1_TCR_INIT	0x0020
#else
#define TMU1_TCR_INIT	0
#endif
#define TMU_TSTR_INIT	1
#define TMU1_TCR_CALIB	0x0000
#define TMU_TOCR	0xffd80000	/* Byte access */
#define TMU_TSTR	0xffd80004	/* Byte access */
#define TMU1_TCOR	0xffd80014	/* Long access */
#define TMU1_TCNT	0xffd80018	/* Long access */
#define TMU1_TCR	0xffd8001c	/* Word access */

static void fast_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned long timer_status;

	timer_status = ctrl_inw(TMU1_TCR);
	timer_status &= ~0x100;
	ctrl_outw(timer_status, TMU1_TCR);

	do_fast_timer();
}

static void fast_timer_set(void)
{
	unsigned long interval;

#ifdef FAST_POLL_INTR
	interval = (current_cpu_data.module_clock/4 + fast_timer_rate/2) / fast_timer_rate;
#else
	interval = 0xffffffff;
#endif

	ctrl_outb(ctrl_inb(TMU_TSTR) & ~0x2, TMU_TSTR); /* disable timer 1 */
	ctrl_outw(TMU1_TCR_INIT, TMU1_TCR);
	ctrl_outl(interval, TMU1_TCOR);
	ctrl_outl(interval, TMU1_TCNT);
	ctrl_outb(ctrl_inb(TMU_TSTR) | 0x2, TMU_TSTR); /* enable timer 1 */
}

static int __init fast_timer_setup(void)
{
#ifdef FAST_POLL_INTR
	make_ipr_irq(FASTTIMER_IRQ, FASTTIMER_IPR_ADDR, FASTTIMER_IPR_POS,
			FASTTIMER_PRIORITY);

	if (request_irq(FASTTIMER_IRQ, fast_timer_interrupt, SA_INTERRUPT,
			"fast timer", NULL))
		return -EBUSY;
#endif

	fast_timer_rate = cpu_data->type == CPU_SH7751R ? 2000 : 1000;
	fast_timer_set();

#ifdef FAST_POLL_INTR
	printk("fast timer: %d Hz, IRQ %d\n", fast_timer_rate,
			FASTTIMER_IRQ);
#else
	printk("fast timer: %d Hz\n", fast_timer_rate);
#endif
	return 0;
}

static void __exit fast_timer_cleanup(void)
{
	ctrl_outb(ctrl_inb(TMU_TSTR) & ~0x2, TMU_TSTR); /* disable timer 1 */
	free_irq(FASTTIMER_IRQ, NULL);
}

#if 0
/*
 * return the current ticks on the fast timer
 */

unsigned long fast_timer_count(void)
{
	return(ctrl_inl(TMU1_TCNT));
}
#endif

#endif
