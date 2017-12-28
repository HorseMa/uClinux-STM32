/* linux/include/asm-arm/arch-realview/debug-macro.S
 *
 * Debugging macro include header
 *
 *  Copyright (C) 1994-1999 Russell King
 *  Moved from linux/arch/arm/kernel/debug.S by Ben Dooks
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
*/

		.macro	addruart,rx
#ifdef CONFIG_CPU_V7M
		mov	\rx,      #0x40000000
#else
		mrc	p15, 0, \rx, c1, c0
		tst	\rx, #1			@ MMU enabled?
		ite	eq
		moveq	\rx,      #0x10000000
		movne	\rx,      #0xf0000000	@ virtual base
#endif
		orr	\rx, \rx, #0x00009000
		.endm

#include <asm/hardware/debug-pl01x.S>
