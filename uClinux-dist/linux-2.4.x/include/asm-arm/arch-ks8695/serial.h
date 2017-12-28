/*
 * linux/include/asm-arm/arch-ks8695/serial.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ASM_ARCH_SERIAL_H
#define __ASM_ARCH_SERIAL_H

/*
 *	This setup is used on the OpenGear CM4xxx hardware.
 */
#define BASE_BAUD	(1843200 / 16)
#define RS_TABLE_SIZE	48

#define STD_SERIAL_PORT_DEFNS
#define EXTRA_SERIAL_PORT_DEFNS

#endif /* __ASM_ARCH_SERIAL_H */
