/* system.h: FR-V CPU control definitions
 *
 * Copyright (C) 2003 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#ifndef _ASM_SYSTEM_H
#define _ASM_SYSTEM_H

#include <linux/config.h> /* get configuration macros */
#include <linux/linkage.h>
#include <asm/atomic.h>

struct thread_struct;

#define prepare_to_switch()    do { } while(0)

/*
 * switch_to(prev, next) should switch from task `prev' to `next'
 * `prev' will never be the same as `next'.
 * The `mb' is to tell GCC not to cache `current' across this call.
 */
extern asmlinkage
struct task_struct *__switch_to(struct thread_struct *prev_thread,
				struct thread_struct *next_thread,
				struct task_struct *prev);

#define switch_to(prev, next, last)					\
do {									\
	(prev)->thread.sched_lr =					\
		(unsigned long) __builtin_return_address(0);		\
	(last) = __switch_to(&(prev)->thread, &(next)->thread, (prev));	\
	mb();								\
} while(0)

#define __cli()						\
do {							\
	unsigned long psr;				\
	asm volatile("	movsg	psr,%0		\n"	\
		     "	ori	%0,%1,%0	\n"	\
		     "	movgs	%0,psr		\n"	\
		     : "=r"(psr)			\
		     : "i" (PSR_PIL)			\
		     : "memory");			\
} while(0)

#define __sti()						\
do {							\
	unsigned long psr;				\
	asm volatile("	movsg	psr,%0		\n"	\
		     "	andi	%0,%1,%0	\n"	\
		     "	movgs	%0,psr		\n"	\
		     : "=r"(psr)			\
		     : "i" (~PSR_PIL)			\
		     : "memory");			\
} while(0)

#define __save_flags(x)		asm("movsg psr,%0" : "=r"(x) : : "memory")
#define __restore_flags(x)	asm volatile("movgs %0,psr" : : "r"(x) : "memory")

#define	__save_and_cli(flags)				\
do {							\
	unsigned long npsr;				\
	asm volatile("	movsg	psr,%0		\n"	\
		     "	ori	%0,%2,%1	\n"	\
		     "	movgs	%1,psr		\n"	\
		     : "=r"(flags), "=r"(npsr)		\
		     : "i" (PSR_PIL)			\
		     : "memory");			\
} while(0)

#define	__save_and_sti(flags)				\
do {							\
	unsigned long npsr;				\
	asm volatile("	movsg	psr,%0		\n"	\
		     "	andi	%0,%2,%1	\n"	\
		     "	movgs	%1,psr		\n"	\
		     : "=r"(flags), "=r"(npsr)		\
		     : "i" (~PSR_PIL)			\
		     : "memory");			\
} while(0)

#define irqs_disabled()					\
({							\
       unsigned long flags;				\
       __save_flags(flags);				\
       (flags & PSR_PIL);				\
})


/* For spinlocks etc */
#define local_irq_save(x)	__save_and_cli(x)
#define local_irq_set(x)	__save_and_sti(x)
#define local_irq_restore(x)	__restore_flags(x)
#define local_irq_disable()	__cli()
#define local_irq_enable()	__sti()

#define cli()			__cli()
#define sti()			__sti()
#define save_flags(x)		__save_flags(x)
#define restore_flags(x)	__restore_flags(x)
#define save_and_cli(x)		__save_and_cli(x)
#define save_and_set(x)		__save_and_sti(x)

/*
 * Force strict CPU ordering.
 * Not really required on m68k...
 */
#define nop()			asm volatile ("nop"::)
#define mb()			asm volatile ("membar" : : :"memory")
#define rmb()			asm volatile ("membar" : : :"memory")
#define wmb()			asm volatile ("membar" : : :"memory")
#define set_mb(var, value)	do { var = value; mb(); } while (0)
#define set_wmb(var, value)	do { var = value; wmb(); } while (0)

#define smp_mb()		barrier()
#define smp_rmb()		barrier()
#define smp_wmb()		barrier()

#define HARD_RESET_NOW()			\
do {						\
	cli();					\
} while(1)

extern void die_if_kernel(const char *, ...) __attribute__((format(printf, 1, 2)));
extern void free_initmem(void);

#endif /* _ASM_SYSTEM_H */
