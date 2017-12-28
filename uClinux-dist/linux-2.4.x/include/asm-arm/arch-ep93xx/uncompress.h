/*
 *  linux/include/asm-arm/arch-ep93xx/uncompress.h
 *
 *  Copyright (C) 1999 ARM Limited
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

#include <asm/io.h>
#include <asm/hardware.h>

#undef EP93XX_APB_BASE
#define EP93XX_APB_BASE (IO_BASE_PHYS | 0x00800000)

#ifdef CONFIG_MACH_IPD
#define _BAUDRATE 115200
#define UART(reg)	UART2##reg
#endif

#ifndef UART
#define UART(reg)	UART1##reg
#endif

#ifndef _BAUDRATE
#define _BAUDRATE 57600
#endif

#define BAUDRATE ((14745600 / (16 * _BAUDRATE)) - 1)

static void puts(const char *s)
{
	while (*s)
	{
		while (inl(UART(FR)) & UARTFR_TXFF)
			barrier();

		outl(*s, UART(DR));
        
		if (*s == '\n') {
			while (inl(UART(FR)) & UARTFR_TXFF)
				barrier();
	    
			outl('\r', UART(DR));
		}
		s++;
	}
	while(inl(UART(FR)) & UARTFR_BUSY)
		barrier();
}

#define arch_decomp_setup()

#ifdef CONFIG_MACH_IPD
#define arch_decomp_wdog() (* (volatile unsigned char *) 0x20000000)
#else
#define arch_decomp_wdog()
#endif
