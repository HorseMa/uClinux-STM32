/*
 *  arch/mips/philips/nino/irq.c
 *
 *  Copyright (C) 2001 Steven J. Hill (sjhill@realitydiluted.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Interrupt service routines for Philips Nino
 */
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include <asm/mipsregs.h>
//#include <asm/rtl865x/interrupt.h>
//#include <asm/rtl8181.h>
#include "rtl8186.h"
#define ALLINTS (IE_IRQ0 | IE_IRQ1 | IE_IRQ2 | IE_IRQ3 | IE_IRQ4 | IE_IRQ5)
//#define ALLINTS (IE_IRQ0 | IE_IRQ2 | IE_IRQ3 | IE_IRQ4 | IE_IRQ5)




static struct {
	int ext;
	int base;
	int idx;
} irqRoute[10]={
	{ 0,30, 3 }, //0:Timer
	{ 8,28, 3 }, // 1:USB
	{ 7,26, 3 }, // 2:PCMCIA
	{ 3,24, 3 }, // 3:UART0
	{ 3,22, 3 }, // 4:UART1
	{ 4,20, 0 }, // 5:PCI
	{ 3,18, 1 }, // 6:NIC
	{ 2,16, 3 }, // 7:GPIO A,B,C
	{ 1,14, 3 }, // 8:EXT
	{ 0,10, 3},// 9: LBC
};		

static void  unmask_irq(unsigned int irq)
{
	REG32(GIMR) |= (1 << irqRoute[irq].ext);
}
static void  mask_irq(unsigned int irq)
{
	REG32(GIMR) &= ~(1 << irqRoute[irq].ext);
}


static void enable_irq(unsigned int irq)
{

	return 0;
}

static unsigned int startup_irq(unsigned int irq)
{
	unmask_irq(irq);
	return 0;		/* Never anything pending  */
}

static void disable_irq(unsigned int irq)
{

	return 0;
}

#define shutdown_irq		disable_irq

static void mask_and_ack_irq(unsigned int irq)
{

	return 0;
}
static void end_irq(unsigned int irq)
{
	return 0;
}

static struct hw_interrupt_type irq_type = {
	"RTL8186",
	startup_irq,
	shutdown_irq,
	enable_irq,
	disable_irq,
	mask_and_ack_irq,
	end_irq,
	NULL
};

__IRAM_GEN void irq_dispatch(struct pt_regs *regs)
{
	unsigned int gimr, gisr,irq_x;  
	gimr = REG32(GIMR);
	REG32(GIMR) = 0;

	gisr = REG32(GISR);
	irq_x = (gimr & gisr);

	do{
		if( irq_x & (1<<0))
			do_IRQ(0, regs); //Timer
//		else if( irq_x & 0x04000000)
//			do_IRQ(5, regs); //PCI
//		else if( irq_x & 0x02000000)
//			do_IRQ(6, regs); //NIC
//		else if( irq_x & 0x40000000)
//			do_IRQ(1, regs); //USB
//		else if( irq_x & 0x01000000)
//			do_IRQ(7, regs); //GPIO A,B,C
		else if( irq_x & (1<<3))
			do_IRQ(4, regs); //uart0
//		else if( irq_x & 0x08000000)
//			do_IRQ(4, regs); //uart1
		gisr = REG32(GISR);
		irq_x = (gimr & gisr);
	}while(irq_x);
	REG32(GIMR)=gimr;
}

void __init nino_irq_setup(void)
{
	extern asmlinkage void ninoIRQ(void);

	unsigned int i;
	
	REG32(GIMR)=0;
	/* Disable all hardware interrupts */
//	change_cp0_status(ST0_IM, 0x00);
//	cil();

	/* Initialize IRQ vector table */
	//init_generic_irq();

	/* Initialize IRQ action handlers */
	for (i = 0; i < 16; i++) {
		hw_irq_controller *handler = NULL;
		handler		= &irq_type;

		irq_desc[i].status	= IRQ_DISABLED;
		irq_desc[i].action	= 0;
		irq_desc[i].depth	= 1;
		irq_desc[i].handler	= handler;
	}

	/* Set up the external interrupt exception vector */
	set_except_vector(0, ninoIRQ);

	/* Enable all interrupts */
//	change_cp0_status(ST0_IM, ALLINTS);
}

void (*irq_setup)(void);

void __init init_IRQ(void)
{
	int flags;
//	printk("\nfile %s cp0 status %x\n",__FILE__,read_c0_status());
	flags = read_c0_status();
	flags |= CAUSEF_IP4|CAUSEF_IP3|CAUSEF_IP5|0xfc00;
	write_c0_status(flags);	
	//printk("\nfile %s cp0 status %x\n",__FILE__,read_c0_status());
#ifdef CONFIG_REMOTE_DEBUG
	extern void breakpoint(void);
	extern void set_debug_traps(void);

	printk("Wait for gdb client connection ...\n");
	set_debug_traps();
	breakpoint();
#endif

	/* Invoke board-specific irq setup */
	irq_setup();
}
