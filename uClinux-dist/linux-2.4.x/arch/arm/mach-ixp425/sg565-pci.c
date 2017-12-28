/*
 * arch/arm/mach-ixp425/sg565-pci.c 
 *
 * SecureComputing/SG565 PCI initialization
 *
 * Copyright (C) 2002 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
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
#define INTA_PIN	IXP425_GPIO_PIN_8

void __init sg565_pci_init(void *sysdata)
{
	gpio_line_config(INTA_PIN, IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_LOW);
	gpio_line_isr_clear(INTA_PIN);
	ixp425_pci_init(sysdata);
}

static int __init sg565_map_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
	if ((slot == 12) || (slot == 14))
		return IRQ_SG565_PCI_INTA;
	return -1;
}

struct hw_pci sg565_pci __initdata = {
	init:		sg565_pci_init,
	swizzle:	common_swizzle,
	map_irq:	sg565_map_irq,
};

