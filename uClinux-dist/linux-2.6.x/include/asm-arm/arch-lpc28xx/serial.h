/*
 * include/asm-arm/arch-lpc28xx/serial.h
 *
 * Copyright (C) 2004 Philips Semiconductors
 */

#ifndef __ASM_ARCH_SERIAL_H__
#define __ASM_ARCH_SERIAL_H__


#define BASE_BAUD  (60000000 / 16)
#define UART0_BASE  0x80101000

/* Standard COM flags */
#define STD_COM_FLAGS (ASYNC_BOOT_AUTOCONF | ASYNC_SKIP_TEST)

#define RS_TABLE_SIZE 2

/*
 * Rather empty table...
 * Hardwired serial ports should be defined here.
 * PCMCIA will fill it dynamically.
 */
#define STD_SERIAL_PORT_DEFNS	\
       /* UART	CLK     	PORT		IRQ	FLAGS		*/ \
	{ 0,	BASE_BAUD,	UART0_BASE, 	12,	STD_COM_FLAGS,	\
	.iomem_reg_shift = 2,		\
	.iomem_base = UART0_BASE,	\
	.io_type = UPIO_MEM}

#define EXTRA_SERIAL_PORT_DEFNS

#define SERIAL_PORT_DFNS	\
	STD_SERIAL_PORT_DEFNS	\
	EXTRA_SERIAL_PORT_DEFNS

#endif /* __ASM_ARCH_SERIAL_H__ */
