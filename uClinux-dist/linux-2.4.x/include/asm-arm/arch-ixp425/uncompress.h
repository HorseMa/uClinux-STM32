/*
 * uncompress.h 
 *
 *  Copyright (C) 2002 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef _ARCH_UNCOMPRESS_H_
#define _ARCH_UNCOMPRESS_H_

#include <asm/hardware.h>
#include <asm/mach-types.h>
#include <linux/serial_reg.h>

static int console_output;
static	volatile u32* UART_BASE;
#define TX_DONE (UART_LSR_TEMT|UART_LSR_THRE)

static __inline__ void putc(char c)
{
	/* Check THRE and TEMT bits before we transmit the character.
	 */
	if (console_output) {
		while ((UART_BASE[UART_LSR] & TX_DONE) != TX_DONE); 
		*UART_BASE = c;
	}
#if defined(CONFIG_MACH_SG560) || defined(CONFIG_MACH_SG580) || \
    defined(CONFIG_MACH_ESS710) || defined(CONFIG_MACH_SG590) || \
    defined(CONFIG_MACH_IVPN)
	/* Tickle watchdog, we need to keep it happy while decompressing */
	*((volatile u32 *)(IXP425_GPIO_BASE_PHYS+IXP425_GPIO_GPOUTR_OFFSET)) ^=
		0x00004000;
#endif
#if defined(CONFIG_MACH_SG8100)
	*((volatile u32 *)(IXP425_GPIO_BASE_PHYS+IXP425_GPIO_GPOUTR_OFFSET)) ^=
		0x00002000;
#endif
#if defined(CONFIG_MACH_SG560USB) || defined(CONFIG_MACH_SG565) || \
    defined(CONFIG_MACH_SHIVA1100)
	*((volatile unsigned char *) IXP425_EXP_BUS_CS7_BASE_PHYS) = 0;
#endif
}

/*
 * This does not append a newline
 */
static void puts(const char *s)
{
	while (*s)
	{
		putc(*s);
		if (*s == '\n')
			putc('\r');
		s++;
	}
}

static __inline__ void arch_decomp_setup(void)
{
        unsigned long mach_type;

        asm("mov %0, r7" : "=r" (mach_type) ); 
	if(mach_type == MACH_TYPE_ADI_COYOTE)
		UART_BASE = IXP425_UART2_BASE_PHYS;
	else
		UART_BASE = IXP425_UART1_BASE_PHYS;
	if ((mach_type != MACH_TYPE_ESS710) && (mach_type != MACH_TYPE_IVPN) &&
	    (mach_type != MACH_TYPE_SG560) && (mach_type != MACH_TYPE_SG560USB) &&
	    (mach_type != MACH_TYPE_SG565) && (mach_type != MACH_TYPE_SG580) &&
	    (mach_type != MACH_TYPE_SHIVA1100) && (mach_type != MACH_TYPE_SG590) &&
	    (mach_type != MACH_TYPE_SG720) && (mach_type != MACH_TYPE_SG8100))
		console_output++;

}

#define arch_decomp_wdog()

#endif
