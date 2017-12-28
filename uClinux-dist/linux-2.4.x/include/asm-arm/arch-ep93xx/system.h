/*
 *  linux/include/asm-arm/arch-ep93xx/system.h
 *
 *  Copyright (C) 1999 ARM Limited
 *  Copyright (C) 2000 Deep Blue Solutions Ltd
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
#ifndef __ASM_ARCH_SYSTEM_H
#define __ASM_ARCH_SYSTEM_H

#include <asm/arch/platform.h>
#include <asm/hardware.h>
#include <asm/io.h>

static void arch_idle(void)
{
	unsigned long ulTemp;

	ulTemp = inl(SYSCON_HALT);
}

extern __inline__ void arch_reset(char mode)
{
	//
	// Disable the peripherals.
	//
	outl(0xffffffff, VIC0INTENCLEAR);
	outl(0xffffffff, VIC1INTENCLEAR);
	outl(0, DMAMP_TX_0_CONTROL);
	outl(0, DMAMP_RX_1_CONTROL);
	outl(0, DMAMP_TX_2_CONTROL);
	outl(0, DMAMP_RX_3_CONTROL);
	outl(0, DMAMM_0_CONTROL);
	outl(0, DMAMM_1_CONTROL);
	outl(0, DMAMP_TX_4_CONTROL);
	outl(0, DMAMP_RX_5_CONTROL);
	outl(0, DMAMP_TX_6_CONTROL);
	outl(0, DMAMP_RX_7_CONTROL);
	outl(0, DMAMP_TX_8_CONTROL);
	outl(0, DMAMP_RX_9_CONTROL);
	outl(1, MAC_SELFCTL);
	while(inl(MAC_SELFCTL) & 1)
		barrier();
	outl(1, HCCOMMANDSTATUS);
	while(inl(HCCOMMANDSTATUS) & 1)
		barrier();
	outl(0, IrEnable);
	outl(0, UART1CR);
	outl(0, UART2CR);
	outl(0, I2STX0En);
	outl(0, I2SRX0En);
	outl(0, AC97GCR);
	outl(0, SSPCR1);
#ifdef CONFIG_ARCH_EP9315
	outl(0, SMC_PCMCIACtrl);
	outl(0, BLOCKCTRL);
#endif
#if defined(CONFIG_ARCH_EP9312) || defined(CONFIG_ARCH_EP9315)
	outl(0, VIDEOATTRIBS);
	outl(0, UART3CR);
	outl(0, I2STX1En);
	outl(0, I2SRX1En);
	outl(0, I2STX2En);
	outl(0, I2SRX2En);
	outl(0, TSSetup);
	outl(0, IDECFG);
	outl(0xaa, SYSCON_SWLOCK);
	outl(0, SYSCON_VIDDIV);
	outl(0xaa, SYSCON_SWLOCK);
	outl(0, SYSCON_KTDIV);
#endif
	outl(0xaa, SYSCON_SWLOCK);
	outl(0, SYSCON_MIRDIV);
	outl(0xaa, SYSCON_SWLOCK);
	outl(0, SYSCON_I2SDIV);
	outl(0, SYSCON_PWRCNT);
	outl(0xaa, SYSCON_SWLOCK);
	outl(0, SYSCON_DEVCFG);
	outl(0x000398e7, SYSCON_CLKSET1);
	inl(SYSCON_CLKSET1);
	__asm__ __volatile__("nop");
	__asm__ __volatile__("nop");
	__asm__ __volatile__("nop");
	__asm__ __volatile__("nop");
	__asm__ __volatile__("nop");
	outl(0x0003c317, SYSCON_CLKSET2);
	__asm__ __volatile__("nop");
	__asm__ __volatile__("nop");
	__asm__ __volatile__("nop");
	__asm__ __volatile__("nop");
	__asm__ __volatile__("nop");
	outl(0, GPIO_PADDR);
	outl(0, GPIO_PBDDR);
	outl(0, GPIO_PCDDR);
#if defined(CONFIG_ARCH_EP9312) || defined(CONFIG_ARCH_EP9315)
	outl(0, GPIO_PDDDR);
#endif
	outl(0x3, GPIO_PEDR);
	outl(0x3, GPIO_PEDDR);
	outl(0, GPIO_PFDDR);
	outl(0, GPIO_PGDR);
	outl(0xc, GPIO_PGDDR);
	outl(0, GPIO_PHDDR);
	outl(0, GPIO_PDDDR);
	outl(0, GPIO_PDDDR);
	outl(0, GPIO_AINTEN);
	outl(0, GPIO_BINTEN);
	outl(0, GPIO_FINTEN);
	outl(0, GPIO_EEDRIVE);

	//
	// Reset the CPU, branching to the appropriate location based on the
	// boot mode.
	//
	cpu_reset((inl(SYSCON_SYSCFG) & SYSCON_SYSCFG_LEECK) ?
		  0x80090000 : 0x0);
}
#endif
