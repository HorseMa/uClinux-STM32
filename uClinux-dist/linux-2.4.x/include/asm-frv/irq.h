#ifndef _ASM_IRQ_H_
#define _ASM_IRQ_H_

#include <linux/config.h>

/*
 * the system has an on-CPU PIC and another PIC on the FPGA and other PICs on other peripherals,
 * so we do some routing in irq-routing.[ch] to reduce the number of false-positives seen by
 * drivers
 */

#define NR_IRQ_LOG2_ACTIONS_PER_GROUP	5
#define NR_IRQ_ACTIONS_PER_GROUP	(1 << NR_IRQ_LOG2_ACTIONS_PER_GROUP)
#define NR_IRQ_GROUPS			4
#define NR_IRQS				(NR_IRQ_ACTIONS_PER_GROUP * NR_IRQ_GROUPS)

/* probe returns a 32-bit IRQ mask:-/ */
#define MIN_PROBE_IRQ	(NR_IRQS - 32)

static inline int irq_cannonicalize(int irq)
{
	return irq;
}

extern void disable_irq_nosync(unsigned int irq);
extern void disable_irq(unsigned int irq);
extern void enable_irq(unsigned int irq);


#endif /* _ASM_IRQ_H_ */
