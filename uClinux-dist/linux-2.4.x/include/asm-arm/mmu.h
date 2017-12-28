/* include/asm-arm/mmu.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  20-Jun-2001         Modified by Adam Wiggins <awiggins@cse.unsw.edu.au>
 *                      for FASS.
 *  11-Oct-2001         Added PID field for PID register relocation support.
 *  26-Apr-2002         Added domain_timestamp field allow lazy clearing of the
 *                      DACR. This is required to keep track of clean domains
 *                      and to disable accessed to unallocated domains.
 *  15-Sep-2003         Modified by John Zaitseff <J.Zaitseff@unsw.edu.au>
 *                      for Linux kernel 2.4.21-rmk1
 */

#ifndef __ARM_MMU_H
#define __ARM_MMU_H

#ifdef CONFIG_ARM_FASS

/***********************
* Forward declerations *
***********************/

/* include/asm-arm/proc-armv/cpd.h */
struct domain_struct;

/* include/asm-arm/proc-armv/pid.h */
struct pid_struct;

/* Address-space context */
struct fass_struct{
  unsigned int pid;                    /* Process ID register */
  unsigned int dacr;                   /* Domain Access Control Register */
  unsigned long long domain_timestamp; /* Last DACR clearing to check domains */

  struct pid_struct* pid_p;            /* PID associated with address-space */
  struct region_struct* region_p;      /* Address-space's private region */
};

typedef struct fass_struct mm_context_t;

struct task_struct;
struct mm_struct;

extern void arm_pid_allocate(struct task_struct *, struct mm_struct *);
static inline void
arch_new_mm(struct task_struct *tp, struct mm_struct *mm)
{
	arm_pid_allocate(tp, mm);
}

#else

/*
 * The ARM doesn't have a mmu context
 */
typedef struct { } mm_context_t;

#endif /* CONFIG_ARM_FASS */

#endif /* !__ARM_MMU_H */
