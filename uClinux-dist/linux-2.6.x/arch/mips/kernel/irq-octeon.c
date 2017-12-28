/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2004-2007 Cavium Networks
 */
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/pci.h>

#include <asm/irq_cpu.h>
#include <asm/mipsregs.h>
#include <asm/system.h>

#include "../cavium-octeon/hal.h"

DEFINE_RWLOCK(octeon_irq_ciu0_rwlock);
DEFINE_RWLOCK(octeon_irq_ciu1_rwlock);
DEFINE_SPINLOCK(octeon_irq_msi_lock);

static void octeon_irq_core_ack(unsigned int irq)
{
	/* We don't need to disable IRQs to make these atomic since they are
	   already disabled earlier in the low level interrupt code */
	clear_c0_status(0x100 << irq);
	/* The two user interrupts must be cleared manually */
	if (irq < 2)
		clear_c0_cause(0x100 << irq);
}

static void octeon_irq_core_eoi(unsigned int irq)
{
	irq_desc_t *desc = irq_desc + irq;
	/* If an IRQ is being processed while we are disabling it the handler
	   will attempt to unmask the interrupt after it has been disabled */
	if (desc->status & IRQ_DISABLED)
		return;
	/* We don't need to disable IRQs to make these atomic since they are
	   already disabled earlier in the low level interrupt code */
	set_c0_status(0x100 << irq);
}

static void octeon_irq_core_enable(unsigned int irq)
{
	/* We need to disable interrupts to make sure our updates are atomic */
	unsigned long flags;
	local_irq_save(flags);
	set_c0_status(0x100 << irq);
	local_irq_restore(flags);
}

static void octeon_irq_core_disable_local(unsigned int irq)
{
	/* We need to disable interrupts to make sure our updates are atomic */
	unsigned long flags;
	local_irq_save(flags);
	clear_c0_status(0x100 << irq);
	local_irq_restore(flags);
}

static void octeon_irq_core_disable(unsigned int irq)
{
#ifdef CONFIG_SMP
	on_each_cpu((void (*)(void *)) octeon_irq_core_disable_local,
		    (void *) (long) irq, 0, 1);
#else
	octeon_irq_core_disable_local(irq);
#endif
}

struct irq_chip octeon_irq_chip_core = {
	.name = "Core",
	.enable = octeon_irq_core_enable,
	.disable = octeon_irq_core_disable,
	.ack = octeon_irq_core_ack,
	.eoi = octeon_irq_core_eoi,
};


static void octeon_irq_ciu0_ack(unsigned int irq)
{
	/* In order to avoid any locking accessing the CIU, we acknowledge CIU
	   interrupts by disabling all of them. This way we can use a per core
	   register and avoid any out of core locking requirements. This has
	   the side affect that CIU interrupts can't be processed recursively */
	/* We don't need to disable IRQs to make these atomic since they are
	   already disabled earlier in the low level interrupt code */
	clear_c0_status(0x100 << 2);
}

static void octeon_irq_ciu0_eoi(unsigned int irq)
{
	/* Enable all CIU interrupts again */
	/* We don't need to disable IRQs to make these atomic since they are
	   already disabled earlier in the low level interrupt code */
	set_c0_status(0x100 << 2);
}

static void octeon_irq_ciu0_enable(unsigned int irq)
{
	int coreid = cvmx_get_core_num();
	unsigned long flags;
	uint64_t en0;
	int bit = irq - OCTEON_IRQ_WORKQ0;	/* Bit 0-63 of EN0 */

	/* A read lock is used here to make sure only one core is ever updating
	   the CIU enable bits at a time. During an enable the cores don't
	   interfere with each other. During a disable the write lock stops any
	   enables that might cause a problem */
	read_lock_irqsave(&octeon_irq_ciu0_rwlock, flags);
	en0 = cvmx_read_csr(CVMX_CIU_INTX_EN0(coreid * 2));
	en0 |= 1ull << bit;
	cvmx_write_csr(CVMX_CIU_INTX_EN0(coreid * 2), en0);
	cvmx_read_csr(CVMX_CIU_INTX_EN0(coreid * 2));
	read_unlock_irqrestore(&octeon_irq_ciu0_rwlock, flags);
}

static void octeon_irq_ciu0_disable(unsigned int irq)
{
	int bit = irq - OCTEON_IRQ_WORKQ0;	/* Bit 0-63 of EN0 */
	unsigned long flags;
	uint64_t en0;
#ifdef CONFIG_SMP
	int cpu;
	write_lock_irqsave(&octeon_irq_ciu0_rwlock, flags);
	for (cpu = 0; cpu < NR_CPUS; cpu++) {
		if (cpu_present(cpu)) {
			int coreid = cpu_logical_map(cpu);
			en0 = cvmx_read_csr(CVMX_CIU_INTX_EN0(coreid * 2));
			en0 &= ~(1ull << bit);
			cvmx_write_csr(CVMX_CIU_INTX_EN0(coreid * 2), en0);
		}
	}
	/* We need to do a read after the last update to make sure all of them
	   are done */
	cvmx_read_csr(CVMX_CIU_INTX_EN0(cvmx_get_core_num() * 2));
	write_unlock_irqrestore(&octeon_irq_ciu0_rwlock, flags);
#else
	int coreid = cvmx_get_core_num();
	local_irq_save(flags);
	en0 = cvmx_read_csr(CVMX_CIU_INTX_EN0(coreid * 2));
	en0 &= ~(1ull << bit);
	cvmx_write_csr(CVMX_CIU_INTX_EN0(coreid * 2), en0);
	cvmx_read_csr(CVMX_CIU_INTX_EN0(coreid * 2));
	local_irq_restore(flags);
#endif
}

#ifdef CONFIG_SMP
static void octeon_irq_ciu0_set_affinity(unsigned int irq, cpumask_t dest)
{
	int cpu;
	int bit = irq - OCTEON_IRQ_WORKQ0;	/* Bit 0-63 of EN0 */

	write_lock(&octeon_irq_ciu0_rwlock);
	for (cpu = 0; cpu < NR_CPUS; cpu++) {
		if (cpu_present(cpu)) {
			int coreid = cpu_logical_map(cpu);
			uint64_t en0 =
				cvmx_read_csr(CVMX_CIU_INTX_EN0(coreid * 2));
			if (cpu_isset(cpu, dest))
				en0 |= 1ull << bit;
			else
				en0 &= ~(1ull << bit);
			cvmx_write_csr(CVMX_CIU_INTX_EN0(coreid * 2), en0);
		}
	}
	/* We need to do a read after the last update to make sure all of them
	   are done */
	cvmx_read_csr(CVMX_CIU_INTX_EN0(cvmx_get_core_num() * 2));
	write_unlock(&octeon_irq_ciu0_rwlock);
}
#endif

struct irq_chip octeon_irq_chip_ciu0 = {
	.name = "CIU0",
	.enable = octeon_irq_ciu0_enable,
	.disable = octeon_irq_ciu0_disable,
	.ack = octeon_irq_ciu0_ack,
	.eoi = octeon_irq_ciu0_eoi,
#ifdef CONFIG_SMP
	.set_affinity = octeon_irq_ciu0_set_affinity,
#endif
};


static void octeon_irq_ciu1_ack(unsigned int irq)
{
	/* In order to avoid any locking accessing the CIU, we acknowledge CIU
	   interrupts by disabling all of them. This way we can use a per core
	   register and avoid any out of core locking requirements. This has
	   the side affect that CIU interrupts can't be processed recursively */
	/* We don't need to disable IRQs to make these atomic since they are
	   already disabled earlier in the low level interrupt code */
	clear_c0_status(0x100 << 3);
}

static void octeon_irq_ciu1_eoi(unsigned int irq)
{
	/* Enable all CIU interrupts again */
	/* We don't need to disable IRQs to make these atomic since they are
	   already disabled earlier in the low level interrupt code */
	set_c0_status(0x100 << 3);
}

static void octeon_irq_ciu1_enable(unsigned int irq)
{
	int coreid = cvmx_get_core_num();
	unsigned long flags;
	uint64_t en1;
	int bit = irq - OCTEON_IRQ_WDOG0;	/* Bit 0-63 of EN1 */

	/* A read lock is used here to make sure only one core is ever updating
	   the CIU enable bits at a time. During an enable the cores don't
	   interfere with each other. During a disable the write lock stops any
	   enables that might cause a problem */
	read_lock_irqsave(&octeon_irq_ciu1_rwlock, flags);
	en1 = cvmx_read_csr(CVMX_CIU_INTX_EN1(coreid * 2 + 1));
	en1 |= 1ull << bit;
	cvmx_write_csr(CVMX_CIU_INTX_EN1(coreid * 2 + 1), en1);
	cvmx_read_csr(CVMX_CIU_INTX_EN1(coreid * 2 + 1));
	read_unlock_irqrestore(&octeon_irq_ciu1_rwlock, flags);
}

static void octeon_irq_ciu1_disable(unsigned int irq)
{
	int bit = irq - OCTEON_IRQ_WDOG0;	/* Bit 0-63 of EN1 */
	unsigned long flags;
	uint64_t en1;
#ifdef CONFIG_SMP
	int cpu;
	write_lock_irqsave(&octeon_irq_ciu1_rwlock, flags);
	for (cpu = 0; cpu < NR_CPUS; cpu++) {
		if (cpu_present(cpu)) {
			int coreid = cpu_logical_map(cpu);
			en1 = cvmx_read_csr(CVMX_CIU_INTX_EN1(coreid * 2 + 1));
			en1 &= ~(1ull << bit);
			cvmx_write_csr(CVMX_CIU_INTX_EN1(coreid * 2 + 1), en1);
		}
	}
	/* We need to do a read after the last update to make sure all of them
	   are done */
	cvmx_read_csr(CVMX_CIU_INTX_EN1(cvmx_get_core_num() * 2 + 1));
	write_unlock_irqrestore(&octeon_irq_ciu1_rwlock, flags);
#else
	int coreid = cvmx_get_core_num();
	local_irq_save(flags);
	en1 = cvmx_read_csr(CVMX_CIU_INTX_EN1(coreid * 2 + 1));
	en1 &= ~(1ull << bit);
	cvmx_write_csr(CVMX_CIU_INTX_EN1(coreid * 2 + 1), en1);
	cvmx_read_csr(CVMX_CIU_INTX_EN1(coreid * 2 + 1));
	local_irq_restore(flags);
#endif
}

#ifdef CONFIG_SMP
static void octeon_irq_ciu1_set_affinity(unsigned int irq, cpumask_t dest)
{
	int cpu;
	int bit = irq - OCTEON_IRQ_WDOG0;	/* Bit 0-63 of EN1 */

	write_lock(&octeon_irq_ciu1_rwlock);
	for (cpu = 0; cpu < NR_CPUS; cpu++) {
		if (cpu_present(cpu)) {
			int coreid = cpu_logical_map(cpu);
			uint64_t en1 =
				cvmx_read_csr(CVMX_CIU_INTX_EN1
					      (coreid * 2 + 1));
			if (cpu_isset(cpu, dest))
				en1 |= 1ull << bit;
			else
				en1 &= ~(1ull << bit);
			cvmx_write_csr(CVMX_CIU_INTX_EN1(coreid * 2 + 1), en1);
		}
	}
	/* We need to do a read after the last update to make sure all of them
	   are done */
	cvmx_read_csr(CVMX_CIU_INTX_EN1(cvmx_get_core_num() * 2 + 1));
	write_unlock(&octeon_irq_ciu1_rwlock);
}
#endif

struct irq_chip octeon_irq_chip_ciu1 = {
	.name = "CIU1",
	.enable = octeon_irq_ciu1_enable,
	.disable = octeon_irq_ciu1_disable,
	.ack = octeon_irq_ciu1_ack,
	.eoi = octeon_irq_ciu1_eoi,
#ifdef CONFIG_SMP
	.set_affinity = octeon_irq_ciu1_set_affinity,
#endif
};


static void octeon_irq_i8289_master_unmask(unsigned int irq)
{
	unsigned long flags;
	local_irq_save(flags);
	outb(inb(0x21) & ~(1 << (irq - OCTEON_IRQ_I8259M0)), 0x21);
	local_irq_restore(flags);
}

static void octeon_irq_i8289_master_mask(unsigned int irq)
{
	unsigned long flags;
	local_irq_save(flags);
	outb(inb(0x21) | (1 << (irq - OCTEON_IRQ_I8259M0)), 0x21);
	local_irq_restore(flags);
}

struct irq_chip octeon_irq_chip_i8259_master = {
	.name = "i8259M",
	.mask = octeon_irq_i8289_master_mask,
	.mask_ack = octeon_irq_i8289_master_mask,
	.unmask = octeon_irq_i8289_master_unmask,
	.eoi = octeon_irq_i8289_master_unmask,
};


static void octeon_irq_i8289_slave_unmask(unsigned int irq)
{
	outb(inb(0xa1) & ~(1 << (irq - OCTEON_IRQ_I8259S0)), 0xa1);
}

static void octeon_irq_i8289_slave_mask(unsigned int irq)
{
	outb(inb(0xa1) | (1 << (irq - OCTEON_IRQ_I8259S0)), 0xa1);
}

struct irq_chip octeon_irq_chip_i8259_slave = {
	.name = "i8259S",
	.mask = octeon_irq_i8289_slave_mask,
	.mask_ack = octeon_irq_i8289_slave_mask,
	.unmask = octeon_irq_i8289_slave_unmask,
	.eoi = octeon_irq_i8289_slave_unmask,
};

#ifdef CONFIG_PCI_MSI

static void octeon_irq_msi_ack(unsigned int irq)
{
	if (!octeon_has_feature(OCTEON_FEATURE_PCIE)) {
		/* These chips have PCI */
		cvmx_write_csr(CVMX_NPI_NPI_MSI_RCV,
			       1ull << (irq - OCTEON_IRQ_MSI_BIT0));
	} else {
		/* These chips have PCIe. Thankfully the ACK doesn't need any
		   locking */
		cvmx_write_csr(CVMX_PEXP_NPEI_MSI_RCV0,
			       1ull << (irq - OCTEON_IRQ_MSI_BIT0));
	}
}

static void octeon_irq_msi_eoi(unsigned int irq)
{
	/* Nothing needed */
}

static void octeon_irq_msi_enable(unsigned int irq)
{
	if (!octeon_has_feature(OCTEON_FEATURE_PCIE)) {
		/* Octeon PCI doesn't have the ability to mask/unmask MSI
		   interrupts individually. Instead of masking/unmasking them
		   in groups of 16, we simple assume MSI devices are well
		   behaved. MSI interrupts are always enable and the ACK is
		   assumed to be enough */
	} else {
		/* These chips have PCIe. Note that we only support the first
		   64 MSI interrupts. Unfortunately all the MSI enables are in
		   the same register. We use MSI0's lock to control access to
		   them all. */
		uint64_t en;
		unsigned long flags;
		spin_lock_irqsave(&octeon_irq_msi_lock, flags);
		en = cvmx_read_csr(CVMX_PEXP_NPEI_MSI_ENB0);
		en |= 1ull << (irq - OCTEON_IRQ_MSI_BIT0);
		cvmx_write_csr(CVMX_PEXP_NPEI_MSI_ENB0, en);
		cvmx_read_csr(CVMX_PEXP_NPEI_MSI_ENB0);
		spin_unlock_irqrestore(&octeon_irq_msi_lock, flags);
	}
}

static void octeon_irq_msi_disable(unsigned int irq)
{
	if (!octeon_has_feature(OCTEON_FEATURE_PCIE)) {
		/* See comment in enable */
	} else {
		/* These chips have PCIe. Note that we only support the first
		   64 MSI interrupts. Unfortunately all the MSI enables are in
		   the same register. We use MSI0's lock to control access to
		   them all. */
		uint64_t en;
		unsigned long flags;
		spin_lock_irqsave(&octeon_irq_msi_lock, flags);
		en = cvmx_read_csr(CVMX_PEXP_NPEI_MSI_ENB0);
		en &= ~(1ull << (irq - OCTEON_IRQ_MSI_BIT0));
		cvmx_write_csr(CVMX_PEXP_NPEI_MSI_ENB0, en);
		cvmx_read_csr(CVMX_PEXP_NPEI_MSI_ENB0);
		spin_unlock_irqrestore(&octeon_irq_msi_lock, flags);
	}
}

struct irq_chip octeon_irq_chip_msi = {
	.name = "MSI",
	.enable = octeon_irq_msi_enable,
	.disable = octeon_irq_msi_disable,
	.ack = octeon_irq_msi_ack,
	.eoi = octeon_irq_msi_eoi,
};
#endif

void __init arch_init_irq(void)
{
	int irq;

	if (NR_IRQS < OCTEON_IRQ_LAST)
		pr_err("octeon_irq_init: NR_IRQS is set too low\n");

	/* 0-7 Mips internal */
	for (irq = OCTEON_IRQ_SW0; irq <= OCTEON_IRQ_TIMER; irq++) {
		set_irq_chip_and_handler(irq, &octeon_irq_chip_core,
					 handle_percpu_irq);
	}

	/* 8-71 CIU_INT_SUM0 */
	for (irq = OCTEON_IRQ_WORKQ0; irq <= OCTEON_IRQ_BOOTDMA; irq++) {
		set_irq_chip_and_handler(irq, &octeon_irq_chip_ciu0,
					 handle_percpu_irq);
	}

	/* 72-135 CIU_INT_SUM1 */
	for (irq = OCTEON_IRQ_WDOG0; irq <= OCTEON_IRQ_RESERVED135; irq++) {
		set_irq_chip_and_handler(irq, &octeon_irq_chip_ciu1,
					 handle_percpu_irq);
	}

	/* 136 - 143 are reserved to align the i8259 in a multiple of 16. This
	   alignment is necessary since old style ISA interrupts hanging off
	   the i8259 have internal alignment assumptions */

	/* 144-151 i8259 master controller */
	for (irq = OCTEON_IRQ_I8259M0; irq <= OCTEON_IRQ_I8259M7; irq++) {
		set_irq_chip_and_handler(irq, &octeon_irq_chip_i8259_master,
					 handle_level_irq);
	}

	/* 152-159 i8259 slave controller */
	for (irq = OCTEON_IRQ_I8259S0; irq <= OCTEON_IRQ_I8259S7; irq++) {
		set_irq_chip_and_handler(irq, &octeon_irq_chip_i8259_slave,
					 handle_level_irq);
	}

#ifdef CONFIG_PCI_MSI
	/* 160-223 PCI/PCIe MSI interrupts */
	for (irq = OCTEON_IRQ_MSI_BIT0; irq <= OCTEON_IRQ_MSI_BIT63; irq++) {
		set_irq_chip_and_handler(irq, &octeon_irq_chip_msi,
					 handle_percpu_irq);
	}
#endif

	set_c0_status(0x300 << 2);
}
