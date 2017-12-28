/*
 *  arch/mips/philips/nino/setup.c
 *
 *  Copyright (C) 2001 Steven J. Hill (sjhill@realitydiluted.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Interrupt and exception initialization for Philips Nino
 */
#include <linux/console.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <asm/addrspace.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/reboot.h>
#include <asm/time.h>
#include <linux/module.h>
#include "rtl8186.h"

unsigned int GetSysClockRate();

static void nino_machine_restart(char *command)
{
	unsigned long flags;
	static void (*back_to_prom)(void) = (void (*)(void)) 0xbfc00000;
	save_flags(flags); cli();
	/* Reboot */
	back_to_prom();
}

static void nino_machine_halt(void)
{
	printk("Nino halted.\n");
	while(1);
}

static void nino_machine_power_off(void)
{
	printk("Nino halted. Please turn off power.\n");
	while(1);
}

static void __init nino_board_init()
{
	/*
	 * Set up the master clock module. The value set in
	 * the Clock Control Register by WindowsCE is 0x00432ba.
	 * We set a few values here and let the device drivers
	 * handle the rest.
	 *
	 * NOTE: The UART clocks must be enabled here to provide
	 *       enough time for them to settle.
	 */
}

static __init void nino_time_init(void)
{
}
static __init void nino_timer_setup(struct irqaction *irq)
{
	int c;
	unsigned int cpu_clock_rate;

	cpu_clock_rate = 22000000;
	
	//printk("irqaction %x %s %d\n",irq,__FILE__,__LINE__);	
	irq->dev_id = (void *) irq;
	setup_irq(0, irq);
	REG32(TCCNR) = 0; /* disable timer before setting CDBR */
	REG32(CDBR)=DIVISOR;
	REG32(TC0DATA) = (cpu_clock_rate/DIVISOR)/HZ;	

    /* We must wait n cycles for timer to re-latch the new value of TC1DATA. */
	for( c = 0; c < DIVISOR; c++ );
	
	REG32(TCCNR) = TC0EN | TC0MODE_TIMER | TC0SRC_BASICTIMER;
	REG32(TCIR)=TC0IE;
}
void timer_ack()
{
	REG32(TCIR) |= TC0IP;
}

/*
 * I/O ASIC systems use a standard writeback buffer that gets flushed
 * upon an uncached read.
 */
static void wbflush_mips(void)
{
//	__fast_iob();
}


void (*__wbflush) (void);
#define IO_MEM_LOGICAL_START   0x1fc00000
#define ONE_MEG 0x100000
void __init nino_setup(void)
{
	extern void nino_irq_setup(void);
	//clear GIMR first!

	irq_setup = nino_irq_setup;
	set_io_port_base(KSEG1ADDR(0x1d010000));
	//mips_io_port_base=RTL8181_REG_BASE;

	_machine_restart = nino_machine_restart;
	_machine_halt = nino_machine_halt;
	_machine_power_off = nino_machine_power_off;

	board_time_init = nino_time_init;
	board_timer_setup = nino_timer_setup;
	mips_timer_ack=timer_ack;

#ifdef CONFIG_FB
	conswitchp = &dummy_con;
#endif

	__wbflush = wbflush_mips;
	nino_board_init();
}

EXPORT_SYMBOL(GetChipVersion);