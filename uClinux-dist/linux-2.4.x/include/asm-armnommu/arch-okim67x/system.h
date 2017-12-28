/*
 * linux/include/asm-armnommu/arch-oki/system.h
 * 2005 Simtec Electronics
 */

#include <asm/arch/watchdog.h>

static inline void arch_idle(void)
{
	while (!current->need_resched && !hlt_counter)
	  cpu_do_idle(IDLE_WAIT_SLOW);
}

extern inline void arch_reset(char mode)
{
	printk("arch_reset: mode %c\n", mode);

	/* set the watchdog going */

	if (mode == 'h') {
		writeb(WDTBCON_PROTECT, WDTBCON);
		writeb(WDTBCON_WDCLK_DIV32 | WDTBCON_SYSRST, WDTBCON);

		writeb(WDTCON_START, WDTCON);

		udelay(1000);

		printk(KERN_ERR "watchdog reset failed?\n");
	}
}
