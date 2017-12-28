#ifndef __ASM_I386_FAST_TIMER_H
#define __ASM_I386_FAST_TIMER_H

#include <linux/sched.h>
#include <asm/io.h>

#if defined(CONFIG_MTD_NETtel)

#define	SC520_CLOCK		1188200
#define SC520_TIMER_PAGE_ADDR	0xfffef000

static void *fast_timer_mmcrp;

static void fast_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	do_fast_timer();
}

static void fast_timer_set(void)
{
	unsigned long interval;

	/* Setup PIT1 timer */
	outb(0x74, 0x43);
	interval = SC520_CLOCK / fast_timer_rate;
	outb(interval & 0xff, 0x41);
	outb((interval >> 8) & 0xff, 0x41);
}

static int __init fast_timer_setup(void)
{
	int pageaddr;
	int offset;
	volatile unsigned char *pit1map;

	pageaddr = SC520_TIMER_PAGE_ADDR & PAGE_MASK;
	offset = SC520_TIMER_PAGE_ADDR & ~PAGE_MASK;
	set_fixmap_nocache(FIX_SC520_TIMER, pageaddr);
	fast_timer_mmcrp = (void *) fix_to_virt(FIX_SC520_TIMER + offset);
	if (fast_timer_mmcrp == NULL)
		return -ENOMEM;

	if (request_irq(15, fast_timer_interrupt, SA_INTERRUPT,
				"fast timer", NULL))
		return -EBUSY;

	/* Set up interrupt routing for PIT1 timer to IRQ 15 */
	pit1map = (volatile unsigned char *) (fast_timer_mmcrp + 0xd21);
	*pit1map = 0x0a;

	fast_timer_rate = 500;
	fast_timer_set();

	printk("fast timer: %d Hz, IRQ %d\n", fast_timer_rate, 15);
	return 0;
}

static void __exit fast_timer_cleanup(void)
{
	volatile unsigned char *pit1map;

	/* Disable interrupt routing for PIT1 timer */
	pit1map = (volatile unsigned char *) (fast_timer_mmcrp + 0xd21);
	*pit1map = 0x00;

	free_irq(15, NULL);
}

#elif defined(CONFIG_MTD_SNAPGEODE)

static void fast_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	/* Ack RTC periodic interrupt */
	outb(0xc, 0x70);
	inb(0x71);

	do_fast_timer();
}

static void fast_timer_set(void)
{
	/* Fixed at 512Hz */
}

static int __init fast_timer_setup(void)
{
	/* We are using the RTC periodic interrupt */
	if (request_irq(8, fast_timer_interrupt, SA_INTERRUPT,
				"fast timer", NULL))
		return -EBUSY;

	/* Set clock to 1.953ms - 512Hz */
	outb(0xa, 0x70);
	outb(((inb(0x71) & 0x70) | 0x7), 0x71);

	/* Enable RTC periodic interrupt */
	outb(0xb, 0x70);
	outb(((inb(0x71) & 0xf7) | 0x40), 0x71);

	fast_timer_rate = 512;
	printk("fast timer: %d Hz, IRQ %d\n", fast_timer_rate, 8);
	return 0;
}

static void __exit fast_timer_cleanup(void)
{
	/* Disable RTC periodic interrupt */
	outb(0xb, 0x70);
	outb((inb(0x71) & 0xb7), 0x71);
}

#else

static struct timer_list fast_timer_list;

static void fast_timer_poll(unsigned long arg)
{
	unsigned long timer_status;

	do_fast_timer();

	fast_timer_list.expires = jiffies + 1;
	add_timer(&fast_timer_list);
}

static void fast_timer_set(void)
{
	/* Fixed at 1 jiffy */
}

static int __init fast_timer_setup(void)
{
	init_timer(&fast_timer_list);
	fast_timerlist.function = fast_timer_poll;
	fast_timerlist.data = 0;
	fast_timerlist.expires = jiffies + 1;
	add_timer(&fast_timer_list);

	printk("fast timer: %d Hz\n", HZ);
	return 0;
}

static void __exit fast_timer_cleanup(void)
{
	del_timer(&fast_timer_list);
}

#endif

#endif
