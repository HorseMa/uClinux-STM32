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
#include <asm/rtl865x/interrupt.h>
//#include <asm/rtl8181.h>
#include "rtl_types.h"
#include "asicRegs.h"
//#define ALLINTS (IE_IRQ0 | IE_IRQ1 | IE_IRQ2 | IE_IRQ3 | IE_IRQ4 | IE_IRQ5)
#define ALLINTS (IE_IRQ0 | IE_IRQ2 | IE_IRQ3 | IE_IRQ4 | IE_IRQ5)
spinlock_t irq_lock = SPIN_LOCK_UNLOCKED;


#ifdef CONFIG_RTL865XC


static struct {
	int ext;
	int base;
	int idx;
} irqRoute[]={
#if 1 /*NEW_SPEC*/
	{ 9,4, 3 }, // 0:GPIO E,F,G,H
	{ 8,0, 3 }, // 1:GPIO A,B,C,H
	{ 7,28, 3 }, // 2: SW_Core
	{ 6,24, 4 }, // 3:PCI
	{ 5,20, 3 }, // 4:UART1
	{ 4,16, 3 }, // 5:UART0
	{ 3,12, 3 }, // 6. PCM
	{ 2,28, 3 }, // 7. USB Host
	{ 1,4, 3 },   // 8:Timer1
	{ 0,0, 3 },   // 9:Timer0
#else
	{ 9,28, 3 }, // 0:GPIO A,B,C,D
	{ 8,24, 3 }, // 1:GPIO E,F,G,H
	{ 7,20, 3 }, // 2:Timer0
	{ 6,16, 3 }, // 3:Timer1
	{ 5,12, 3 }, // 4:UART0
	{ 4,8, 3 },   // 5:UART1
	{ 3,4, 3 },   // 6:PCMCIA
	{ 2,0, 4 },   // 7:PCI
	{ 1,28, 3 }, // 8:NIC
	{ 0,28, 3},  // 9:???
#endif
};
#else
static struct {
	int ext;
	int base;
	int idx;
} irqRoute[]={
	{ 9,30, 3 }, // 0:Timer
	{ 8,28, 3 }, // 1:USB
	{ 7,26, 3 }, // 2:PCMCIA
	{ 6,24, 3 }, // 3:UART0
	{ 5,22, 3 }, // 4:UART1
	{ 4,20, 0 }, // 5:PCI
	{ 3,18, 1 }, // 6:NIC
	{ 2,16, 3 }, // 7:GPIO A,B,C
	{ 1,14, 3 }, // 8:EXT
	{ 0,10, 3},  // 9:LBC
	{ 0,10, 3},  // 10:LBC(faked)
	{ 31,29,1 }, // 11:CRYPTO
	{ 28,26,1 }, // 12:AUTH
	{ 25,23,1 }, // 13:PCM
	{ 22,20,1 }, // 14:PCM
	{ 19,17,1 }, // 15:PCIBTMO
};		
#endif

static void  unmask_irq(unsigned int irq)
{
#ifdef CONFIG_RTL865XC
	#if 1 /*NEW_SPEC*/
	REG32(GIMR) = (REG32(GIMR)) | (1 << (17-irq));   // manually set UART0

	if ( (irq == 0) || (irq == 1) || (irq == 6) )
		REG32(IRR2)|= ((irqRoute[irq].idx & 0xF)<<irqRoute[irq].base);
	else
		REG32(IRR1)|= ((irqRoute[irq].idx & 0xF)<<irqRoute[irq].base);
	
	printk("IRR1(%d)=%08x\n", irq, REG32(IRR1));
	printk("GIMR(%d)=%08x\n", irq, REG32(GIMR));
	#else
	REG32(GIMR) = (REG32(GIMR)) | (1 << (31-irq));
	REG32(IRR)|= ((irqRoute[irq].idx & 0xF)<<irqRoute[irq].base);
	printk("IRR(%d)=%08x\n", irq, REG32(IRR));
	printk("GIMR(%d)=%08x\n", irq, REG32(GIMR));
	#endif
#else
	REG32(GIMR) = (REG32(GIMR)) | (1 << (31-irq));
	if ( irq < ICU_CRYPTO )
	{
		/* Set IIR */
		/* yjlou note: It seems a bug in the following line. The extension bit is never used. */
		REG32(IRR)|= ((irqRoute[irq].idx &3)<<irqRoute[irq].base) | (((irqRoute[irq].idx >>2)&1)<<irqRoute[irq].base);
		printk("IRR(%d)=%08x\n", irq, REG32(IRR));
	}
	else
	{
		/* Set IIR2, extension bit is no use in IIR2. */
		REG32(IRR2)|= ((irqRoute[irq].idx&7)<<irqRoute[irq].base);
		printk("IRR2(%d)=%08x\n", irq, REG32(IRR2));
	}
#endif
}


static void  mask_irq(unsigned int irq)
{
#ifdef CONFIG_RTL865XC
#if 1 /*NEW_SPEC*/
	REG32(GIMR)=(REG32(GIMR)) & (~(1 << (17-irq)));
#else
	REG32(GIMR)=(REG32(GIMR)) & (~(1 << (31-irq)));
#endif
#endif
}


static void enable_irq(unsigned int irq)
{
	unsigned long flags;
	spin_lock_irqsave(&irq_lock, flags);
	unmask_irq(irq);
	spin_unlock_irqrestore(&irq_lock, flags);
}

static unsigned int startup_irq(unsigned int irq)
{
	enable_irq(irq);
}

static void disable_irq(unsigned int irq)
{
	unsigned long flags;
	spin_lock_irqsave(&irq_lock, flags);
	mask_irq(irq);
	spin_unlock_irqrestore(&irq_lock, flags);
}

#define shutdown_irq		disable_irq

static void mask_and_ack_irq(unsigned int irq)
{
}
static void end_irq(unsigned int irq)
{
}

static struct hw_interrupt_type irq_type = {
	"RTL865x",
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
#ifdef CONFIG_RTL865XC
	#if 1 /*NEW_SPEC*/
	REG32(GIMR)&= ~0x0000C000;
	#else
	REG32(GIMR)&= ~0x01000000;
	#endif
#else
	REG32(GIMR)&= ~0x06000000;
#endif

#ifdef CONFIG_RTL865X_ROMEPERF
	rtl8651_romeperfEnterPoint(20/*ROMEPERF_INDEX_UNTIL_RXTHREAD*/);
#endif

	gisr = REG32(GISR);
	irq_x = (gimr & gisr);

#ifdef CONFIG_RTL865XC

#if 1 /*NEW_SPEC*/
	do{
		if( irq_x & 0x00000100)
		{

			do_IRQ(9, regs); //Timer
		}
		else if( irq_x & 0x00004000)
		{
			do_IRQ(3, regs); //PCI
		}
		else if( irq_x & 0x00008000)
			do_IRQ(2, regs); //NIC
		else if( irq_x & 0x00010000)
			do_IRQ(0, regs); //GPIO A,B,C,D
		else if( irq_x & 0x00020000)
			do_IRQ(1, regs); //GPIO E,F,G,H
		else if( irq_x & 0x00001000)
		{
			do_IRQ(5, regs); //uart0
		}
		else if( irq_x & 0x00002000)
			do_IRQ(4, regs); //uart1
		else if( irq_x & 0x00000400)
			do_IRQ(7, regs); //USB			
		else if( irq_x & 0x00080000)
			do_IRQ(6, regs); //PCM
		else
		{
			printk("$$$ Unknown IRQ $$$  0x%08x\n",irq_x);
		}
		gisr = REG32(GISR);
		irq_x = (gimr & gisr);
	}while(irq_x);

#else
	do{
		if( irq_x & 0x20000000)
		{
			do_IRQ(2, regs); //Timer
		}
		else if( irq_x & 0x01000000)
		{
			printk("$$$ PCI IRQ $$$\n");
			do_IRQ(7, regs); //PCI
		}
		else if( irq_x & 0x00800000)
			do_IRQ(8, regs); //NIC
		else if( irq_x & 0x80000000)
			do_IRQ(0, regs); //GPIO A,B,C
		else if( irq_x & 0x08000000)
		{
			do_IRQ(4, regs); //uart0
		}
		else if( irq_x & 0x04000000)
			do_IRQ(5, regs); //uart1
		else
		{
			printk("$$$ Unknown IRQ $$$\n");
		}
		gisr = REG32(GISR);
		irq_x = (gimr & gisr);
	}while(irq_x);
#endif

#else
	do{
		if( irq_x & 0x80000000)
			do_IRQ(0, regs); //Timer
		else if( irq_x & 0x04000000)
			do_IRQ(5, regs); //PCI
		else if( irq_x & 0x02000000)
			do_IRQ(6, regs); //NIC
		else if( irq_x & 0x40000000)
			do_IRQ(1, regs); //USB
		else if( irq_x & 0x01000000)
			do_IRQ(7, regs); //GPIO A,B,C
		else if( irq_x & 0x10000000)
			do_IRQ(3, regs); //uart0
		else if( irq_x & 0x08000000)
			do_IRQ(4, regs); //uart1
		else if( irq_x & 0x00100000)
			do_IRQ(ICU_CRYPTO, regs); //CRYPTO
		else if( irq_x & 0x00080000)
			do_IRQ(ICU_AUTH, regs); //AUTH
		else if( irq_x & 0x00040000)
			do_IRQ(ICU_PCM, regs); //PCM
		else if( irq_x & 0x00020000)
			do_IRQ(ICU_PDE, regs); //GPIO DEFGHI
		else if( irq_x & 0x00010000)
			do_IRQ(ICU_PCIBTO, regs); //PCI Bridge Timeout
		gisr = REG32(GISR);
		irq_x = (gimr & gisr);
	}while(irq_x);
#endif

	REG32(GIMR)=gimr;
}

void __init nino_irq_setup(void)
{
	extern asmlinkage void ninoIRQ(void);

	unsigned int i;
	
	REG32(GIMR)=0;
	REG32(IRR)=0;
#ifdef CONFIG_RTL865XC
#if 1 /*NEW_SPEC*/
	REG32(IRR1)=0;
#endif
	REG32(IRR2)=0;
#endif
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
