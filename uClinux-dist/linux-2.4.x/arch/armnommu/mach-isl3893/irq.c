/*
 *  linux/arch/arm/mach-isl3893/irq.c
 */
#include <linux/init.h>

#include <asm/mach/irq.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>

inline void isl3893_irq_ack(int irq)
{
}


void __inline__ isl3893_mask_irq(unsigned int irq)
{
	/* reset corresponding bit in the 32bit Int mask register */
	uIRQ->EnableClear = (1 << (irq & 0x1f));
}


void __inline__ isl3893_unmask_irq(unsigned int irq)
{
	/* set corresponding bit in the 32bit Int mask register */
	uIRQ->EnableSet = (1 << (irq & 0x1f));
}


inline void isl3893_mask_ack_irq(unsigned int irq)
{
}





