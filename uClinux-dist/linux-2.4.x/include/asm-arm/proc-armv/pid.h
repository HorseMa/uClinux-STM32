/*
* Filename:    include/asm-arm/proc-armv/pid.h                                 
* Description: ARM Porcess ID (PID) includes for Fast Address Space Switching
*              (FASS) in ARM Linux.
* Created:     14/10/2001
* Changes:     19/02/2002 - Macros added.
* Copyright:   (C) 2001, 2002 Adam Wiggins <awiggins@cse.unsw.edu.au>
********************************************************************************
* This program is free software; you can redistribute it and/or modify
* it under the terms of teh GNU General Public License version 2 as
* published by the Free Software Foundation.
*******************************************************************************/

#ifndef __ASM_PROC_PID_H
#define __ASM_PROC_PID_H
#ifdef CONFIG_ARM_FASS

#include <asm/proc/cpd.h>

/**********
* Defines *
**********/

#define ARMPID_SHIFT 25

/* PID to be allocated first */
#define ARMPID_START 1

/* Only 64 relocation spots on the StrongARM, see notes in arch/arm/mm/pid.c */
/* (Have more on xscale, need to parameterise this) */
#define ARMPID_MAX (1U << 6)

/* Size of PID relocation area */
#define ARMPID_TASK_SIZE (1UL << ARMPID_SHIFT)

/* Start of dummy vm_area to seperate bss and stack */
#define ARMPID_BRK_LIMIT (ARMPID_TASK_SIZE - MEGABYTE(1)) /* 31MB */

/* Number of CPD entries in the ARM PID relocation area */
#define ARMPID_PTRS_PER_PGD (ARMPID_TASK_SIZE / PGDIR_SIZE)

/* Mask to get rid of PID from relocated address */
#define ARMPID_MASK (ARMPID_TASK_SIZE - 1)


/* Convert PID number ot address space location */
#define PIDNUM_TO_PID(pid_num) ((pid_num) << ARMPID_SHIFT)

/* And back again */
#define PID_TO_PIDNUM(pid) ((pid) >> ARMPID_SHIFT)

/* Gets the ARM PID register value from a Modified Virtual Address (MVA) */
#define MVA_TO_PID(mva) ((mva) & ~ARMPID_MASK)
#define MVA_TO_PIDNUM(mva) (PID_TO_PIDNUM(MVA_TO_PID(mva)))
#define MVA_TO_VA(mva) (mva & ARMPID_MASK)
#define VA_TO_MVA(va) (va | get_armpid())

/* Find out the CPD index offset due to ARM PID relocation */
#define ARMPID_CPD_OFFSET(pid) ((pid) >> PGDIR_SHIFT)


/* PID context */
struct pid_struct {
	unsigned int number;   /* ARM PID number */
	unsigned int mm_count; /* Number of address-spaces using this ARM PID */
};

/**********
* Externs *
**********/

extern struct pid_struct pids[ARMPID_MAX];

/*******************
* Inline Functions *
*******************/

/* Sets the CPU's PID Register */
static inline void
set_armpid(unsigned int pid)
{
	__asm__ __volatile__(
		"mcr	p15, 0, %0, c13, c0, 0	@ set cpu dacr"
		: : "r" (pid));

} /* set_armpid() */

/* Returns the state of the CPU's PID Register */
static inline int
get_armpid(void)
{
	int pid;

	__asm__ __volatile__(
		"mrc	p15, 0, %0, c13, c0, 0	@ Get ARM PID"
		: "=&r" (pid) : );

	return (pid & (~ARMPID_MASK));

} /* get_armpid() */

static inline unsigned long
mva_to_va(unsigned long mva, struct mm_struct* mm_p)
{
	//printk("** mva_to_va: mm_p 0x%x mm_p->ctx.pid 0x%x CPU PID 0x%x **\n",
	//	 mm_p, mm_p->context.pid, get_armpid());
	//printk("** mva_to_va: pid_num %d mva 0x%x **\n",
	//	 PID_TO_PIDNUM(mm_p->context.pid), mva);

	if (get_armpid() && (get_armpid() == MVA_TO_PID(mva))) {
		//printk("** mva_to_va: represents va 0x%x **\n", MVA_TO_VA(mva));

		return MVA_TO_VA(mva);
	}

	return mva;

} /* mva_to_va() */

static inline unsigned long
va_to_mva(unsigned long va, struct mm_struct* mm_p)
{
	//printk("** va_to_mva: mm_p 0x%x mm_p->ctx.pid 0x%x CPU PID 0x%x **\n",
	//	 mm_p, mm_p->context.pid, get_armpid());
	//printk("** va_to_mva: pid_num %d va 0x%x **\n",
	//	 PID_TO_PIDNUM(mm_p->context.pid), va);

	if (va < ARMPID_TASK_SIZE) {
		//printk("** va_to_mva: relocated to mva 0x%x **\n", VA_TO_MVA(va));

		return VA_TO_MVA(va);
	}

	return va;

} /* va_to_mva() */


/* PID Manipulation */
void arm_pid_allocate(struct task_struct* task_p, struct mm_struct* mm_p);
void arm_pid_unallocate(struct pid_struct* pid_p);


#endif /* CONFIG_ARM_FASS */
#endif /* !__ASM_PROC_PID_H */
