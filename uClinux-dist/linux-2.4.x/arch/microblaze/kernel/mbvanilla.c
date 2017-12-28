/*
 * arch/microblaze/kernel/mbvanilla.h -- Machine-dependent defs for
 *	"vanillia" Microblaze system
 *
 *  Copyright (C) 2003	John Williams <jwilliams@itee.uq.edu.au>
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License.  See the file COPYING in the main directory of this
 * archive for more details.
 *
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/bootmem.h>
#include <linux/irq.h>

#include <asm/atomic.h>
#include <asm/page.h>
#include <asm/machdep.h>
#include <asm/bitops.h>

/* Get base address definitions for core peripherals */
#include <asm/mbvanilla.h>

/* Declarations of core setup functions */
#include "mach.h"

/* Get header files for the peripherals */
#include <asm/microblaze_timer.h>
#include <asm/microblaze_intc.h>
#include <asm/microblaze_gpio.h>

/* for 2.6, include asm-generic/sections.h */
extern char __bss_start[], __bss_stop[];

#define RAM_START (ERAM_ADDR)
#define RAM_END ((ERAM_ADDR)+(ERAM_SIZE))

extern void memcons_setup (void);
extern void xmbrs_console_init(void);


#define REG_DUMP_ADDR		0x220000


extern struct irqaction reg_snap_action; /* fwd decl */

/* Function to drive one of the LED segments */
void mach_heartbeat(unsigned int value)
{
	/* 
	 * FIXME Strictly speaking this should probably be 
	 * protected by a semaphore 
	 */

	/* Read current value on the port */
	volatile unsigned int cur_val = microblaze_gpio_read(MICROBLAZE_GPIO_BASE_ADDR);

	/* Clear LED bit  */
	cur_val &= ~0x00010000;

	/* Set it if required */
	if(value)
		cur_val |=  0x00010000;
	
	microblaze_gpio_write(MICROBLAZE_GPIO_BASE_ADDR,cur_val);
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

/* mach_early_init() is called very early, after the kernel stack and SDA 
   pointers are configured, but before BSS/SBSS are zeroed etc.  Be careful 
   what you do in here.  The bss and sbss sections are zeroed in here */
void __init mach_early_init (void)
{
	int i;
	unsigned int *src, *dst;
	unsigned int len;

	extern unsigned char _ssbss, _esbss;
	extern unsigned int _intv_load_end, _intv_load_start;
	extern unsigned int _ramstart;

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

	/* Copy ROMFS above bss */
	src = (unsigned int *)__bss_start;
	len = src[2];    /* Bytes to words, round up */

	/* Use memmove to handle likely case of memory overlap */
	memmove(__bss_stop,__bss_start,len);

        _ramstart = (unsigned long)__bss_stop + len;
	
	/* Zero the BSS and SBSS sections */
	memset(__bss_start,0,__bss_stop-__bss_start);
	memset(&_ssbss,0,&_esbss-&_ssbss);

}

/* Reserve memory where the root_fs is stored.  
 * Used when blkmem is the underlying driver, and the symbols
 * _root_fs_image_start and _end are defined as the bounds of 
 * the romfs image */
#if 0
void __init reserve_blkmem_root_fs(void)
{
	extern char _root_fs_image_start, _root_fs_image_end;
	u32 root_fs_image_start = (u32)&_root_fs_image_start;
	u32 root_fs_image_end = (u32)&_root_fs_image_end;

	if(root_fs_image_start >= RAM_START && root_fs_image_start < RAM_END)
	{
		#ifdef CONFIG_MICROBLAZE_DEBUGGING
	 	printk("ROMFS (on blkmem):%08X -> %08X (%08X)\n",root_fs_image_start, root_fs_image_end, root_fs_image_end - root_fs_image_start); 
		#endif
		reserve_bootmem(root_fs_image_start,
				root_fs_image_end - root_fs_image_start);
	}
	
}
#endif

/* Reserve memory where the root_fs is stored.
 * Used when MTD ram_map is the underlying driver, and
 * the romfs image is tacked on at __bss_stop, and its length
 * is derived from the romfs image itself */
void __init reserve_mtd_ram_root_fs(void)
{
	u32 root_fs_image_start = (u32)__bss_stop;
	u32 root_fs_image_len = ((u32 *)root_fs_image_start)[2];

	if(root_fs_image_start >= RAM_START && root_fs_image_start < RAM_END)
	{
		#ifdef CONFIG_MICROBLAZE_DEBUGGING
	 	printk("ROMFS (on mtd):%08X -> %08X (%08X)\n",root_fs_image_start, root_fs_image_start + root_fs_image_len, root_fs_image_len); 
		#endif
		reserve_bootmem(root_fs_image_start,
				root_fs_image_len);
	}
	
}

void __init mach_reserve_bootmem(void)
{
	/* FIXME need a CONFIG_something here to determine which memory
	   driver is carrying the rootfs */
	#if 0
		reserve_blkmem_root_fs();
		reserve_mtd_ram_root_fs();
	#endif
}

void __init mach_setup (char **cmdline)
{
	/* Enable the instruction and data caches (if present) */
	#if CONFIG_XILINX_MICROBLAZE0_USE_ICACHE==1
	__enable_icache();
	#endif

	#if CONFIG_XILINX_MICROBLAZE0_USE_DCACHE==1
	__enable_dcache();
	#endif

	printk (KERN_INFO "CPU: MICROBLAZE\n");

	/* memcons_setup (); */
	/* This is defined as an initcall, but doesn't seem to work...*/
	xmbrs_console_init(); 
	
	/* 
	 * Enable master control on interrupt controller.  Note
         * this does not enable interrupts in the processor, nor 
	 * does it enable individual IRQs on the controller.  Just
         * initialises the intc in preparation for these things */
	microblaze_intc_master_enable();

	/* Configure the GPIO */
	/* 8 inputs, 16 outputs */
	microblaze_gpio_setdir(MICROBLAZE_GPIO_BASE_ADDR,MICROBLAZE_GPIO_DIR);
}

void mach_get_physical_ram (unsigned long *ram_start, unsigned long *ram_len)
{
	*ram_start = ERAM_ADDR;
	*ram_len = ERAM_SIZE;
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
	machine_halt ();
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


/* An interrupt handler that copies the registers to a known memory location,
   for debugging purposes.  */

#if 0
static void make_reg_snap (int irq, void *dummy, struct pt_regs *regs)
{
	(*(unsigned *)REG_DUMP_ADDR)++;
	(*(struct pt_regs *)(REG_DUMP_ADDR + sizeof (unsigned))) = *regs;
}

static int reg_snap_dev_id;
static struct irqaction reg_snap_action = {
	make_reg_snap, 0, 0, "reg_snap", &reg_snap_dev_id, 0
};
#endif
