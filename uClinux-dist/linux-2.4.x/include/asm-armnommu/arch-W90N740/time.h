/*
 * linux/include/asm/arch-W90N740/time.h
 * 2003 winbond
 */

#ifndef __ASM_ARCH_TIME_H__
#define __ASM_ARCH_TIME_H__

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/hardware.h>
#include <asm/arch/timex.h>

extern struct irqaction timer_irq;
extern struct irqaction watchdog_irq;

extern unsigned long winbond_gettimeoffset(void);
extern void winbond_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs);
extern void winbond_watchdog_interrupt(int irq, void *dev_id, struct pt_regs *regs);


void __inline__ setup_timer (void)
{

	/*----- disable timer -----*/
	CSR_WRITE(TCR0, 0);
   	CSR_WRITE(TCR1, 0);
   	
	/* configure GPIO */
	//CSR_WRITE(GPIO_CFG, 0x00150D0);//15AD8
	CSR_WRITE(GPIO_CFG, 0x00050D0);//15AD8

	CSR_WRITE (AIC_SCR7, 0x41);  /* high-level sensitive, priority level 1 */
	/*----- timer 0 : periodic mode, LATCH-1 ticks/jiffy -----*/
	CSR_WRITE(TICR0, LATCH-1);
   
	timer_irq.handler = winbond_timer_interrupt;
	setup_arm_irq(IRQ_TIMER, &timer_irq);
	
	INT_ENABLE(IRQ_TIMER);
		/*----- clear interrupt flag bit -----*/
	CSR_WRITE(TISR, 0);  /* clear for safty */   

	CSR_WRITE(TCR0, 0xe8000000 + (fMCLK_MHz/CLOCK_TICK_RATE));

	gettimeoffset = winbond_gettimeoffset;

#if 0//clyu 030616
	/*enable Watch dog*/
	CSR_WRITE(WTCR, 0x01);/*reset timer*/
	CSR_WRITE(WTCR, 0xD2);/*time-out=11*/
	watchdog_irq.handler = winbond_watchdog_interrupt;
	setup_arm_irq(INT_WDTINT, &watchdog_irq);
#endif
}

#endif /* __ASM_ARCH_TIME_H__ */
