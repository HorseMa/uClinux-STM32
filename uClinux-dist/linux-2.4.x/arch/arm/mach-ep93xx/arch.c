/*
 *  linux/arch/arm/mach-ep93xx/arch.c
 *
 *  Copyright (C) 2000 Deep Blue Solutions Ltd
 * (c) Copyright 2002-2003 Cirrus Logic, Inc., Austin, Tx 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/config.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/utsname.h>

#include <asm/irq.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/hardware.h>
#include <asm/io.h>

#include <asm/memory.h>
#include <asm/arch/platform.h>

#include <asm/mach/arch.h>

#include <asm/arch/ssp.h>

extern void ep93xx_map_io(void);
extern void ep93xx_init_irq(void);

static int __init
ep93xx_init(void)
{
#ifndef CONFIG_MACH_IPD
	unsigned int uiTemp;
	int SSP_Handle;

	/*
	 * Enable halt mode.
	 */
	uiTemp = inl(SYSCON_DEVCFG) | SYSCON_DEVCFG_SHena;
	SysconSetLocked(SYSCON_DEVCFG, uiTemp);

	/*
	 * Set the bus priority.
	 */
	outl(BMAR_PRIORD_02, SYSCON_BMAR);

	/*
	 * Get the hostname from the SPI FLASH if it has been programmed.
	 */
	SSP_Handle = SSPDriver->Open(SERIAL_FLASH, 0);
	if(SSP_Handle != -1) {
		SSPDriver->Read( SSP_Handle, 0x1000, &uiTemp );
		if (uiTemp == 0x43414d45) {
			SSPDriver->Read( SSP_Handle, 0x1010, &uiTemp );
			system_utsname.nodename[0] = uiTemp & 255;
			system_utsname.nodename[1] = (uiTemp >> 8) & 255;
			system_utsname.nodename[2] = (uiTemp >> 16) & 255;
			system_utsname.nodename[3] = uiTemp >> 24;
			SSPDriver->Read( SSP_Handle, 0x1014, &uiTemp );
			system_utsname.nodename[4] = uiTemp & 255;
			system_utsname.nodename[5] = (uiTemp >> 8) & 255;
			system_utsname.nodename[6] = (uiTemp >> 16) & 255;
			system_utsname.nodename[7] = uiTemp >> 24;
			SSPDriver->Read( SSP_Handle, 0x1018, &uiTemp );
			system_utsname.nodename[8] = uiTemp & 255;
			system_utsname.nodename[9] = (uiTemp >> 8) & 255;
			system_utsname.nodename[10] = (uiTemp >> 16) & 255;
			system_utsname.nodename[11] = uiTemp >> 24;
			SSPDriver->Read( SSP_Handle, 0x101c, &uiTemp );
			system_utsname.nodename[12] = uiTemp & 255;
			system_utsname.nodename[13] = (uiTemp >> 8) & 255;
			system_utsname.nodename[14] = (uiTemp >> 16) & 255;
			system_utsname.nodename[15] = uiTemp >> 24;
			system_utsname.nodename[16] = 0;
		}
		SSPDriver->Close( SSP_Handle );
	}
#endif /* CONFIG_MACH_IPD */
}

__initcall(ep93xx_init);
  
/* Machine Descriptor created by macros in asm-arm/mach/arch.h
    bootmem: phys ram base, phys io base, virt io base
    bootpara: phys param base  */

#ifdef CONFIG_ARCH_EDB9301
  MACHINE_START(EDB9301, "edb9301")
      MAINTAINER("Cirrus Logic")
    BOOT_MEM( 0x00000000, 0x80000000, 0xff000000 )
    BOOT_PARAMS( 0x00000100 )
    MAPIO(ep93xx_map_io)
    INITIRQ(ep93xx_init_irq)
  MACHINE_END
#endif

#ifdef CONFIG_ARCH_EDB9302
  MACHINE_START(EDB9302, "edb9302")
    MAINTAINER("Cirrus Logic")
    BOOT_MEM( 0x00000000, 0x80000000, 0xff000000 )
    BOOT_PARAMS( 0x00000100 )
      MAPIO(ep93xx_map_io)
      INITIRQ(ep93xx_init_irq)
  MACHINE_END
#endif
  
#ifdef CONFIG_ARCH_EDB9312
  MACHINE_START(EDB9312, "edb9312")
      MAINTAINER("Cirrus Logic")
    BOOT_MEM( 0x00000000, 0x80000000, 0xff000000 )
    BOOT_PARAMS( 0x00000100 )
      MAPIO(ep93xx_map_io)
      INITIRQ(ep93xx_init_irq)
  MACHINE_END
  #endif
  
#ifdef CONFIG_ARCH_EDB9315
  MACHINE_START(EDB9315, "edb9315")
      MAINTAINER("Cirrus Logic")
    BOOT_MEM( 0x00000000, 0x80000000, 0xff000000 )
    BOOT_PARAMS( 0x00000100 )
      MAPIO(ep93xx_map_io)
      INITIRQ(ep93xx_init_irq)
  MACHINE_END
#endif

#ifdef CONFIG_MACH_IPD
  MACHINE_START(IPD, "ipd")
      MAINTAINER("Cyberguard Inc.")
    BOOT_MEM( SDRAM_START, 0x80000000, 0xff000000 )
    BOOT_PARAMS( SDRAM_START + 0x00000100 )
      MAPIO(ep93xx_map_io)
      INITIRQ(ep93xx_init_irq)
  MACHINE_END
#endif

