/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2008, Greg Ungerer <gerg@snapgear.com>
 */
#ifndef __ASM_MIPS_MACH_CAVIUM_OCTEON_GPIO_H
#define __ASM_MIPS_MACH_CAVIUM_OCTEON_GPIO_H

#define	OCTEON_GPIO_INPUT	0x00
#define	OCTEON_GPIO_OUTPUT	0x01
#define	OCTEON_GPIO_INPUT_XOR	0x02
#define	OCTEON_GPIO_INTERRUPT	0x04
#define	OCTEON_GPIO_INT_LEVEL	0x00
#define	OCTEON_GPIO_INT_EDGE	0x08

void octeon_gpio_config(int line, int type);
unsigned long octeon_gpio_read(void);
void octeon_gpio_set(unsigned long bits);
void octeon_gpio_clear(unsigned long bits);
void octeon_gpio_interrupt_ack(int line);

#endif /* __ASM_MIPS_MACH_CAVIUM_OCTEON_GPIO_H */
