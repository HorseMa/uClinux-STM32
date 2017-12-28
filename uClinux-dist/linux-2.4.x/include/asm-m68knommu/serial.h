/*
 * linux/include/asm-m68knommu/serial.h
 *
 * Copyright (C) 2003 Develer S.r.l. (http://www.develer.com/)
 * Author: Bernardo Innocenti <bernie@codewiz.org>
 *
 * Based on linux/include/asm-i386/serial.h
 */
#include <linux/config.h>


#if defined(CONFIG_SERIAL_CDB4)
/*
 * This assumes you have a 1.8432 MHz clock for your UART.
 *
 * It'd be nice if someone built a serial card with a 24.576 MHz
 * clock, since the 16550A is capable of handling a top speed of 1.5
 * megabits/second; but this requires the faster clock.
 */
#define BASE_BAUD ( 1843200 / 16 )
#define CONFIG_SERIAL_SHARE_IRQ
#define CDB4_COM_BASE		((u8 *)0x40000000)
#define CDB4_COM_IRQ		67	/* external IRQ3 */
#define COM_IRQPRI		5	/* interrupt priority */
#define CDB4_COM_RESET_BIT	13	/* PA13 hooked to 16C554 RESET line, active high */
#define STD_COM_FLAGS		ASYNC_BOOT_AUTOCONF

#define SERIAL_PORT_DFNS				\
	{						\
		.baud_base	= BASE_BAUD,		\
		.iomem_base	= CDB4_COM_BASE + 0x10,	\
		.io_type	= SERIAL_IO_MEM,	\
		.irq		= CDB4_COM_IRQ,		\
		.flags		= STD_COM_FLAGS		\
	},						\
	{						\
		.baud_base	= BASE_BAUD,		\
		.iomem_base	= CDB4_COM_BASE + 0x18,	\
		.io_type	= SERIAL_IO_MEM,	\
		.irq		= CDB4_COM_IRQ,		\
		.flags		= STD_COM_FLAGS		\
	},						\
	{						\
		.baud_base	= BASE_BAUD,		\
		.iomem_base	= CDB4_COM_BASE + 0x20,	\
		.io_type	= SERIAL_IO_MEM,	\
		.irq		= CDB4_COM_IRQ,		\
		.flags		= STD_COM_FLAGS		\
	},						\
	{						\
		.baud_base	= BASE_BAUD,		\
		.iomem_base	= CDB4_COM_BASE + 0x28,	\
		.io_type	= SERIAL_IO_MEM,	\
		.irq		= CDB4_COM_IRQ,		\
		.flags		= STD_COM_FLAGS		\
	}

#define RS_TABLE_SIZE  4

#elif defined(CONFIG_SED_SIOSIII)
#define BASE_BAUD (12500000 / 16)
#define STD_COM_FLAGS		ASYNC_BOOT_AUTOCONF
#define SERIAL_PORT_DFNS \
	{\
		.baud_base	= BASE_BAUD,		 \
		.iomem_base	= ((void *)0x80020000),	\
		.io_type	= SERIAL_IO_MEM,	\
		.irq		= 25,			\
		.flags		= STD_COM_FLAGS | SA_SHIRQ		\
	},\
	{\
		.baud_base      = BASE_BAUD,             \
		.iomem_base     = ((void *)0x80020008), \
		.io_type        = SERIAL_IO_MEM,        \
		.irq            = 25,                   \
		.flags          = STD_COM_FLAGS | SA_SHIRQ \
	}
#define RS_TABLE_SIZE	2
#undef CONFIG_SERIAL_MULTIPORT
#elif defined(CONFIG_SIGNAL_MCP751)

#define BASE_BAUD ( (1843200*3*4) / 16 )

#define CONFIG_SERIAL_SHARE_IRQ

#define MCP751_COM_BASE		((u8 *)0x44000000)
#define MCP751_COM_IRQ		91	/* external IRQ6 */
#define COM_IRQPRI	5		/* interrupt priority */
#define STD_COM_FLAGS		ASYNC_BOOT_AUTOCONF

#define SERIAL_PORT_DFNS				\
	{						\
		.baud_base	= BASE_BAUD,		\
		.iomem_base	= MCP751_COM_BASE + 0x0,	\
		.io_type	= SERIAL_IO_MEM,	\
		.irq		= MCP751_COM_IRQ,		\
		.flags		= STD_COM_FLAGS		\
	},						\
	{						\
		.baud_base	= BASE_BAUD,		\
		.iomem_base	= MCP751_COM_BASE + 0x8,	\
		.io_type	= SERIAL_IO_MEM,	\
		.irq		= MCP751_COM_IRQ,		\
		.flags		= STD_COM_FLAGS		\
	},						\

#define RS_TABLE_SIZE  2

#define CS3_BASE	   		 	(0x43000000)
#define EN_INT_UART_ON(line)    *((volatile unsigned short *)(CS3_BASE+0x18)) |= (0x4000 << line)
#define EN_INT_UART_OFF(line)   *((volatile unsigned short *)(CS3_BASE+0x18)) &= ~(0x4000 << line)
#define TEST_INT_UART(line)     (*((volatile unsigned short *)(CS3_BASE+0x18)) & (0x0010 << line))

#else
#error serial port not supported on this board
#endif

