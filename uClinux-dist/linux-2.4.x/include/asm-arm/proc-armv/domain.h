/*
 *  linux/include/asm-arm/proc-armv/domain.h
 *
 *  Copyright (C) 1999 Russell King.
 *
 *  20-Jun-2001        Modified by Adam Wiggins <awiggins@cse.unsw.edu.au>
 *                     for FASS.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ASM_PROC_DOMAIN_H
#define __ASM_PROC_DOMAIN_H

/*
 * Domain numbers
 *
 *  DOMAIN_IO     - domain 2 includes all IO only
 *  DOMAIN_KERNEL - domain 1 includes all kernel memory only
 *  DOMAIN_USER   - domain 0 includes all user memory only
 *
 * FASS Changes
 * 
 *  To support debugging the zero domain is unallocated and domains 3-15 are
 *  used for the USER while 1,2 are used by the KERNEL. Really only 0 should be
 *  used by the KERNEL and 1-15 by USER.
 */
#define DOMAIN_USER	0
#define DOMAIN_KERNEL	1
#define DOMAIN_TABLE	1
#define DOMAIN_IO	2

#ifdef CONFIG_ARM_FASS
#define DOMAIN_MAX      16
#define DOMAIN_END      (DOMAIN_MAX - 1)
#define DOMAIN_START    3

#define DOMAIN_USER_MASK (domain_val(DOMAIN_IO, DOMAIN_MANAGER) |              \
                          domain_val(DOMAIN_KERNEL, DOMAIN_MANAGER) |          \
                          domain_val(DOMAIN_USER, DOMAIN_NOACCESS))

/* [JNZ] The following is Adam's original definition:
#define SYSTEM_DOMAINS_MASK (~(domain_val(DOMAIN_USER, DOMAIN_MANAGER) &&      \
                               domain_val(DOMAIN_KERNEL, DOMAIN_MANAGER) &&    \
                               domain_val(DOMAIN_IO, DOMAIN_MANAGER)))
*/
#define SYSTEM_DOMAINS_MASK (~(domain_val(DOMAIN_USER, DOMAIN_MANAGER) |      \
                               domain_val(DOMAIN_KERNEL, DOMAIN_MANAGER) |    \
                               domain_val(DOMAIN_IO, DOMAIN_MANAGER)))

#endif /* CONFIG_ARM_FASS */


/*
 * Domain types
 */
#define DOMAIN_NOACCESS	0
#define DOMAIN_CLIENT	1
#define DOMAIN_MANAGER	3

#ifdef CONFIG_ARM_FASS
/*
 * Domain dirty indicators
 */
#define DIRTY 0xFFFFFFFF
#define CLEAN 0x00000000

extern unsigned long domain_tlb_coherence;
extern unsigned long domain_dcache_coherence;
extern unsigned long domain_icache_coherence;
extern unsigned long domain_touched; /* Used by domain recycling clock algo */

#define domain_val(dom,type)	((type) << 2*(dom))

/* Sets the CPU's Domain Access Control Register DACR */
static inline void
set_dacr(unsigned long dacr)
{
  domain_tlb_coherence |= dacr;
  domain_dcache_coherence |= dacr;
  domain_icache_coherence |= dacr;        

  //printk("** set_dacr: dacr 0x%x **\n", dacr);

  __asm__ __volatile__(
  "mcr	p15, 0, %0, c3, c0	@ set cpu dacr"
  : : "r" (dacr));

} /* set_dacr() */

/* Returns the state of the CPU's DACR */
static inline unsigned long
get_dacr(void)
{
  unsigned long dacr;

  __asm__ __volatile__(
  "mrc	p15, 0, %0, c3, c0	@ Get domain"
  : "=&r" (dacr) : );

  return dacr;

} /* get_dacr() */

/* This macro updates the threads DACR context from task_p's mm context */

#if CLOCK_RECYCLE

#define update_dacr(domain)                                                    \
do{                                                                            \
  unsigned long dacr;                                                           \
                                                                               \
  current->mm->context.dacr |= domain_val(domain, DOMAIN_CLIENT); /* Added */  \
  dacr = current->thread.dacr | current->mm->context.dacr;                     \
  domain_touched |= dacr;                                                      \
  set_dacr(dacr);                                                              \
} while(0)

#else

#define update_dacr(domain)                                                    \
do{                                                                            \
  unsigned long dacr;                                                           \
                                                                               \
  current->mm->context.dacr |= domain_val(domain, DOMAIN_CLIENT); /* Added */  \
  dacr = current->thread.dacr | current->mm->context.dacr;                     \
  set_dacr(dacr);                                                              \
} while(0)


#endif

/* Return the domain of page middle diretory (really page directory) entry */
#define pmd_domain(pmd) ((pmd_val(pmd) >> 5) & (DOMAIN_MAX - 1))

/* Tests if a domain is active in the given DACR */
#define domain_active(dacr, domain) (((dacr) >> (2*(domain))) & DOMAIN_MANAGER)

#define modify_domain(dom,type)				\
	do {						\
	unsigned long dacr = current->thread.dacr;	\
	dacr &= ~domain_val(dom, DOMAIN_MANAGER);	\
	dacr |= domain_val(dom, type);			\
	current->thread.dacr = dacr;			\
	set_dacr(dacr);          			\
	} while (0)

#else

#define domain_val(dom,type)	((type) << 2*(dom))

#define set_domain(x)					\
	do {						\
	__asm__ __volatile__(				\
	"mcr	p15, 0, %0, c3, c0	@ set domain"	\
	  : : "r" (x));					\
	} while (0)

#define modify_domain(dom,type)				\
	do {						\
	unsigned int domain = current->thread.domain;	\
	domain &= ~domain_val(dom, DOMAIN_MANAGER);	\
	domain |= domain_val(dom, type);		\
	current->thread.domain = domain;		\
	set_domain(current->thread.domain);		\
	} while (0)

#endif /* CONFIG_ARM_FASS */

#endif /* !__ASM_PROC_DOMAIN_H */
