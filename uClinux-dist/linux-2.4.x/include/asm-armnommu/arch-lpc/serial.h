#ifndef __ASM_ARCH_SERIAL_H
#define __ASM_ARCH_SERIAL_H

#include <asm/arch/hardware.h>
#include <asm/irq.h>

#define RS_TABLE_SIZE 2
#define BASE_BAUD 115200
#define STD_COM_FLAGS (ASYNC_BOOT_AUTOCONF | ASYNC_SKIP_TEST)

#define STD_SERIAL_PORT_DEFNS \
	{	\
	type : PORT_16550A, \
	xmit_fifo_size : 16, \
	baud_base: BASE_BAUD, \
	irq: IRQ_UART0, \
	flags : STD_COM_FLAGS, \
	iomem_base : (u8*)LPC_UART0_BASE, \
	io_type : SERIAL_IO_MEM, \
	iomem_reg_shift: 2 \
	},	\
	{ \
	type : PORT_16550A, \
	xmit_fifo_size : 16, \
	baud_base: BASE_BAUD, \
	irq: IRQ_UART1, \
	flags: STD_COM_FLAGS, \
	iomem_base : (u8*)LPC_UART1_BASE, \
	io_type : SERIAL_IO_MEM, \
	iomem_reg_shift: 2 \
	}

unsigned int baudrate_div(unsigned int baudrate)
{
	return ((Fpclk / 16) / baudrate);
}

#define EXTRA_SERIAL_PORT_DEFNS
#endif
