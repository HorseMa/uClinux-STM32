/*
 *  linux/include/asm-arm/mmu_context.h
 *
 *  Copyright (C) 1996 Russell King.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Changelog:
 *   27-06-1996	RMK	Created
 *   20-06-2001 AGW     Defined (destroy/init_new)_context for FASS.
 */
#ifndef __ASM_ARM_MMU_CONTEXT_H
#define __ASM_ARM_MMU_CONTEXT_H

#include <asm/pgtable.h>
#include <asm/memory.h>
#include <asm/proc-fns.h>

#ifdef CONFIG_ARM_FASS
#include <asm/proc-armv/cpd.h>
#include <asm/proc-armv/pid.h>

static inline void
destroy_context(struct mm_struct* mm_p)
{
	//printk("** destroy_context: current 0x%x mm_p 0x%x **\n", current, mm_p);
  
	cpd_deallocate_region(mm_p->context.region_p);

	if (mm_p->context.pid_p) {
		arm_pid_unallocate(mm_p->context.pid_p);
	}

}

/* Do these values need to be zeroed? It seemed not to work if they weren't */
static inline int
init_new_context(struct task_struct* task_p, struct mm_struct* mm_p)
{
	//printk("** init_new_context: task_p 0x%x  mm_p 0x%x **\n", task_p, mm_p);

	if ((mm_p->context.region_p = cpd_allocate_region()) == NULL) {
		return -1; /* Is this right? */
	}

	/* No domain or ARM PID yet allocated */
	mm_p->context.pid = 0;
	mm_p->context.pid_p = NULL;
	mm_p->context.dacr = 0;
  
	return 0;

}

#else

#define destroy_context(mm)		do { } while(0)
#define init_new_context(tsk,mm)	0

#endif /* CONFIG_ARM_FASS */


/*
 * This is called when "tsk" is about to enter lazy TLB mode.
 *
 * mm:  describes the currently active mm context
 * tsk: task which is entering lazy tlb
 * cpu: cpu number which is entering lazy tlb
 *
 * tsk->mm will be NULL
 */
static inline void
enter_lazy_tlb(struct mm_struct *mm, struct task_struct *tsk, unsigned cpu)
{
}

/*
 * This is the actual mm switch as far as the scheduler
 * is concerned.  No registers are touched.
 */
static inline void
switch_mm(struct mm_struct *prev, struct mm_struct *next,
	  struct task_struct *tsk, unsigned int cpu)
{
	if (prev != next)
#ifdef CONFIG_ARM_FASS
		cpd_switch_mm(next);
#else
		cpu_switch_mm(next->pgd, tsk);
#endif
}

#define activate_mm(prev, next) \
	switch_mm((prev),(next),NULL,smp_processor_id())

#endif
