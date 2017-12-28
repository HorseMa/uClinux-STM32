/*
 * arch/arm/mach-ixp425/sg8100-pci.c 
 *
 * SecureComputing/SG8100 PCI initialization
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

void __init sg8100_pci_init(void *sysdata)
{
	gpio_line_config(INTA_PIN, IXP425_GPIO_IN | IXP425_GPIO_ACTIVE_LOW);
	gpio_line_isr_clear(INTA_PIN);
	ixp425_pci_init(sysdata);
}

static int __init sg8100_map_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
	if (slot == 16)
		return IRQ_SG8100_PCI_INTA;
	return -1;
}

void sg8100_cardbus_fixup(struct pci_controller *hose, int current_bus, int pci_devfn)
{
	unsigned long ba;
	unsigned int scr, mfr;
	unsigned short bcr;
	unsigned char dc;

	/* Leave the cardbus slots in the reset state for now */
	bcr = 0x0340;
	early_write_config_word(hose, current_bus, pci_devfn, 0x3e, bcr);

	/* Set to use serialized interrupts - the power on default */
	dc = 0x66;
	early_write_config_byte(hose, current_bus, pci_devfn, 0x92, dc);

	/* Enable MFUNC0 to be interrupt source for slot */
	scr = 0x28449060;
	early_write_config_dword(hose, current_bus, pci_devfn, 0x80, scr);

	mfr = 0x00001002;
	early_write_config_dword(hose, current_bus, pci_devfn, 0x8c, mfr);

	/* Turn of power to cardbus slot */
	ba = 0;
	early_read_config_dword(hose, current_bus, pci_devfn, PCI_BASE_ADDRESS_0, &ba);
	if (ba) {
		/* Request power off on cardbus slot */
		writel(0, ba+0x4); /* MASK */
		writel(0, ba+0x10); /* CONTROL */
	}
}

struct hw_pci sg8100_pci __initdata = {
	init:		sg8100_pci_init,
	swizzle:	common_swizzle,
	map_irq:	sg8100_map_irq,
};

