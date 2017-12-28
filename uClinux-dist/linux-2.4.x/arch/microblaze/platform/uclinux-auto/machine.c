/*
 * arch/microblaze/platform/uclinux-auto/machine.h -- Machine-dependent defs for
 *	auto-configured Microblaze system
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
#include <asm/pgalloc.h>

/* Declarations of core setup functions */
#include <asm/mach.h>

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

/* Called with each timer tick, if non-zero.  */
void (*mach_tick)(void) = 0;

/* Function to drive one of the LED segments */
void mach_heartbeat(unsigned int value)
{
	/* 
	 * FIXME Strictly speaking this should probably be 
	 * protected by a semaphore 
	 */

#ifdef CONFIG_XILINX_GPIO_0_INSTANCE
	/* Read current value on the port */
	volatile unsigned int cur_val = microblaze_gpio_read(CONFIG_XILINX_GPIO_0_BASEADDR);

	/* Clear LED bit  */
	cur_val &= ~0x00010000;

	/* Set it if required */
	if(value)
		cur_val |=  0x00010000;
	
	microblaze_gpio_write(CONFIG_XILINX_GPIO_0_BASEADDR,cur_val);
#endif
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

#ifdef CONFIG_MODEL_RAM

/* Return starting point of romfs image */
inline unsigned *get_romfs_base(void)
{
	/* For now, assume "Standard" model of bss_start */
	return (unsigned *)&__bss_start;
}

/* Handle both romfs and cramfs types, without generating unnecessary
   code (ie no point checking for CRAMFS if it's not even enabled) */
inline unsigned get_romfs_len(unsigned long *addr)
{
#ifdef CONFIG_ROMFS_FS
	if (memcmp(&addr[0], "-rom1fs-", 8) == 0)       /* romfs */
		return be32_to_cpu(addr[2]);
#endif

#ifdef CONFIG_CRAMFS
	if (addr[0] == le32_to_cpu(0x28cd3d45))	/* cramfs */
		return le32_to_cpu(addr[1]);
#endif
	return 0;
}

#endif

/* mach_early_init() is called very early, after the kernel stack and SDA 
   pointers are configured, but before BSS/SBSS are zeroed etc.  Be careful 
   what you do in here.  The bss and sbss sections are zeroed in here */
void __init mach_early_init (void)
{
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

#ifdef CONFIG_MODEL_RAM
	/* Copy ROMFS above bss */
	src=get_romfs_base();
	len=get_romfs_len(src);

	if(len)
	{
		/* Use memmove to handle likely case of memory overlap */
		memmove(__bss_stop,__bss_start,len);
	}

        _ramstart = (unsigned long)__bss_stop + len;
#endif

#ifdef CONFIG_MODEL_ROM
	_ramstart = (unsigned long)__bss_stop;
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

	/* now called from drivers/char/tty_io.c:console_init(), 
	   where it should be! */
	/* xmbrs_console_init();  */
	
	/* 
	 * Enable master control on interrupt controller.  Note
         * this does not enable interrupts in the processor, nor 
	 * does it enable individual IRQs on the controller.  Just
         * initialises the intc in preparation for these things */
	microblaze_intc_master_enable();

#ifdef CONFIG_XILINX_GPIO_0_INSTANCE
	/* Configure the GPIO */
	/* 8 inputs, 16 outputs */
	/* microblaze_gpio_setdir(CONFIG_XILINX_GPIO_0_BASEADDR,MICROBLAZE_GPIO_DIR); */
#endif
}

void mach_get_physical_ram (unsigned long *ram_start, unsigned long *ram_len)
{
	*ram_start = CONFIG_XILINX_ERAM_START;
	*ram_len = CONFIG_XILINX_ERAM_SIZE;
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
