/*
 * uclinux/include/asm-armnommu/arch-isl3893/serial.h
 */
/*  $Header$
 *
 *  Copyright (C) 2002 Intersil Americas Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef __ASM_ARCH_SERIAL_H
#define __ASM_ARCH_SERIAL_H

#include <linux/serial_reg.h>
#include <asm/arch/irqs.h>
#include <asm/arch/gpio.h>
#include <asm/arch/uart.h>

#ifndef __ASSEMBLER__

/*
 * Definitions used in drivers/char/serial.c
 */

#define STD_COM_FLAGS (ASYNC_BOOT_AUTOCONF | ASYNC_SKIP_TEST)

#define BASE_BAUD ( 1843200 / 16 )

#define RS_TABLE_SIZE  1
#define STD_SERIAL_PORT_DEFNS \
	{  \
	magic: 0, \
	baud_base: BASE_BAUD, \
	port: (unsigned long)uUART, \
	irq: IRQ_UART, \
	flags: STD_COM_FLAGS \
	}       /* ttyS0 */
#define EXTRA_SERIAL_PORT_DEFNS

static inline void uart_initialize(void)
{
	unsigned baud_div = (66000000 / BASE_BAUD + 8) / 16;

	uGPIO3->FSEL = gpio_set( GPIO3_MASK_UART_RX | GPIO3_MASK_UART_TX );
	/* Load the Divisor Latch. Note that Register0 and InterruptEnable
	 * are shared registers.
	 */
	uUART->LineControl = UARTLineControlDLAB;
	uUART->Register0 = baud_div;
	uUART->InterruptEnable = baud_div >> 8;
	uUART->ModemControl = 0;
	uUART->LineControl = 3;	// 8 bits, 1 stop, no parity
}

static inline unsigned char uart_rx(void)
{
	return (unsigned char)uUART->Register0;
}

static inline void uart_tx(unsigned char ch)
{
	uUART->Register0 = (unsigned int)(ch & 0xff);
}

#define UART_IRQSTAT_RX(status)		((status & 0x7) == UARTInterrupIDRBFI)
#define UART_IRQSTAT_TX(status)		((status & 0x7) == UARTInterrupIDTBEI)
#define UART_IRQSTAT_ANY(status)	(UART_IRQSTAT_RX(status) || UART_IRQSTAT_TX(status))

static inline unsigned int uart_read_irqstat(void)
{
	return(uUART->InterrupID & 0x7);
}

static inline int uart_read_lsr(void)
{
	unsigned char uart_status;
	int status = 0;

	uart_status = uUART->LineStatus;
	if (uart_status & UARTLineStatusOE)
		status |= UART_LSR_OE;
	if (uart_status & UARTLineStatusBI)
		status |= UART_LSR_BI;
	if (uart_status & UARTLineStatusPE)
		status |= UART_LSR_PE;
	if (uart_status & UARTLineStatusFE)
		status |= UART_LSR_FE;
	if (uart_status & UARTLineStatusTEMT)
		status |= UART_LSR_TEMT;
	if (uart_status & UARTLineStatusTHRE)
		status |= UART_LSR_THRE;
	if (uart_status & UARTLineStatusDR)
		status |= UART_LSR_DR;

	return status;
}

static inline void uart_clear_errors(void)
{
	/* Errors need not be cleared */
}

static inline unsigned int uart_save_ier(void)
{
	return(uUART->InterruptEnable);
}

static inline void uart_restore_ier(unsigned int ier)
{
	uUART->InterruptEnable = ier;
}

static inline void uart_interrupt_enable(int interrupt)
{
	if(interrupt == UART_IER_THRI)		/* Transmitter holding register int. */
		uUART->InterruptEnable |= UARTInterruptEnableTBEI;
	else if(interrupt == UART_IER_RDI)	/* Enable receiver data interrupt */
		uUART->InterruptEnable |= UARTInterruptEnableRBFI;
	else
		printk("uart_interrupt_enable: Unknown Interrupt %d", interrupt);
}

static inline void uart_interrupt_disable(int interrupt)
{
	if(interrupt == UART_IER_THRI)		/* Transmitter holding register int. */
		uUART->InterruptEnable &= ~UARTInterruptEnableTBEI;
	else if(interrupt == UART_IER_RDI)	/* Enable receiver data interrupt */
		uUART->InterruptEnable &= ~UARTInterruptEnableRBFI;
	else
		printk("uart_interrupt_disable: Unknown Interrupt %d", interrupt);
}

#endif /* __ASSEMBLER__ */

#endif /* __ASM_ARCH_SERIAL_H */
