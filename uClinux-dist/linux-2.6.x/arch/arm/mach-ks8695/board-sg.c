/*
 * arch/arm/mach-ks8695/board-sg.c
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

static void __init micrel_init(void)
{
	/* Add devices */
#if 0
	ks8695_add_device_wan();	/* eth0 = WAN */
	ks8695_add_device_lan();	/* eth1 = LAN */
#endif
}

#ifdef CONFIG_MACH_LITE300
MACHINE_START(LITE300, "SecureComputing/SG300")
	/* Secure Computing Corporation */
	.phys_io	= KS8695_IO_PA,
	.io_pg_offst	= (KS8695_IO_VA >> 18) & 0xfffc,
	.boot_params	= 0x100,
	.map_io		= ks8695_map_io,
	.init_irq	= ks8695_init_irq,
	.init_machine	= micrel_init,
	.timer		= &ks8695_timer,
MACHINE_END
#endif

#ifdef CONFIG_MACH_SG310
MACHINE_START(SG310, "SecureComputing/SG310")
	/* Secure Computing Corporation */
	.phys_io	= KS8695_IO_PA,
	.io_pg_offst	= (KS8695_IO_VA >> 18) & 0xfffc,
	.boot_params	= 0x100,
	.map_io		= ks8695_map_io,
	.init_irq	= ks8695_init_irq,
	.init_machine	= micrel_init,
	.timer		= &ks8695_timer,
MACHINE_END
#endif

#ifdef CONFIG_MACH_PFW
MACHINE_START(PFW, "SecureComputing/PFW")
	/* Secure Computing Corporation */
	.phys_io	= KS8695_IO_PA,
	.io_pg_offst	= (KS8695_IO_VA >> 18) & 0xfffc,
	.boot_params	= 0x100,
	.map_io		= ks8695_map_io,
	.init_irq	= ks8695_init_irq,
	.init_machine	= micrel_init,
	.timer		= &ks8695_timer,
MACHINE_END
#endif

#ifdef CONFIG_MACH_SE4200
MACHINE_START(SE4200, "SecureComputing/SE4200")
	/* Secure Computing Corporation */
	.phys_io	= KS8695_IO_PA,
	.io_pg_offst	= (KS8695_IO_VA >> 18) & 0xfffc,
	.boot_params	= 0x100,
	.map_io		= ks8695_map_io,
	.init_irq	= ks8695_init_irq,
	.init_machine	= micrel_init,
	.timer		= &ks8695_timer,
MACHINE_END
#endif

