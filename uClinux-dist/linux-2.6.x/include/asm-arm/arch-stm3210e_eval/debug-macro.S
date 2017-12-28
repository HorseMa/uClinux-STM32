/* linux/include/asm-arm/arch-stm3210e_eval/debug-macros.S
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
*/

		.macro	addruart,rx
		mov	\rx,      #0x40000000
		orr	\rx, \rx, #0x00013800
		.endm

#include <driver/stm32/stm32f10x_usart.h>
#define	UART01x_DR	USART1_BASE+0x04
#define	UART01x_FR	USART1_BASE+0x00
#define	UART01x_FR_TXFF	0x0080

		.macro	senduart,rd,rx
		strb	\rd, [\rx, #UART01x_DR]
		.endm

		.macro	waituart,rd,rx
1001:		ldr	\rd, [\rx, #UART01x_FR]
		tst	\rd, #UART01x_FR_TXFF
		be	1001b
		.endm

		.macro	busyuart,rd,rx
1001:		ldr	\rd, [\rx, UART01x_FR]
		tst	\rd, UART01x_FR_TXFF
		bne	1001b
		.endm