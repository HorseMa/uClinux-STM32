/*
 *  arch/armnommu/mach-S3C44B0X/arch.c
 *
 *  Architecture specific fixups.  This is where any
 *  parameters in the params struct are fixed up, or
 *  any additional architecture specific information
 *  is pulled from the params struct.
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
#include <linux/module.h>

#include <asm/io.h>
#include <asm/elf.h>
#include <asm/setup.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>

extern void genarch_init_irq(void);

/* forward declaration, needed for exporting */
unsigned int arch_cpu_clock(void);

/**
 * clock frequency of the CPU in [Hz],
 * might be overwritten by a kernel parameter
 * passed by the bootloader
 */
static unsigned int clock_frequency = 60000001;

MACHINE_START(S3C44B0, "S3C44B0X")
       MAINTAINER("Thomas Eschenbacher")
       INITIRQ(genarch_init_irq)
MACHINE_END

/**
 * Hardware reset.
 */
void arch_hard_reset(void)
{
	/* disable interrupts */
	cli();
	
	/* force startup with cache disabled */
	outl(0x00000000, S3C44B0X_SYSCFG);
        
	/* the rest will be done by the watchdog timer... */
	outw(0x0121, S3C44B0X_WTCON);
                
	while (1);
}

/**
 * Returns the CPU clock in Hz
 */
unsigned int arch_cpu_clock(void)
{
	return clock_frequency;
}

/** copy of atoi, which is not available in the kernel */
static int __init my_atoi(const char *name)
{
    int val = 0;

    for (;; name++) {
        switch (*name) {
            case '0'...'9':
                val = 10*val+(*name-'0');
                break;
            default:
                return val;
        }
    }
}

/**
 * Parses the CPU clock frequency parameter from the kernel command line.
 * Currently only does atoi without any range checking!
 */
static int __init _clock_setup(char *str)
{
	if (str == NULL || *str == '\0')
		return 0;

	clock_frequency = my_atoi(str);

	return 0;
}

__setup("clk=", _clock_setup);

EXPORT_SYMBOL(arch_cpu_clock);

/* end of file */
