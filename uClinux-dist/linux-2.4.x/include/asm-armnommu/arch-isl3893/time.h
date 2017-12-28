/*
 * linux/include/asm-arm/arch-isl3893/time.h
 */

#ifndef __ASM_ARCH_TIME_H__
#define __ASM_ARCH_TIME_H__

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/hardware.h>
#include <asm/arch/irq.h>
#include <asm/arch/irqs.h>
#include <asm/arch/timex.h>
#include <linux/interrupt.h>

#define KERNEL_TIMER_IRQ_NUM	IRQ_RTC

extern struct irqaction timer_irq;
extern unsigned long (*gettimeoffset)(void);

extern unsigned long isl3893_gettimeoffset(void);
extern void isl3893_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs);

/*
 * Reset the timer.
 */
extern __inline__ int reset_timer(void)
{
	uRTC->Data = 0;
	uRTC->IntClear = RTCIntClearEVENT;
	return (1);
}

extern __inline__ void setup_timer (void)
{
	gettimeoffset = isl3893_gettimeoffset;
	timer_irq.name = "timer";
	timer_irq.handler = isl3893_timer_interrupt;
	timer_irq.flags = SA_INTERRUPT;
	setup_arm_irq(KERNEL_TIMER_IRQ_NUM, &timer_irq);

#ifdef ISL3893_SIMSONLY
/* For simulations, this is a small value. In real life, set this to 10 */
	uRTC->Match = 2;
#else
	uRTC->Match = 10;
#endif
	uRTC->Data = 0;
	uRTC->IntClear = RTCIntClearEVENT;
	uRTC->IntControl |= RTCIntControlEVENT;
}

#endif /* __ASM_ARCH_TIME_H__ */
