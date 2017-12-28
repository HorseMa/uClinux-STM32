#ifndef __ASM_ARCH_IRQ_H
#define __ASM_ARCH_IRQ_H

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/mach/irq.h>
#include <asm/arch/irqs.h>

#define fixup_irq(x) (x)

extern void lpc_mask_irq(unsigned int irq);
extern void lpc_unmask_irq(unsigned int irq);
extern void lpc_mask_ack_irq(unsigned int irq);
extern void lpc_init_vic(void);

static __inline__ void irq_init_irq(void)
{
	int irq;
	lpc_init_vic();
	for(irq = 0; irq < NR_IRQS; irq++) {
		if(!VALID_IRQ(irq)) continue;
		irq_desc[irq].valid = 1;
		irq_desc[irq].probe_ok = 1;
		irq_desc[irq].mask_ack = lpc_mask_ack_irq;
		irq_desc[irq].mask = lpc_mask_irq;
		irq_desc[irq].unmask = lpc_unmask_irq;
	}
}
#endif /* __ASM_ARCH_IRQ_H */
