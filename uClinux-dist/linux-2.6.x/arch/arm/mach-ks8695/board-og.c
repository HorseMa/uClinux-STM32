/*
 * arch/arm/mach-ks8695/board-og.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>
#include <asm/arch/devices.h>
#include "generic.h"

#ifdef CONFIG_PCI
static int micrel_pci_map_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
	return KS8695_IRQ_EXTERN0;
}

static struct ks8695_pci_cfg __initdata micrel_pci = {
	.mode		= KS8695_MODE_PCI,
	.map_irq	= micrel_pci_map_irq,
};
#endif

static void __init micrel_init(void)
{
#ifdef CONFIG_PCI
	ks8695_init_pci(&micrel_pci);
#endif

	/* Add devices */
#if 0
	ks8695_add_device_wan();	/* eth0 = WAN */
	ks8695_add_device_lan();	/* eth1 = LAN */
#endif
}

MACHINE_START(CM4002, "OpenGear/CM4002")
	/* OpenGear Inc. */
	.phys_io	= KS8695_IO_PA,
	.io_pg_offst	= (KS8695_IO_VA >> 18) & 0xfffc,
	.boot_params	= 0x100,
	.map_io		= ks8695_map_io,
	.init_irq	= ks8695_init_irq,
	.init_machine	= micrel_init,
	.timer		= &ks8695_timer,
MACHINE_END
#endif

#ifdef CONFIG_MACH_CM4008
MACHINE_START(CM4008, "OpenGear/CM4008")
	/* OpenGear Inc. */
	.phys_io	= KS8695_IO_PA,
	.io_pg_offst	= (KS8695_IO_VA >> 18) & 0xfffc,
	.boot_params	= 0x100,
	.map_io		= ks8695_map_io,
	.init_irq	= ks8695_init_irq,
	.init_machine	= micrel_init,
	.timer		= &ks8695_timer,
MACHINE_END
#endif

#ifdef CONFIG_MACH_CM41xx
MACHINE_START(CM41xx, "OpenGear/CM41xx")
	/* OpenGear Inc. */
	.phys_io	= KS8695_IO_PA,
	.io_pg_offst	= (KS8695_IO_VA >> 18) & 0xfffc,
	.boot_params	= 0x100,
	.map_io		= ks8695_map_io,
	.init_irq	= ks8695_init_irq,
	.init_machine	= micrel_init,
	.timer		= &ks8695_timer,
MACHINE_END
#endif
