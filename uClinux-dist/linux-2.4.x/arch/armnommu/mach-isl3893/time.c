/*
 * time.c  Timer functions for isl3893
 */

#include <linux/time.h>
#include <linux/timex.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <asm/io.h>
#include <asm/arch/hardware.h>
#include <asm/arch/time.h>

unsigned long isl3893_gettimeoffset (void)
{
	// We don't sync our time yet.
	return(0);
}

void isl3893_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	if( reset_timer () )
        	do_timer(regs);
}
