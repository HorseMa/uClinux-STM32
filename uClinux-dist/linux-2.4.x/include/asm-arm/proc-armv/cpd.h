/*
 * Filename:    include/asm-arm/proc-armv/cpd.h
 * Description: Caching Page Directory (CPD) includes for Fast Address 
 *		Space Switching (FASS) in ARM Linux.
 * Created: 20/06/2001
 * Changes: 19/02/2002 - Added helper macros.
 *          22/02/2002 - cpd_struct updated to include doubly linked list
 *          of cpd_structs allocated to the same domain.
 *          19/03/2002 - Start of modifications to support shared domains.
 *          26/04/2002 - Change per address-space domain disabling, via the
 *          DACR, to be lazy rather then diabling all domains on a context
 *          switch. Now all domains only diabled if a cache/tlb clean event
 *          occurred since the address-space last ran or if a domain was
 *          unallocated.
 *          28/04/2002 - Added macro's for clock domain recycling algorithm.
 *
 * Copyright:   (c) 2001, 2002 Adam Wiggins <awiggins@cse.unsw.edu.au>
 *		(c) 2004 National ICT Australia
 *
 ***************************************************************************
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of teh GNU General Public License version 2 as
 * published by the Free Software Foundation.                               
 */


#ifndef __ASM_PROC_CPD_H
#define __ASM_PROC_CPD_H
#ifdef CONFIG_ARM_FASS

#include <linux/sched.h>
//#include <asm/pgtable.h>

/******************
* Benchmark Flags *
******************/

#define CLOCK_RECYCLE 0 /* Round-robin domain recycling */
#define PID_ENABLE    1 /* PID relocation enabling/disabling */
#define LAZY_DISABLE  1 /* Lazy vs strict domain disabling via DACR */
#define ALWAYS_DIRTY  0 /* Always mark tlb/caches as incoherent */

/**********
* Defines *
**********/

//#define HAVE_ARCH_VM_SHARING_DATA

/* Base address for mmap() allocations */
#define CPD_UNMAPPED_BASE 0x80000000 /* Evilly hardcoded to 2GB */

/*********
* Macros *
*********/

/*
 * Set to enable FASS specific debugging options
 * yes there is probably a better
 * way to do this. 
 */
#if 1
#define fassert(expr) BUG_ON(!(expr))
#else
#define fassert(expr) 
#endif

/* Some handy macros */

#define MEGABYTE(n) ((n) << 20)
#define MASK_MEGABYTE(addr, n) ((addr) & ~(MEGABYTE(n) - 1))
#define CPD_P_TO_INDEX(cpd_p) (((unsigned long)(cpd_p) >> 2) &              \
                               ((1 << 12) - 1)) /* I know this is ugly */

#define STATS_P_TO_INDEX(cpd_stats_p) ((((unsigned long)(cpd_stats_p)) -     \
					((unsigned long)(cpd_stats))) /      \
				       sizeof(struct cpd_struct))

/* Is a domain/mm dirty with respect to the TLBs/Caches? */

#if ALWAYS_DIRTY
# define cpd_is_domain_coherent(domain)        0
# define cpd_is_domain_tlb_coherent(domain)    0
# define cpd_is_domain_cache_coherent(domain)  0
# define cpd_is_domain_dcache_coherent(domain) 0
# define cpd_is_domain_icache_coherent(domain) 0

# define cpd_is_mm_coherent(mm_p)        0
# define cpd_is_mm_tlb_coherent(mm_p)    0
# define cpd_is_mm_cache_coherent(mm_p)  0
# define cpd_is_mm_dcache_coherent(mm_p) 0
# define cpd_is_mm_icache_coherent(mm_p) 0

#else

# define cpd_is_domain_coherent(domain)                                   \
          (cpd_is_domain_tlb_coherent(domain) &&                          \
           cpd_is_domain_cache_coherent(domain))
#define cpd_is_domain_tlb_coherent(domain)                                \
          !domain_active(domain_tlb_coherence, (domain))
#define cpd_is_domain_cache_coherent(domain)                              \
          (cpd_is_domain_dcache_coherent(domain) &&                       \
           cpd_is_domain_icache_coherent(domain))
#define cpd_is_domain_dcache_coherent(domain)                             \
          !domain_active(domain_dcache_coherence, (domain))
#define cpd_is_domain_icache_coherent(domain)                             \
          !domain_active(domain_icache_coherence, (domain))

#define cpd_is_mm_coherent(mm_p)                                          \
          (cpd_is_mm_tlb_coherent(mm_p) && cpd_is_mm_cache_coherent(mm_p))
#define cpd_is_mm_tlb_coherent(mm_p)                                      \
          (!((mm_p)->context.dacr & domain_tlb_coherence &                \
             SYSTEM_DOMAINS_MASK))
#define cpd_is_mm_cache_coherent(mm_p)                                    \
          (cpd_is_mm_dcache_coherent((mm_p)) &&                           \
           cpd_is_mm_icache_coherent((mm_p)))
#define cpd_is_mm_dcache_coherent(mm_p)                                   \
          (!((mm_p)->context.dacr & domain_dcache_coherence &             \
             SYSTEM_DOMAINS_MASK))
#define cpd_is_mm_icache_coherent(mm_p)                                   \
          (!((mm_p)->context.dacr & domain_icache_coherence &             \
             SYSTEM_DOMAINS_MASK))

#endif

#if CLOCK_RECYCLE

#define cpd_domain_touched(domain)                                        \
          (domain_active(domain_touched, (domain)))
#define cpd_set_domain_touched(domain)                                    \
          (domain_touched |= domain_val((domain), DOMAIN_CLIENT))

#define cpd_clear_domain_touched(domain)                                  \
          (domain_touched &= ~(domain_val((domain), DOMAIN_MANAGER)))

#endif

/* Clean the TLBs/Caches */

#define cpd_tlb_clean() \
do {								\
/*printk("** cpd_tlb_clean() **\n");*/				\
	cpu_tlb_invalidate_all();				\
	set_dacr(current->thread.dacr); /* To keep track of dirty domains */\
	domain_clock++;                 /* as above */\
	domain_tlb_coherence = 0;\
} while (0)

#define cpd_cache_clean() \
do { \
	/*printk("** cpd_cache_clean() **\n");*/		\
	cpu_cache_clean_invalidate_all();			\
	set_dacr(current->thread.dacr); /* To keep track of dirty domains */ \
	domain_clock++;                 /* as above */\
	domain_dcache_coherence = 0;\
	domain_icache_coherence = 0;\
} while (0)

#define cpd_dcache_clean() \
do {								\
	cpu_dcache_clean_invalidate_all();			\
	set_dacr(current->thread.dacr); /* To keep track of dirty domains */\
	domain_clock++;                 /* as above */\
	domain_dcache_coherence = 0;\
} while(0)

/***********
* Typedefs *
***********/

struct domain_struct; /* Dummy prototype */

/* Region context */
struct region_struct {
	int mm_count;                   /* Number of address-spaces using region */
	struct domain_struct* domain_p; /* Domain this region is tagged with */
	struct region_struct* next;     /* Next region using this domain */
};

/* Domain context */
struct domain_struct {
	unsigned int number;            /* Domain number */
	unsigned int cpd_count;         /* CPD entries associated with this domain */
	struct cpd_struct* cpd_head;    /* List of CPD entries with this domain */
	struct region_struct* region_p; /* Region that owns this domain currently */
};

/* CPD information
 * Notes:
 *   - Number of collisions can be maintained in cpd_load.
 *   - Number of mm_struct's using this cpd entry, don't know how to calculate.
 */
struct cpd_struct {
	unsigned int collisions;     /* Number collisions on this entry */
	unsigned int mm_count;       /* Number of mm_struct's using this cpd entry */
	struct cpd_struct* cpd_next; /* Next CPD entry with the same domain */
	struct cpd_struct* cpd_prev; /* Previous CPD entry with the same domain */
};

/**********
* Externs *
**********/

extern struct domain_struct domains[DOMAIN_MAX];
extern struct cpd_struct cpd_stats[USER_PTRS_PER_PGD];

/*******************
* Inline Functions *
*******************/


static inline void
cpd_stats_add(struct domain_struct* domain_p, struct cpd_struct* cpd_stats_p)
{
	fassert(domain_p);
	fassert(cpd_stats_p);
	//printk("** cpd_stats_add: cpd_struct_p 0x%x **\n", (unsigned int)cpd_stats_p);
	//dump_cpd_stats(domain_p->number);
	fassert(cpd_stats_p != domain_p->cpd_head);

	cpd_stats_p->cpd_next = domain_p->cpd_head;
	cpd_stats_p->cpd_prev = NULL;

	if (domain_p->cpd_head) {
		domain_p->cpd_head->cpd_prev = cpd_stats_p;
	}
	domain_p->cpd_head = cpd_stats_p;

	//dump_cpd_stats(domain_p->number);

}

/* cpd_stats_del: Simply removes the cpd_stat from the domain_p's list */
static inline void
cpd_stats_del(int domain, struct cpd_struct* cpd_stats_p)
{
	fassert(cpd_stats_p);
	//printk("** cpd_stats_del: cpd_struct_p 0x%x prev 0x%x next 0x%x **\n",
	//	 (unsigned int)cpd_stats_p, (unsigned int)cpd_stats_p->cpd_prev,
	//	 (unsigned int)cpd_stats_p->cpd_next);

	if (domains[domain].cpd_head == cpd_stats_p) {
		domains[domain].cpd_head = cpd_stats_p->cpd_next;
	}

	/* De-link from list */
	if (cpd_stats_p->cpd_prev) {
		cpd_stats_p->cpd_prev->cpd_next = cpd_stats_p->cpd_next;
	}

	if (cpd_stats_p->cpd_next) {
		cpd_stats_p->cpd_next->cpd_prev = cpd_stats_p->cpd_prev;
	}

} /* cpd_stats_del() */

/***********************
* Function Prototypes *
***********************/

struct mm_struct;
void cpd_switch_mm(struct mm_struct* mm_p);

/* CPD Manipulation */
void cpd_load(pmd_t pmd, unsigned long mv_addr);

/* Region Manipulation */
struct region_struct* cpd_allocate_region(void);
void cpd_deallocate_region(struct region_struct* region_p);

/* Domain Manipulation */
int cpd_get_active_domain(struct mm_struct* mm_p, unsigned int v_addr);
struct domain_struct* cpd_get_domain(struct mm_struct* mm_p,
				     unsigned int v_addr);
struct domain_struct* cpd_allocate_domain(struct region_struct* region_p);
void cpd_deallocate_domain(struct domain_struct* domain_p);

/* mmap address allocation */

inline unsigned long cpd_next_area(unsigned long given_addr);
unsigned long cpd_get_shared_area(struct file *file_p, unsigned long addr,
				  unsigned long len, unsigned long pgoff,
				  unsigned long flags);
unsigned long cpd_get_unique_area(struct file *file_p, unsigned long addr,
				  unsigned long len, unsigned long pgoff,
				  unsigned long flags);
unsigned long cpd_get_private_area(struct file *file_p, unsigned long addr,
				   unsigned long len, unsigned long pgoff,
				   unsigned long flags);
unsigned long cpd_get_alloced_area(struct file *file_p, unsigned long addr,
				   unsigned long len, unsigned long pgoff,
				   unsigned long flags);

/* TLB/CPD/PageTable Coherence */
void cpd_set_pmd(pmd_t* pmd_p, pmd_t pmd);

void cpd_tlb_flush_all(void);
void cpd_tlb_flush_mm(struct mm_struct* mm_p);
void cpd_tlb_flush_range(struct mm_struct* mm_p,
                        unsigned long va_start, unsigned long va_end);
void cpd_tlb_flush_page(struct vm_area_struct* vma_p, unsigned long va_page);

/* Cache Coherence */
void cpd_cache_flush_all(void);
void cpd_cache_flush_mm(struct mm_struct* mm_p);
void cpd_cache_flush_range(struct mm_struct* mm_p,
                           unsigned long va_start, unsigned long va_end);
void cpd_cache_flush_page(struct vm_area_struct* vma_p, unsigned long va_page);


#endif /* CONFIG_ARM_FASS */
#endif /* !__ASM_PROC_CPD_H */
