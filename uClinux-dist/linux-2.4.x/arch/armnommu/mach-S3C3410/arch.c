/*
 * arch/arm/mach-S3C3410/arch.c
 *
 * Architecture specific fixups.  This is where any
 * parameters in the params struct are fixed up, or
 * any additional architecture specific information
 * is pulled from the params struct.
 *
 * Copyright (c) 2003 by Thomas Eschenbacher <thomas.eschenbacher@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/tty.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/init.h>

#include <asm/elf.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/io.h>

extern void genarch_init_irq(void);

MACHINE_START(S3C3410, "S3C3410")
       MAINTAINER("Thomas Eschenbacher")
       INITIRQ(genarch_init_irq)
MACHINE_END

void arch_hard_reset(void)
{
	/* configure P7.5 as output */
	outw(inw(S3C3410X_PCON7) | (1 << 5), S3C3410X_PCON7);

	/* set P7.5 to 0 */
	outb(inb(S3C3410X_PDAT7) & ~(1 << 5), S3C3410X_PDAT7);

	/*
	 * if we get here: the hardware reset failed.
	 * Now try to use the watchdog
	 */
	outw(0x00FF, S3C3410X_BTCON);
	while (1)
	{
		/* the rest will be done through the watchdog */
	}

}
