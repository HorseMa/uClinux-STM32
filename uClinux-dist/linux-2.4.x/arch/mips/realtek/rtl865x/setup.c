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
#include <linux/autoconf.h>
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
#include "rtl_types.h"
#include "asicRegs.h"

unsigned int GetSysClockRate();

static void nino_machine_restart(char *command)
{
	unsigned long flags;
	static void (*back_to_prom)(void) = (void (*)(void)) 0xbe000000;
	printk("Enable Watch Dog to Reset whole system\n");
	save_flags(flags); cli();
	#ifdef CONFIG_RTL865XC
	*(volatile unsigned long *)(0xB800311c)=0; /*this is to enable 865xc watch dog reset*/
	#else
	*(volatile unsigned long *)(0xBD01203c)=0; /* this is to enable 865xb watch dog reset*/
	#endif
	for(;;);
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

	cpu_clock_rate = GetSysClockRate();
	
	//printk("irqaction %x %s %d\n",irq,__FILE__,__LINE__);	
	irq->dev_id = (void *) irq;

#ifdef CONFIG_RTL865XC

#if 1 /*NEW_SPEC*/
	setup_irq(9, irq);
	REG32(TCCNR) = 0; /* disable timer before setting CDBR */
	REG32(CDBR)=(DIVISOR) << DIVF_OFFSET;
	REG32(TC0DATA) = (((cpu_clock_rate/DIVISOR)/HZ)) << TCD_OFFSET;	
#else
	setup_irq(2, irq);
	REG32(TCCNR) = 0; /* disable timer before setting CDBR */
	REG32(CDBR)=(DIVISOR) << DIVF_OFFSET;
	REG32(TC0DATA) = (((cpu_clock_rate/DIVISOR)/HZ)) << TCD_OFFSET;	
#endif	
#else
	setup_irq(0, irq);
	REG32(TCCNR) = 0; /* disable timer before setting CDBR */
	REG32(CDBR)=(DIVISOR) << DIVF_OFFSET;
	REG32(TC0DATA) = (((cpu_clock_rate/DIVISOR)/HZ)) << TCD_OFFSET;	
#endif

    /* We must wait n cycles for timer to re-latch the new value of TC1DATA. */
	for( c = 0; c < DIVISOR; c++ );
	
	REG32(TCCNR) = TC0EN | TC0MODE_TIMER;
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

unsigned int GetSysClockRate()
{
	unsigned int SysClkRate;
	
#ifdef CONFIG_RTL865XC
	/* HS1 clock : 200 MHz */
	SysClkRate= 200000000;
#else

    REG32(MACMR) = REG32(MACMR) & ~SYS_CLK_MASK;
	switch ( REG32(MACMR) & SYS_CLK_MASK )
	{
        	case SYS_CLK_100M:
	       		SysClkRate = 100000000;
	        	break;
	        case SYS_CLK_90M:
       			SysClkRate = 90000000;
            		break;
	        case SYS_CLK_85M:
        	    	SysClkRate = 85000000;
			break;
		case SYS_CLK_96M:
			SysClkRate = 96000000;
			break;
		case SYS_CLK_80M:
			SysClkRate = 80000000;
			break;
		case SYS_CLK_75M:
			SysClkRate = 75000000;
			break;
		case SYS_CLK_70M:
			SysClkRate = 70000000;
			break;
		case SYS_CLK_50M:
			SysClkRate = 50000000;
			break;
		default:
			while(1);
	}

#endif

	return SysClkRate;
}

//#define REG32(offset)	(*(volatile unsigned long *)(offset))

int GetChipVersion(char *name,unsigned int size, int *rev)
{

#ifdef CONFIG_RTL865XC
	if (name)
	{
		memset(name, 0, size);

		if (size > strlen("865xC"))
		{
			strcpy(name, "865xC");
		}
	}

	if (rev)
	{
		*rev = 0;
	}

	return 0;
#else

	unsigned int id = REG32(0xbc805104/*CRMR*/);
	if(!id){
		id = REG32(0xbc805100/*CHIPID*/);
		if((unsigned short)(id>>16)==0x8650)
			strncpy(name,"8650",size);
		else
			strncpy(name,"8651",size);
		if(rev)
			*rev=0;
	}else if((unsigned short)id==0x5788){
		int revId;
		revId = (id>>16)&0xf;
#if 1
		/* chenyl: in 865xB rev.C, we can't and don't distinguish the difference between 8650B and 8651B */
		if (revId >= 2 /* rev.C */)
		{
			strncpy(name, "865xB", size);
		}else
#endif
		/* in 865xB rev.A and rev.B, we can distinguish the difference between 8650B and 8651B */
		{
			if(id&0x02000000)
				strncpy(name,"8651B",size);
			else
				strncpy(name,"8650B",size);
		}

		if(rev)
			*rev=revId;
	}else
		snprintf(name, size, "%08x", id);
	//rtlglue_printf("Read CRMR=%08x, CHIP=%08x\n", READ_MEM32(CRMR),READ_MEM32(CHIPID));
	return 0;

#endif
}

EXPORT_SYMBOL(GetChipVersion);
