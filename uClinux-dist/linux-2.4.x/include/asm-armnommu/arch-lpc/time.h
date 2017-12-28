#if (KERNEL_TIMER == 0)
#	define KERNEL_TIMER_IRQ_NUM IRQ_TC0
#elif (KERNEL_TIMER == 1)
#	define KERNEL_TIMER_IRQ_NUM IRQ_TC1

#else
#error Wierd -- KERNEL_TIMER is not defined or something...
#endif

static unsigned long lpc_gettimeoffset(void)
{
	/* volatile struct lpc_timers * tt = (struct lpc_timers*)(LPC_TC_BASE); */
	return 0;
}

static void lpc_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	/* printk("lpc_timer_interrupt\n"); */
	__arch_putl(0x1, T0IR);
	do_timer(regs);
	do_profile(regs);
}

extern void lpc_unmask_irq(int);

extern inline void setup_timer(void)
{
	// init timer
	__arch_putl(0x2, T0TCR);
	__arch_putl(0xffffffff, T0IR);
	__arch_putl(0x0, T0PR);
	__arch_putl(0x3, T0MCR);
	__arch_putl(Fpclk/HZ, T0MR0);
	__arch_putl(0x1, T0TCR);

	lpc_unmask_irq(KERNEL_TIMER_IRQ_NUM);

	gettimeoffset = lpc_gettimeoffset;
	timer_irq.handler = lpc_timer_interrupt;
	setup_arm_irq(KERNEL_TIMER_IRQ_NUM, &timer_irq);
}
