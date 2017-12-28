/*
 *  linux/include/asm-arm/arch-realview/uncompress.h
 *
 *  Copyright (C) 2003 ARM Limited
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
#include <asm/hardware.h>
#include <asm/mach-types.h>

#include <asm/arch/board-eb.h>
#define	UART01x_DR	USART1_BASE+0x04
#define	UART01x_FR	USART1_BASE+0x00
#define	UART01x_FR_TXE	0x0080

#include <driver/stm32/stm32f10x_usart.h>

#define AMBA_UART_DR(base)	(*(volatile unsigned char *)((base) + 0x04))
#define AMBA_UART_LCRH(base)	(*(volatile unsigned char *)((base) + 0x2c))
#define AMBA_UART_CR(base)	(*(volatile unsigned char *)((base) + 0x30))
#define AMBA_UART_FR(base)	(*(volatile unsigned char *)((base) + 0x00))

/*
 * Return the UART base address
 */
static inline unsigned long get_uart_base(void)
{
	if (machine_is_realview_eb())
		return USART1_BASE;
	return 0;
}

/*
 * This does not append a newline
 */
static inline void putc(int c)
{
	unsigned long base = get_uart_base();

	while (!(AMBA_UART_FR(base) & (1 << 8)))
		barrier();

	AMBA_UART_DR(base) = c;
}

static inline void flush(void)
{
	unsigned long base = get_uart_base();

	while (AMBA_UART_FR(base) & USART_FLAG_RXNE)
		barrier();
}

/*
 * nothing to do
 */
#define arch_decomp_setup()
#define arch_decomp_wdog()
