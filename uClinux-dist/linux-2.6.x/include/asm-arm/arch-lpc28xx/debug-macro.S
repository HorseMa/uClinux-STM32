/* linux/include/asm-arm/arch-lpc22xx/debug-macro.S
 *
 * Debugging macro include header
 *
 *  Copyright (C) 1994-1999 Russell King (linux/include/asm-arm/arch-omap/debug-macro.S)
 *  Copyright (C) 2007 Siemens Building Technologies (mailto:philippe.goetz@siemens.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * NOTE: It is assumed that:
 * - the debugging operates on UART0
 * - the UART0 (as well as PINSEL) is configured.
*/

		/* UART base address */
		.macro	addruart,rx
		ldr	\rx, =0xe000c000
		.endm

		/* send char */
		.macro	senduart,rd,rx
		strb	\rd, [\rx]		// #UnTHR
		.endm

		/* wait for end of transmission */
		.macro	busyuart,rd,rx
1001:		ldrb	\rd, [\rx, #0x14]	// #UnLSR
		tst	\rd, #0x40		// #UnLSR_TEMT
		bne	1001b
		.endm

		/* wait for fifo space */
		/* quickest way here: test for >0x20 chars in fifo */
		.macro	waituart,rd,rx
1001:		ldrb	\rd, [\rx, #0x14]	// #UnLSR
		tst	\rd, #0x20		// #UnLSR_THRE
		beq	1001b
		.endm
