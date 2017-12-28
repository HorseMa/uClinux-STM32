/* nmi.c: FRV NMI handler (level 15 external interrupt)
 *
 * Copyright (C) 2003 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/config.h>
#include <linux/ptrace.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/timex.h>
#include <linux/smp_lock.h>
#include <linux/init.h>
#include <linux/kernel_stat.h>
#include <linux/irq.h>
#include <linux/proc_fs.h>

#include <asm/atomic.h>
#include <asm/io.h>
#include <asm/smp.h>
#include <asm/system.h>
#include <asm/bitops.h>
#include <asm/pgalloc.h>
#include <asm/delay.h>
#include <asm/irq.h>
#include <asm/gdb-stub.h>

/*****************************************************************************/
/*
 * deal with the NMI
 */
asmlinkage void do_NMI(void)
{
} /* end do_IRQ() */
