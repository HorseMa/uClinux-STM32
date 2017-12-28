/*
 *  linux/arch/arm/mach-lpc22xx/time.c
 *
 *  Copyright (C) 2004 Philips Semiconductors
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/irq.h>

#include <asm/io.h>
#include <asm/hardware.h>

#include <asm/mach/time.h>


/* set timer to generate interrupt every 10ms */
#define TIMER_10MS		0x01DC

extern void	lpc28xx_unmask_irq(unsigned int);

//static unsigned long clocks_per_usec = 0;

#define CLOCKS_PER_USEC (12000000/1000000)

unsigned long lpc28xx_gettimeoffset (void)
{
	return ((TIMER_10MS-T0VALUE)*21);
}

static irqreturn_t
lpc28xx_timer_interrupt(int irq, void *dev_id)
{
	timer_tick();  /* modified 20050608 for new version */
	T0CLR=0x0;
	return IRQ_HANDLED;
}

static struct irqaction lpc28xx_timer_irq = {
        .name           = "lpc28xx-tick",
	.flags		= IRQF_DISABLED | IRQF_TIMER,
        .handler        = lpc28xx_timer_interrupt
};

/*
 * Set up timer interrupt, and return the current time in seconds.
 */
void __init  lpc28xx_time_init (void)
{
	printk("%s\n",__FUNCTION__);
	
	/*gettimeoffset     = lpc28xx_gettimeoffset; */
	lpc28xx_timer_irq.handler = lpc28xx_timer_interrupt;

	/* set up the interrupt vector for timer 0 match */
	setup_irq(LPC28XX_IRQ_TIMER0, &lpc28xx_timer_irq);
	
	/* enable the timer IRQ */
	lpc28xx_unmask_irq(LPC28XX_IRQ_TIMER0);

	/* let timer 0 run... */
	T0LOAD = TIMER_10MS;	/* Timer value  */
	T0VALUE = TIMER_10MS;
	T0CLR = 0;
	T0CTRL=0xC8;
}

struct sys_timer lpc28xx_timer = {
	.init		= lpc28xx_time_init,
	.offset		= lpc28xx_gettimeoffset,
};
