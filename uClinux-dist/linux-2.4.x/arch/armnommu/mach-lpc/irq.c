#include <linux/init.h>

#include <asm/mach/irq.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>

extern struct irqdesc irq_desc[];

void lpc_mask_irq(unsigned int irq)
{
	__arch_putl(1<<irq, VIC_IECR);
}

void lpc_unmask_irq(unsigned int irq)
{
	unsigned long mask = 1 << (irq);
	unsigned long ier = __arch_getl(VIC_IER);
	
	ier = mask|ier;
	__arch_putl(ier, VIC_IER);
}

void lpc_mask_ack_irq(unsigned int irq)
{
	__arch_putl(0x0, VIC_AR);
	lpc_mask_irq(irq);
}

void lpc_init_vic(void)
{
	int irqno;
	
	/* Disable all interrupts */
	__arch_putl(0xffffffff, VIC_IECR);

	/* Clear all soft interrupts */
	__arch_putl(0xffffffff, VIC_SICR);

	/* use IRQ not FIQ */
	__arch_putl(0x0, VIC_ISLR);

//	for(irqno = 0; irqno < 16; irqno++)
//	{
//		__arch_putl(irqno, VIC_VAR(irqno)); /*index*/
//		__arch_putl(0x20|irqno, VIC_VCR(irqno)); /*vect*/
//	}
//	__arch_putl(16, VIC_DVAR);

	/* set protect */
	__arch_putl(1, VIC_PER);

	/* remap IRQ to RAM */
	__arch_putl(0x2, MEMMAP);
}
