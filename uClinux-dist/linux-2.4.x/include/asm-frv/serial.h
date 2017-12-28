/*
 * serial.h
 *
 * Copyright (C) 2003 Develer S.r.l. (http://www.develer.com/)
 * Author: Bernardo Innocenti <bernie@codewiz.org>
 *
 * Based on linux/include/asm-i386/serial.h
 */
#include <linux/config.h>
#include <asm/serial-regs.h>

/*
 * This assumes you have a 1.8432 MHz clock for your UART.
 *
 * It'd be nice if someone built a serial card with a 24.576 MHz
 * clock, since the 16550A is capable of handling a top speed of 1.5
 * megabits/second; but this requires the faster clock.
 */
#define BASE_BAUD 0

#define STD_COM_FLAGS		ASYNC_BOOT_AUTOCONF

#define RS_TABLE_SIZE		2
#define SERIAL_PORT_DFNS	{ line: 0 }, { line: 1 }
