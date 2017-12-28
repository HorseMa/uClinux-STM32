/*
 * arch/microblaze/platform/suzaku/machine.c -- platform dependant code
 *
 *  Copyright (C) 2004  Atmark Techno, Inc. <yashi@atmark-techno.com>
 *  Copyright (C) 2001  NEC Corporation
 *  Copyright (C) 2001  Miles Bader <miles@gnu.org>
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License.  See the file COPYING in the main directory of this
 * archive for more details.
 *
 * Written by Miles Bader <miles@gnu.org>
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/irq.h>
#include <linux/time.h>
#include <linux/delay.h>

#include <asm/io.h>
#include <asm/param.h>
#include <asm/cache.h>
#include <asm/microblaze_timer.h>
#include <asm/microblaze_intc.h>
#include <asm/pgalloc.h>

/* Called with each timer tick, if non-zero.  */
void (*mach_tick)(void) = 0;

/* for 2.6, include asm-generic/sections.h */
extern char __bss_start[], __bss_stop[];

void mach_heartbeat(unsigned int value)
{
}

/* mach_gettimeoffset returns the number of microseconds elapsed since the
   start of the current tick */
unsigned long mach_gettimeoffset(void)
{
	unsigned long tctr=timer_get_time(CONFIG_XILINX_TIMER_0_BASEADDR,0);
	unsigned long tcmp=timer_get_compare(CONFIG_XILINX_TIMER_0_BASEADDR,0);
	unsigned long offset;

	/*
	 * Timer used in count up mode, so subtract load value from
	 * counter register to get elapsed ticks */
	offset = (tctr-tcmp)/(CONFIG_XILINX_CPU_CLOCK_FREQ/1000000);

	/*
	 * If we are still in the first half of the upcount and a
	 * timer interupt is pending, then add on a tick's worth of time.
	 */

	if (((offset * 2) < (1000000/HZ)) && 
		(timer_get_csr(CONFIG_XILINX_TIMER_0_BASEADDR,0)&TIMER_INTERRUPT))
	{
		offset += 1000000/HZ;
	}

	return offset;
}

/* Bytes to words, round up */
#define get_romfs_len(addr) (((unsigned int *)(addr))[2])

#define ROMFS_LOCATION (__bss_start)
#define MOVE_ROMFS(dst, src, len) ({memmove(__bss_stop,__bss_start,len);})

/* mach_early_init() is called very early, after the kernel stack and SDA 
   pointers are configured, but before BSS/SBSS are zeroed etc.  Be careful 
   what you do in here.  The bss and sbss sections are zeroed in here */
void __init mach_early_init (void)
{
	unsigned int *src, *dst;
	unsigned int len;

	extern unsigned char _ssbss, _esbss;
	extern unsigned int _intv_load_end, _intv_load_start;
	extern unsigned long _ramstart;

	/* Copy interrupt vectors from high memory down to where
	   they belong. */
	dst = (unsigned int *)0x0;		
	src = (unsigned int *)&_intv_load_start;

#if 1	/* JW don't overwrite reset vector */
	src++;
	src++;
	dst++;
	dst++;
#endif
	do {
		*(dst++) = *(src++);
	} while (src < &_intv_load_end);

        _ramstart = (unsigned long)__bss_stop;

#ifdef CONFIG_MTD_UCLINUX
        /* if CONFIG_MTD_UCLINUX is defined, assume ROMFS is at the
         * end of kernel, which is ROMFS_LOCATION defined above. */
	len = get_romfs_len(ROMFS_LOCATION);
        _ramstart += len;

 /* Use memmove to handle likely case of memory overlap */
	MOVE_ROMFS(__bss_stop,__bss_start,len);
#endif

	/* Zero the BSS and SBSS sections */
	memset(__bss_start,0,__bss_stop-__bss_start);
	memset(&_ssbss,0,&_esbss-&_ssbss);

}

void __init mach_reserve_bootmem(void)
{
}

void __init mach_setup (char **cmdline)
{
	/* Enable the instruction and data caches (if present) */
	#if CONFIG_XILINX_MICROBLAZE0_USE_ICACHE==1
	__flush_icache_all();
	__enable_icache();
	#endif

	#if CONFIG_XILINX_MICROBLAZE0_USE_DCACHE==1
	__flush_dcache_all();
	__enable_dcache();
	#endif

	printk (KERN_INFO "CPU: MICROBLAZE\n");

	/* this is now called from drivers/tty_io.c, as it should be */
	/* xmbrs_console_init();  */
	
	/* 
	 * Enable master control on interrupt controller.  Note
         * this does not enable interrupts in the processor, nor 
	 * does it enable individual IRQs on the controller.  Just
         * initialises the intc in preparation for these things */
	microblaze_intc_master_enable();
}

void __init mach_sched_init (struct irqaction *timer_action)
{
	/* Initialise timer */
	microblaze_timer_configure(0,HZ);

	/* And install timer interrupt handler at appropriate vector */

	/* Timer is always attached to interrupt 0 (highest priority) */
	/* Note we use setup_irq rather than request_irq */
	setup_irq (CONFIG_XILINX_TIMER_0_IRQ, timer_action);
}

void mach_gettimeofday (struct timeval *tv)
{
	tv->tv_sec = 0;
	tv->tv_usec = 0;
}

void machine_halt (void) __attribute__ ((noreturn));
void machine_halt (void)
{
	for (;;) {
		/* Following code should disable interrupts */
		asm ("nop; nop; nop; nop; nop");
	}
}

void machine_restart (char *__unused)
{
        /* wait till "Restarting system\n" is out of uart fifo */
        mdelay(20);
        writel(MICROBLAZE_SYSREG_RECONFIGURE, MICROBLAZE_SYSREG_BASE_ADDR);
        machine_halt();
}

void machine_power_off (void)
{
	machine_halt ();
}


/* Interrupts */

/* Data to initialise interrupt controller functionality */
struct microblaze_intc_irq_init irq_inits[] = {
	/* Name, base, num, priority (unused in microblaze) */
	{ "XINTC", 0, NUM_MACH_IRQS,	7 },
	{ 0 }
};
#define NUM_IRQ_INITS ((sizeof irq_inits / sizeof irq_inits[0]) - 1)

struct hw_interrupt_type hw_itypes[NUM_IRQ_INITS];

/* Initialize interrupts.  */
void __init mach_init_irqs (void)
{
	microblaze_intc_init_irq_types (irq_inits, hw_itypes);
}
