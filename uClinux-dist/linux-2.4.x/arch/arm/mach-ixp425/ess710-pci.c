/*
 * arch/arm/mach-ixp425/ess710-pci.c 
 *
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
#include <asm/arch/pci.h>


/* PCI controller pin mappings */
#define INTA_PIN	IXP425_GPIO_PIN_6
#define INTB_PIN	IXP425_GPIO_PIN_7
#define INTC_PIN	IXP425_GPIO_PIN_8

#define IXP425_PCI_RESET_GPIO   IXP425_GPIO_PIN_12
#define IXP425_PCI_CLK_PIN      IXP425_GPIO_CLK_0
#define IXP425_PCI_CLK_ENABLE   IXP425_GPIO_CLK0_ENABLE
#define IXP425_PCI_CLK_TC_LSH   IXP425_GPIO_CLK0TC_LSH
#define IXP425_PCI_CLK_DC_LSH   IXP425_GPIO_CLK0DC_LSH

void __init ess710_pci_init(void *sysdata)
{
	printk("PCI: reset bus...\n");
	gpio_line_set(IXP425_GPIO_PIN_13, 0);
	gpio_line_config(IXP425_GPIO_PIN_13, IXP425_GPIO_OUT);
	gpio_line_set(IXP425_GPIO_PIN_13, 0);
	mdelay(50);
	gpio_line_set(IXP425_GPIO_PIN_13, 1);
	mdelay(50);

	gpio_line_config(INTA_PIN, IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_LOW);
	gpio_line_config(INTB_PIN, IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_LOW);
	gpio_line_config(INTC_PIN, IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_LOW);

	gpio_line_isr_clear(INTA_PIN);
	gpio_line_isr_clear(INTB_PIN);
	gpio_line_isr_clear(INTC_PIN);

	ixp425_pci_init(sysdata);
}

static int __init ess710_map_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
	if (slot == 16)
		return IRQ_ESS710_PCI_INTA;
	else if (slot == 15)
		return IRQ_ESS710_PCI_INTB;
	else if (slot == 14)
		return IRQ_ESS710_PCI_INTC;
	else if (slot == 13)
		return IRQ_ESS710_PCI_INTC;
	else return -1;
}

struct hw_pci ess710_pci __initdata = {
	init:		ess710_pci_init,
	swizzle:	common_swizzle,
	map_irq:	ess710_map_irq,
};

/*
 *	Hard set the ESS710 memory size to be 128M. Early boot loaders
 *	passed in 64MB in their boot tags, but now we really can use the
 *	128M that the hardware has.
 */
void ess710_memsize(struct machine_desc *mdesc, struct param_struct *tags, char **cmdline, struct meminfo *mi)
{
	mi->nr_banks = 1;
	mi->bank[0].start = PHYS_OFFSET;
	mi->bank[0].size = (128*1024*1024);
}

