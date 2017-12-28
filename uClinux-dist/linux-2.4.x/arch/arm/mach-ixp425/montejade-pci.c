/*
 * arch/arm/mach-ixp425/montejade-pci.c 
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

#include <asm/mach/pci.h>
#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/arch/pci.h>


/* PCI controller pin mappings */
#define INTA_PIN	IXP425_GPIO_PIN_6
#define INTB_PIN	IXP425_GPIO_PIN_7

void __init montejade_pci_init(void *sysdata)
{
	printk("PCI: reset bus...\n");
	gpio_line_set(IXP425_GPIO_PIN_8, 0);
	gpio_line_config(IXP425_GPIO_PIN_8, IXP425_GPIO_OUT);
	gpio_line_set(IXP425_GPIO_PIN_8, 0);
	mdelay(50);
	gpio_line_set(IXP425_GPIO_PIN_8, 1);
	mdelay(50);

	gpio_line_config(INTA_PIN, IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_LOW);
	gpio_line_config(INTB_PIN, IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_LOW);
	gpio_line_isr_clear(INTA_PIN);
	gpio_line_isr_clear(INTB_PIN);

	ixp425_pci_init(sysdata);
}

static int __init montejade_map_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
	if (slot == 14)
		return IRQ_MONTEJADE_PCI_INTA;
	else if (slot == 12)
		return IRQ_MONTEJADE_PCI_INTB;
	else if (slot == 13)
		return IRQ_MONTEJADE_PCI_INTB;
	return -1;
}

struct hw_pci montejade_pci __initdata = {
	init:		montejade_pci_init,
	swizzle:	common_swizzle,
	map_irq:	montejade_map_irq,
};

