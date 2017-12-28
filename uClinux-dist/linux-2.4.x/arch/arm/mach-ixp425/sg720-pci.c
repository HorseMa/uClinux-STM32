/*
 * arch/arm/mach-ixp425/sg720-pci.c 
 *
 * (C) Copyright 2006, SnapGear - A division of Secure Computing.
 * (C) Copyright 2004, SnapGear - A CyberGuard Company.

 * This code based on the coyote-pci.c code, which was:
 *
 * Copyright (C) 2002 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/config.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/delay.h>

#include <asm/types.h>
#include <asm/setup.h>
#include <asm/hardware.h>
#include <asm/memory.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/pci.h>
#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/arch/pci.h>


/* PCI controller pin mappings */
#define INTA_PIN	IXP425_GPIO_PIN_8
#define INTB_PIN	IXP425_GPIO_PIN_9

void __init sg720_pci_init(void *sysdata)
{
	/*
	 * Check the stepping of the IXP465 CPU. If it is not A0 or A1 then
	 * enable the MPI port for direct memory bus access. Much faster.
	 * This is broken on older steppings, so don't enable.
	 */
	if (cpu_is_ixp46x()) {
		unsigned long processor_id;
		asm("mrc p15, 0, %0, cr0, cr0, 0;" : "=r"(processor_id) :);
		if ((processor_id & 0xf) >= 2) {
			printk("MPI: enabling fast memory bus...\n");
			*IXP425_EXP_CFG1 |= 0x80000000;
		} else {
			printk("MPI: disabling fast memory bus...\n");
			*IXP425_EXP_CFG1 &= ~0x80000000;
		}
	}

	printk("PCI: reset bus...\n");
	gpio_line_set(IXP425_GPIO_PIN_13, 0);
	gpio_line_config(IXP425_GPIO_PIN_13, IXP425_GPIO_OUT);
	gpio_line_set(IXP425_GPIO_PIN_13, 0);
	mdelay(50);
	gpio_line_set(IXP425_GPIO_PIN_13, 1);
	mdelay(50);

	gpio_line_config(INTA_PIN, IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_LOW);
	gpio_line_config(INTB_PIN, IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_LOW);

	gpio_line_isr_clear(INTA_PIN);
	gpio_line_isr_clear(INTB_PIN);

	ixp425_pci_init(sysdata);
}

static int __init sg720_map_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
#if defined(CONFIG_MACH_SG590)
	if (slot == 12)
		return IRQ_SG590_PCI_INTA;
	else if (slot == 13)
		return IRQ_SG590_PCI_INTA;
#elif defined(CONFIG_MACH_SG720)
	if (slot == 12)
		return IRQ_SG720_PCI_INTB;
	else if (slot == 13)
		return IRQ_SG720_PCI_INTB;
	else if (slot == 14)
		return IRQ_SG720_PCI_INTA;
	else if (slot == 15)
		return IRQ_SG720_PCI_INTA;
#endif
	return -1;
}

struct hw_pci sg720_pci __initdata = {
	init:		sg720_pci_init,
	swizzle:	common_swizzle,
	map_irq:	sg720_map_irq,
};

