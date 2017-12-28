/*
 * linux/include/asm/arch-S3C3410/serial.h
 *
 * Copyright (c) 2003 by Thomas Eschenbacher <thomas.eschenbacher@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#ifndef __ASM_ARCH_SERIAL_H
#define __ASM_ARCH_SERIAL_H

/*
 * This assumes you have a 1.8432 MHz clock for your UART.
 *
 * It'd be nice if someone built a serial card with a 24.576 MHz
 * clock, since the 16550A is capable of handling a top speed of 1.5
 * megabits/second; but this requires the faster clock.
 */
#define BASE_BAUD ( 3686400 / 16 )

/** 
 * serial port on the debug card
 */
#define EXTRA_SERIAL_PORT_DEFNS                \
    { baud_base: BASE_BAUD,                    \
      irq: S3C3410X_INTERRUPT_EINT8,           \
      flags: STD_COM_FLAGS,                    \
      iomem_base: (S3C3410X_EXTDAT1 + 0x0A00), \
      iomem_reg_shift: 6,                      \
      io_type: SERIAL_IO_MEM },

#endif /* __ASM_ARCH_SERIAL_H */
