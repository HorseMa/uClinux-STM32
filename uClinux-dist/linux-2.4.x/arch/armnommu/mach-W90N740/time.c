/*
 * time.c  Timer functions for Winbond W90N740
 */

#include <linux/time.h>
#include <linux/timex.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <asm/io.h>
#include <asm/arch/hardware.h>
#include <linux/interrupt.h>
#include <asm/irq.h>

struct irqaction watchdog_irq =
        {
        name: "watchdog"
                ,
        };


#define TICK_CONST ((1000000 + HZ/2) / HZ)

unsigned long winbond_gettimeoffset (void)
{
unsigned long offset;

/* instead of the variable "tick", the constant TICK_CONST is used here.
 * This way, the multiplication and divide are optimized away by the compiler
 * if "LATCH" equals "TICK_CONST"
 */
	offset = (unsigned long) ((LATCH - CSR_READ(TDR0) - 1)*TICK_CONST)/LATCH;

	if ((offset < (TICK_CONST / 2)) && (CSR_READ(AIC_ISR)&(1<<INT_TINT0)))
		offset += TICK_CONST;

	return offset;
}

void winbond_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
        CSR_WRITE(TISR,2); /* clear TIF0 */
        do_timer(regs);
}

void winbond_watchdog_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
        CSR_WRITE(WTCR, (CSR_READ(WTCR)&0xF7)|0x01);
}
