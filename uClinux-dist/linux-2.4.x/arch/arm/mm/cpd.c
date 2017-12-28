/*
* Filename:    arch/arm/mm/cpd.c
* Description: Caching Page Directory (CPD) functions for Fast Address Space
*              Switching (FASS) in ARM Linux.
*
* Created:     20/06/2001
* Changes:     14/10/2001 - Added code to deal with ARM PID reloation in
*              cpd_load() and the cpd_*_flush_(page/range)() code.
*              19/02/2002 - Modified arch_get_unmapped_area() to be more
*              intelligent and added support functions to bring this about.
*              22/02/2002 - Modifed domain manipulation to add cpd_stats house
*              keeping information. cpd_deallocate_domain() moved to list
*              traversal of CPD.
*              19/03/2002 - Start of modifications to support shared domains.
*              25/04/2002 - Changed cpd_allocate_domain() to simply use a
*              round-robin algorithm on the third pass, ie no clean domains.
*              This improved performance during domain thrashing to better then*
*              normal linux.
*              26/04/2002 - Change per address-space domain disabling, via the
*              DACR, to be lazy rather then diabling all domains on a context
*              switch. Now all domains only diabled if a cache/tlb clean event
*              occured since the address-space last ran or if a domain was
*              deallocated.
*              28/04/2002 - Replaced round-robin domain recycling algorithm
*              with a clock algorithm.
* Copyright:   (C) 2001, 2002 Adam Wiggins <awiggins@cse.unsw.edu.au>
*		(C) 2004 National ICT Australia and UNSW
*******************************************************************************
*
* Notes:
*   - Cache/TLB flushes don't do anything if no domain is allocated since no
*     user memory can be touched without a domain, similarly if the allocated
*     domain is marked as clean.
*
*******************************************************************************
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public Licence version 2 as
* published by the Free Software Foundation.
*
*/

#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <asm/proc/cpd.h>
#include <asm/proc/pid.h>
#include <asm/proc/cache.h>
#include <asm/proc/pgtable.h>


/* Used to lazily update address-space DACRs */
static unsigned long long domain_clock;

/* The next domain to be allocated in when domain thrashing */
static int next_domain = DOMAIN_START;

/* The next 1MB region for mmap()s. Hack for now */
static int next_unmapped = PAGE_ALIGN(CPD_UNMAPPED_BASE);

/* Domain TLB/Cache dirty bits, Cleared when coherent */
unsigned long domain_tlb_coherence = DIRTY;
unsigned long domain_dcache_coherence = DIRTY;
unsigned long domain_icache_coherence = DIRTY;

EXPORT_SYMBOL(domain_tlb_coherence);
EXPORT_SYMBOL(domain_dcache_coherence);
EXPORT_SYMBOL(domain_icache_coherence);

unsigned long domain_touched = DIRTY; /* Used by domain recycling clock algo */

/* Array of domain_structs for ARM's 16 domains */
static struct domain_struct domains[DOMAIN_MAX] = {
	{.number = 0},
	{.number = 1},
	{.number = 2},
	{.number = 3},
	{.number = 4},
	{.number = 5},
	{.number = 6},
	{.number = 7},
	{.number = 8},
	{.number = 9},
	{.number = 10},
	{.number = 11},
	{.number = 12},
	{.number = 13},
	{.number = 14},
	{.number = 15}
};

/* Statistics on CPD entries */
struct cpd_struct cpd_stats[USER_PTRS_PER_PGD];


/* cpd_switch_mm: This function switches address-space contexts.
 *
 * Notes:
 *   - At the moment all domains are disabled on a context switch and reenabled
 *     via the domain faults. This might need to be changed as the cost is
 *     noticable, if small. The problem with simply setting it to enable only
 *     dirty domains means we don't test if a domain has been deallocated,
 *     reallocated and touched. It either needs to be tested here or revoked
 *     from all address spaces at deallocation time. Need to think about it a
 *     little more.
 */
void
cpd_switch_mm(struct mm_struct* mm_p)
{

#if LAZY_DISABLE

	if (mm_p->context.domain_timestamp != domain_clock) {
		//printk("** cpd_switch: tlb 0x%x icache 0x%x dcache 0x%x dacr 0x%x**\n",
		//	   domain_tlb_coherence, domain_icache_coherence, domain_dcache_coherence,
		//	   get_dacr());
      

		/* Disable 'mm_p's domains that are clean or recycled */
		mm_p->context.dacr = 0; /* disables all domains */
		mm_p->context.domain_timestamp = domain_clock;
	}

#else

	/* Disable 'mm_p's domains that are clean or recycled */
	mm_p->context.dacr = 0; /* disables all domains */

#endif

#if CLOCK_RECYCLE
	domain_touched |= mm_p->context.dacr;
#endif

}

/* cpd_load: This function loads a Page Directory Entry 'pmd' from the 'current'
 *   process into the CPD. It simply takes 'pmd' and tags it with the domain
 *   allocated to the region it is associated with. The result is then loaded
 *   into the required CPD entry. The function updates the cpd_count of the
 *   domain and does a coherence check if the CPD entry is being replaced, 
 *   i.e., it was previously allocated to another region.
 *
 * Notes:
 *   - mv_addr is the ARM PID relocated Modifed Virtual Address (MVA) seen by
 *     the FASS part of the kernel. Since the CPD is the only pg_dir walked by
 *     the hardware it is the only pg_dir that needs to do ARM PID relocation.
 */
void
cpd_load(pmd_t pmd, unsigned long mv_addr)
{
	struct domain_struct* domain_p =
		cpd_get_domain(current->mm, mva_to_va(mv_addr, current->mm));
	pmd_t* cpd_p = pmd_offset(pgd_offset_k(mv_addr), mv_addr);
	unsigned int index = CPD_P_TO_INDEX(cpd_p);
	int old_domain = pmd_domain(*cpd_p);

#if 0
	printk("** cpd_load: i %d old %d new %d , v_addr %lx, mv_addr %lx**\n", index, old_domain,
	       domain_p->number, mva_to_va(mv_addr, current->mm), mv_addr);
#endif
	if (old_domain) { /* CPD entry being replaced, TLB/Cache coherence needed */
		if (old_domain == domain_p->number)
		{
			printk("Arrgh.  cpd_load called from %p\b", __builtin_return_address(0));
			printk("        pmd_count = %d, pmd_val(pmd)= 0x%lx\n",
			       domain_p->cpd_count,
			       pmd_val(pmd));
		}
		fassert(old_domain != domain_p->number);
    
		/* update cpd_stats[] */
		cpd_stats[index].collisions++;
		cpd_stats_del(old_domain, cpd_stats + index);
    
		/* update domains[] */
		domains[old_domain].cpd_count--;    

		if (!cpd_is_domain_cache_coherent(old_domain)) {
      			cpd_cache_clean();
		} /* The cache clean is not required for ARM9, see cpd.h Notes */
    
		//if (!cpd_is_domain_tlb_coherent(old_domain)) {
		// This should be made into a targeted TLB flush.
		cpd_tlb_clean();
		//}
	}

	/* Update domains[] */
  
	domain_p->cpd_count++;
	cpd_stats_add(domain_p, cpd_stats + index);

	/* Update CPD */
	pmd_val(pmd) |= PMD_DOMAIN(domain_p->number); /* Tag CPD entry */
	cpu_set_pmd(cpd_p, pmd);
}

/*
 * cpd_allocate_region: ?
 */
struct region_struct*
cpd_allocate_region(void)
{
	struct region_struct* region_p = kmalloc(sizeof(struct region_struct),
						 GFP_KERNEL); /* Is this correct? */
	if (region_p) {
		region_p->mm_count = 1;
		region_p->domain_p = NULL;
		region_p->next = NULL;
	}

	return region_p;

}

/* cpd_deallocate_region: Frees a region.
 *
 * Notes:
 *   - Using the non-lazy cache/tlb flushing because the lazy versions crash
 *     the kernel, have to come back and have a look at this later.
 */
void
cpd_deallocate_region(struct region_struct* region_p)
{
	struct domain_struct* domain_p = region_p->domain_p;

	/* Free domain if we have one */
	if (domain_p) {
 
		/* Make domain coherent */
		if (!cpd_is_domain_cache_coherent(domain_p->number)) {
			cpu_cache_clean_invalidate_all(); /* avoiding lazy flush, see below */
		}
    
		if (!cpd_is_domain_tlb_coherent(domain_p->number)) {
			cpu_tlb_invalidate_all(); /* avoiding lazy flush because it crashes */
		}

		/* Deallocate domain */
		cpd_deallocate_domain(domain_p);
	}

	kfree(region_p);
}

/*
 * cpd_get_region: This function retrieves the region allocated to the
 *   address-space at v_addr.
 */
struct region_struct*
cpd_get_region(struct mm_struct* mm_p, unsigned int v_addr)
{
	struct vm_area_struct* vma_p = find_vma(mm_p, v_addr);

	if (vma_p && (v_addr >= vma_p->vm_start) && vma_p->vm_sharing_data)
		return vma_p->vm_sharing_data;
		
	return mm_p->context.region_p;
}

/*
 * cpd_get_active_domain: This function simply returns the domain number
 *   allocated to 'mm_p' in the region covered by 'v_addr'. If no domain is
 *   allocated 0 is returned. 'v_addr' == NULL requests the private domain of
 *   'mm_p'.
 *
 * Notes: 
 *   * Currently just returns the private domain, needs fixing.
 */
int
cpd_get_active_domain(struct mm_struct* mm_p, unsigned int v_addr)
{
	struct region_struct* region_p;

	if (mm_p) {
		region_p = cpd_get_region(mm_p, v_addr);

		return region_p->domain_p ? region_p->domain_p->number : 0;
	}
		
	return 0;
}

/*
 * cpd_get_domain: This function retrieves the domain currently
 * allocated to the given region. If there is none, then one is
 * allocated, possibly preempted from another region.
 *
 * Notes: 
 *   * Currently just returns the private domain, needs fixing.
 */
struct domain_struct*
cpd_get_domain(struct mm_struct* mm_p, unsigned int v_addr)
{
	struct region_struct* region_p = cpd_get_region(mm_p, v_addr);

	return region_p->domain_p ? region_p->domain_p :
		cpd_allocate_domain(region_p);

}

/*
 * cpd_allocate_domain: This function finds a free domain or preempts a used
 *   one. A victim domain is selected with the following criteria:
 *   - The first deallocated domain is used. OR
 *   - The domain with the least CPD entries, which doesn't require a cache/TLB
 *     flush. OR
 *   - The domain with the least CPD entries.
 * Notes:
 *   - A policy of replacing the domain with the least CPD entries might not be
 *     the best. A round robbin or other policy may be more appropriate. This
 *     needs further thought and investigation.
 */
struct domain_struct*
cpd_allocate_domain(struct region_struct* region_p)
{
	int i;
	int new_domain;
	struct domain_struct* domain_p;

	/* First pass to find an deallocated domain */
	for (i = DOMAIN_START; i <= DOMAIN_END; i++) {
		if (!domains[i].region_p) {
			domain_p = domains + i;
			goto allocate;
		}
	}

#if CLOCK_RECYCLE
	/* First run of the clock domain_usage might not equal zero */
clock:
	/* Second pass to find a clean domain with the least entries in the CPD */
	for (i = DOMAIN_START; i <= DOMAIN_END; i++) {
		new_domain = next_domain;
		(next_domain == DOMAIN_END) ? (next_domain = DOMAIN_START) : next_domain++;

		if (cpd_is_domain_coherent(next_domain) &&
		    !cpd_domain_touched(next_domain)) {
			domain_p = domains + next_domain;

			goto deallocate;
		}
	}

	/* Third pass to find a domain using clock scheme */
	for (i = DOMAIN_START; i <= DOMAIN_END; i++) {
		new_domain = next_domain;
		(next_domain == DOMAIN_END) ? (next_domain = DOMAIN_START) : next_domain++;

		if (!cpd_domain_touched(next_domain)) {
			domain_p = domains + next_domain;

			goto flush;
		}

		cpd_clear_domain_touched(next_domain);
	}

	/* Now, domain_usage == 0 */
	goto clock;

	/* Deallocated domain needs to be made coherent */
flush:
 
	/* 
	 *The old third pass leads to very poor behaviour in the case
	 * that domains are thrashing, ie about 3-4 times the number
	 * of domains in processes are heavily thrashing.
	 */
	/* Third pass to find a domain with the fewest entries in the CPD */
	//for(i = new_domain = DOMAIN_START; i <= DOMAIN_END; i++) {
	//  if(domains[i].cpd_count < domains[new_domain].cpd_count) {
	//    new_domain = i;
	//  }
	//}

#else /* Round-robin recycling algorithm */

	/* Second pass to find a clean domain with the least entries in the CPD */
	for (i = new_domain = DOMAIN_START; i <= DOMAIN_END; i++) {
		if (cpd_is_domain_coherent(i) &&
		    ((domains[i].cpd_count * domains[i].region_p->mm_count) <
		     (domains[new_domain].cpd_count * domains[i].region_p->mm_count))) {

			domain_p = domains + i;

			goto deallocate;
		}
	}

	/* Third pass to find a domain in round-robin scheme when thrashing. */ 
  
	new_domain = next_domain;
	(next_domain == DOMAIN_END) ? next_domain = DOMAIN_START : next_domain++;

#endif
	domain_p = domains + new_domain;
	/* Domain has entries in CPD, clean caches and TLB */
	if (!cpd_is_domain_cache_coherent(new_domain)) {
		cpd_cache_clean();
	}

	if (!cpd_is_domain_tlb_coherent(new_domain)) {
		cpd_tlb_clean();
	}

deallocate:
	cpd_deallocate_domain(domain_p);
  
allocate:

	domain_p->region_p = region_p;
	region_p->domain_p = domain_p;

	//printk("** cpd_allocate_domain: %d **\n", domain_p->number);

	return domain_p;
}

/* cpd_deallocate_domain: Insures all CPD entries owned by 'domain_p' are
 *   removed from CPD and the domain is deallocated.
 * Assumptions:
 *   - domain_p != NULL.
 *   - domain_p->region_p != NULL;
 *   - TLBs coherent, ie no TLB entries tagged with this domain.
 * Notes:
 *   - Should domain and region pointer updates be atomic?
 */
void
cpd_deallocate_domain(struct domain_struct* domain_p)
{
	int i;
	pmd_t* cpd_p = pmd_offset(pgd_offset_k(0), 0); /* Get CPD Address */
	struct cpd_struct* cpd_stat_p;

	//printk("** cpd_deallocate_domain: %d **\n", domain_p->number);

	/* Loop through CPD entries used by this domain */
	for (cpd_stat_p = domain_p->cpd_head; cpd_stat_p != NULL;
	     cpd_stat_p = cpd_stat_p->cpd_next) {

		i = STATS_P_TO_INDEX(cpd_stat_p);

		/* Update CPD */
		cpu_set_pmd(cpd_p + i, __pmd(0));

		/* Update domain_p */
		domain_p->cpd_count--;
	}

	/* Remove deallocate domain from address-space context */

	domain_p->cpd_head = NULL;
	domain_p->region_p->domain_p = NULL;
	domain_p->region_p = NULL;

#if LAZY_DISABLE
	/* Update the domain clock since we deallocated a domain */
	domain_clock++;
#endif

	//dump_cpd(1);

	set_dacr(current->thread.dacr);
     
}

void
arch_remove_shared_vm_struct(struct vm_area_struct* vma_p)
{
	struct region_struct* region_p;

	/* If we have a region assigned update the count */
	region_p = (struct region_struct*)vma_p->vm_sharing_data;
	if (region_p == NULL)
		return;

	/* Free the shared region if this is the last user */
	if (0 == --(region_p->mm_count)) {
		cpd_deallocate_region(region_p);
	}

}

/* arch_get_unmapped_area: To minimise conflicts in the CPD, hence minimise
 *   cache/TLB flushes. Mappings are split into two categories and the relevant
 *   handler is called to allocate an address:
 *   - Shared mappings.
 *   - Private mappings.
 */
unsigned long
arch_get_unmapped_area(struct file *file_p, unsigned long addr,
		       unsigned long len, unsigned long pgoff,
		       unsigned long flags)
{
	if (len > (TASK_SIZE - CPD_UNMAPPED_BASE))
		return -ENOMEM;

	/*
	 * If the caller passed in a hint, try to give it what it 
	 * asked for
	 */
	if (addr) {
		struct vm_area_struct *vma;

		addr = PAGE_ALIGN(addr);
		vma = find_vma(current->mm, addr);
		if (TASK_SIZE - len >= addr &&
		    (!vma || addr + len <= vma->vm_start)) {
			return addr;
		}
	}

	if (flags & MAP_SHARED) {
		return cpd_get_shared_area(file_p, addr, len, pgoff, flags);
	}
	return cpd_get_private_area(file_p, addr, len, pgoff, flags);
}

/*
 * cpd_next_area: Allocates a starting point to search from.
 * Notes:
 *  - Currently ignores the addr argument, might remove.
 */
static inline unsigned long
cpd_next_area(unsigned long given_addr)
{
	unsigned long addr = next_unmapped;

	next_unmapped = (next_unmapped >= (TASK_SIZE - MEGABYTE(1)) ?
			 PAGE_ALIGN(CPD_UNMAPPED_BASE) : next_unmapped + MEGABYTE(1));

	return addr;
}

/* cpd_get_shared_area: This function tries to allocate the shared mapping to
 *   the same address it is allocated to in other address spaces. If it cannot
 *   or this is the first instance of the shared mapping it will allocate a new
 *   unique area.
 * Notes:
 *   - Not sure if we need to check file_p. Can we have shared mappings without
 *     a file linking them?
 *   - If shared domains are used this will promote the situation where the
 *     shared mappings are at the same memory location which is what we want to
 *     avoid aliases.
 *   - This will lead to incorrect behaviour as really to be truely sharing
 *     the length of the new and existing mappings have to be the same and the
 *     perms have to be the same otherwise one of the address-spaces gets to see
 *     more or get to do more with it. This is limited by max len/permissions of
 *     the set of address-spaces sharing the mapping at this VA.
 */
unsigned long
cpd_get_shared_area(struct file *file_p, unsigned long addr, unsigned long len,
		    unsigned long pgoff, unsigned long flags)
{
	struct vm_area_struct* vma_priv_p; /* VMA in current address-space */
	struct vm_area_struct* vma_shrd_p; /* VMA of address-space sharing mapping */

	if (file_p) {
		for (vma_shrd_p = file_p->f_dentry->d_inode->i_mapping->i_mmap_shared;
		     vma_shrd_p; vma_shrd_p = vma_shrd_p->vm_next_share) {
			/* Check we are looking at the 'same' part of the file */
			if (vma_shrd_p->vm_pgoff == pgoff) {
				addr = vma_shrd_p->vm_start;
				vma_priv_p = find_vma(current->mm, addr);

				/* Will it fit in current address space? */
				if (!vma_priv_p || ((addr + len) <= vma_priv_p->vm_start)) {
					/* It does so use the same virtual address */
					return addr;
				}
			}
		}

		/* This is the first mapping for the file so allocate a unique area */
		return cpd_get_unique_area(file_p, addr, len, pgoff, flags);
	}

	/* Can't share so simply map private */
	return cpd_get_private_area(file_p, addr, len, pgoff, flags);

}

/*
 *  cpd_get_unique_area: This function allocates a unique 1MB region for the
 *			 allocation in the processes address space. It tries 
 *			 to allocate a region which will not lead to CPD 
 *			 conflicts.
 * Notes:
 *   - Currently it simply cycles through 1MB regions between CPD_UNMAPPED_BASE
 *     and TASK_SIZE. This should be made smarter to take into account the
 *     following:
 *     * The contention of the 1MB region. A 1MB region already allocated to
 *       another task is probably a bad idea since it will lead to CPD
 *       conflicts.
 */
unsigned long
cpd_get_unique_area(struct file *file_p, unsigned long addr, unsigned long len,
		    unsigned long pgoff, unsigned long flags)
{
	char wrap = 0;
	char dense = 0;
	unsigned long start = cpd_next_area(addr);
	struct vm_area_struct *vma_p;
  
	addr = start;

	for (vma_p = find_vma(current->mm, addr);;
	     addr = vma_p->vm_end, vma_p = vma_p->vm_next) {
		/* At this point:  (!vma || addr < vma->vm_end). */
		if ((TASK_SIZE - len) < addr) { /* Wrap around */
			addr = PAGE_ALIGN(CPD_UNMAPPED_BASE);
			wrap = 1;
		}
    
		if (wrap && ((start - len) < addr)) {
			if(dense) { /* Mapping doesn't fit */
				return -ENOMEM;
			}
      
			wrap = 0;
			dense = 1; /* Start 2nd run, buddy up in 1MB slots already being used */
		}

		if (!dense && vma_p &&
		    (MASK_MEGABYTE(addr, 1) == MASK_MEGABYTE(vma_p->vm_start, 1))) {
			continue; /* First run try keep mapping in a unique 1MB slot */
		}

		if (!vma_p || addr + len <= vma_p->vm_start) {
			return addr;
		}
	}
}

/* cpd_get_private_area: This function attempts to allocate an address inside
 *   the process 32MB relocated area of its address space. If no suitable
 *   region is found cpd_get_alloced_area() is called.
 *
 * Notes:
 *   - vma_p will never be NULL as a 4KB dummy VMA is mapped at 31MB.
 *   - vma__prev_p will never be NULL as the brk vm_area is there or the loop
 *     invarient has failed.
 */
unsigned long
cpd_get_private_area(struct file *file_p, unsigned long addr, unsigned long len,
		     unsigned long pgoff, unsigned long flags)
{
	unsigned int start, end;
	struct vm_area_struct* vma_p;
	struct vm_area_struct* vma_prev_p;

	if (len < ARMPID_BRK_LIMIT) {
		/* Loop till we hit brk */
		for (start = end = ARMPID_BRK_LIMIT; 
		     start >= PAGE_SIZE && len < end;
		     end = vma_prev_p->vm_start) {
      
			start = end - len;
			vma_p = find_vma_prev(current->mm, end, &vma_prev_p);
      
			if (start >= vma_prev_p->vm_end) {
				return start;
			}

			if (!vma_prev_p) {
				break;
			}
		}
	}
	return cpd_get_alloced_area(file_p, addr, len, pgoff, flags);
}

/* cpd_get_alloced_area: This function attempts to allocate a 1MB region already
 *   in use by the process that the new mapping fits into. If no suitable
 *   region is found a unique one is allocated.
 */
unsigned long
cpd_get_alloced_area(struct file *file_p, unsigned long addr, unsigned long len,
		     unsigned long pgoff, unsigned long flags)
{
	unsigned long slot; /* 1MB slot being checked */
	struct vm_area_struct* vma_p;

	if (len <= MEGABYTE(1)) { /* Allocations of 1MB or more get a unique area */
		addr = CPD_UNMAPPED_BASE;

		/* For each vm_area see if we can buddy up */
		for (vma_p = find_vma(current->mm, addr); vma_p;
		     addr = vma_p->vm_end, vma_p = vma_p->vm_next) {
			slot = MASK_MEGABYTE(vma_p->vm_start, 1); /* vm_area's 1MB start slot */
			if (addr < slot) { /* Seek to 1MB's slots start if not already in it */
				addr = slot;
			}
      
			/* Try to allocate in the 1MB the vm_area starts in */
			if ((addr + len) <= vma_p->vm_start) { /* Fits */
				return addr;
			}
		}
	}

	return cpd_get_unique_area(file_p, addr, len, pgoff, flags);

}
     
/* cpd_set_pmd: Insures coherence of CPD with user pmd modifications.
 * Assumptions:
 *   - CPD contains kernel mappings which are always coherent.
 *   - User changes are coherent if the old entry is not in the CPD.
 *   - The pmd being set belongs to 'current->mm'. I haven't verified this
 *     though. Please let me know if this assumption is (in)correct.
 * Action:
 *   - If the CPD entry coresponding to 'pmd_p' is tagged with the same domain 
 *     as 'pmd_p's region then update it. If current->mm doesn't exist the CPD
 *     is uneffected.
 * Notes:
 *   - This function is a bit of a mess, could do with a clean up.
 *   - The TLBs/Caches are flushed here because the CPD entry is zero'd.
 *   - Not totally sure the cache flush is needed as an unmapping would probably
 *     have already flushed the cache. However that marks it clean so this flush
 *     would be avoided...
 *   - zero'ing the CPD entry could be replaced with actually updating it. Would
 *     require an extra check. However this function is only going to be used
 *     when a new pmd is (un)allocated, in one case it couldn't have been in the
 *     CPD and in the other it is removed from the CPD anyway.
 */
void
cpd_set_pmd(pmd_t* pmd_p, pmd_t pmd)
{
	pmd_t* cpd_p;
	int domain_cpd, domain_pmd;
	struct mm_struct* mm_p = current->mm;
	unsigned long index = CPD_P_TO_INDEX(pmd_p);
             
	if (index > USER_PTRS_PER_PGD) { /* If pmd is kernel space mapping */
		cpu_set_pmd(pmd_p, pmd);
	} else { /* Must make CPD coherent */
		if (index < ARMPID_PTRS_PER_PGD && mm_p) {/* Adjust for ARM PID relocation */
			index = index + ARMPID_CPD_OFFSET(mm_p->context.pid);
		}

		cpd_p = pmd_offset(pgd_offset_k(0), 0) + index;
		domain_cpd = pmd_domain(*cpd_p);
		domain_pmd = cpd_get_active_domain(mm_p, MEGABYTE(index));

		if (domain_cpd && (domain_cpd == domain_pmd)) {
			/* update cpd_stats[] */
			cpd_stats[index].collisions++; /* Not really a collision */
			cpd_stats_del(domain_cpd, cpd_stats + index);
			domains[domain_cpd].cpd_count--;

			if (!cpd_is_domain_cache_coherent(domain_cpd)) {
				cpd_cache_clean();
			}

			cpu_set_pmd(cpd_p, __pmd(0));

			if (!cpd_is_domain_tlb_coherent(domain_cpd)) {
				cpd_tlb_clean();
			}
		}
		*pmd_p = pmd; /* Hardware never accesses user pg_dirs, only cpd code does */
	}
}

/* cpd_tlb_flush_all: Insures coherence of all TLB entries owned by the kernel,
 *   ie addresses above PAGE_OFFSET.
 * Assumptions:
 *   - Only used to make kernel mappings coherent.
 *   - Kernel mappings in CPD are always coherent since it is the master pg_dir.
 *   - Kernel mappings are not coherent with respect to the TLB.
 * Action:
 *   - Flush all TLB entries.
 */
void
cpd_tlb_flush_all(void)
{
	cpd_tlb_clean();
}

/* cpd_tlb_flush_mm: Insures coherence of all TLB entries entries owned by
 *   'mm_p' with its page table.
 * Assumptions:
 *   - No kernel mapping changed so TLB coherent with respect to them.
 *   - User TLB entries covered by CPD entries owned by 'mm_p' are not coherent.
 *   - CPD entries are updated if required via set_pmd().
 * Action:
 *   - If any CPD entries are tagged with 'mm_p's domain, if any, flush all TLB
 *     entries.
 */
void
cpd_tlb_flush_mm(struct mm_struct* mm_p)
{
	if (!cpd_is_mm_tlb_coherent(mm_p)) {
		cpd_tlb_clean();
	}
}

/* cpd_tlb_flush_range: Insures coherence of all TLB entries owned by 'mm_p' in
 *   the range 'va_start' to 'va_end'.
 * Assumptions:
 *   - No kernel mappings effected so TLB coherent with respect to them.
 *   - CPD coherent with task pg_dir entries, via set_pmd().
 *   - TLB entries covered by CPD entries owned by 'mm_p' are not coherent over
 *     the range.
 * Action:
 *   - If any entries in the CPD covered by the range are tagged with 'mm_p's
 *     domain, clean TLBs.
 * Notes:
 *   - cpd_tlb_clean() is used instead of cpu_tlb_invalidate_range() since it
 *     can mark the TLBs as clean. This should maybe be changed to only do
 *     cpd_tlb_clean() if the range is a significant portion of the TLB
 *     coverage.
 *   - The range is specified in VA's while the flushing call and CPD accesses
 *     use MVA's.
 */
void
cpd_tlb_flush_range(struct mm_struct* mm_p,
                    unsigned long va_start, unsigned long va_end)
{
	int domain;
	unsigned long mva_start = va_to_mva(va_start, mm_p);
	unsigned long mva_end = va_to_mva(va_end, mm_p);
	pmd_t* cpd_end_p = pmd_offset(pgd_offset_k(mva_end), mva_end);
	pmd_t* cpd_start_p = pmd_offset(pgd_offset_k(mva_start), mva_start);

	if (!cpd_is_mm_tlb_coherent(mm_p)) { /* 'mm_p' has dirty CPD entries */
		while (cpd_start_p <= cpd_end_p) {
			domain = pmd_domain(*cpd_start_p);

			if (!cpd_is_domain_tlb_coherent(domain) &&
			   domain_active(mm_p->context.dacr, domain)) {

				cpd_tlb_clean();
				return; /* Flush TLB only once */
			}
			cpd_start_p++;
		}
	}
}

/* cpd_tlb_flush_page: Insures coherence of TLB entry owned by 'vma_p's mm for
 *   'va_page'.
 * Assumptions:
 *   - Mapping is not a kernel one so TLB is coherent with respect to kernel
 *     mappins.
 *   - User CPD entry coherent, via set_pmd() and only a single page effected.
 *   - TLB entry mapping 'va_page' is not coherent iff covered by a CPD entry
 *     owned by the mm associated with 'vma_p'.
 * Action:
 *   - if CPD covering 'va_page' is owned by 'vma_p's mm, invalidate TLB
 *     entries.
 * Notes:
 *   - The page is specified by a VA while the flushing call and CPD access
 *     uses a MVA.
 */
void
cpd_tlb_flush_page(struct vm_area_struct* vma_p, unsigned long va_page)
{
	pmd_t cpd;
	int domain;
	unsigned long mva_page = va_to_mva(va_page, vma_p->vm_mm);

	/* Does 'vma_p's mm have any incoherencies? */
	if (!cpd_is_mm_tlb_coherent(vma_p->vm_mm)) {
		cpd = *pmd_offset(pgd_offset_k(mva_page), mva_page);
		domain = pmd_domain(cpd);

		/* 
		 * Is CPD entry's domain incoherent and active in
		 * 'vma_p's mm?
		 */
		if (!cpd_is_domain_tlb_coherent(domain) &&
		    domain_active(vma_p->vm_mm->context.dacr, domain)) {
			cpu_tlb_invalidate_page(mva_page, vma_p->vm_flags & VM_EXEC);
		}
	}
}

/* cpd_cache_flush_all: Insures coherence of all cache entries owned by the
 *   kernel, ie addresses above PAGE_OFFSET.
 * Assumptions:
 *   - Only use to make kernel memory coherent.
 * Action:
 *   - Clean/Flush all caches.
 */
void
cpd_cache_flush_all(void)
{
	cpd_cache_clean();
}

/* cpd_cache_flush_mm: Insures coherence of all cache entries owned by 'mm_p'.
 * Assumptions:
 *   - Kernel memory coherent with caches.
 *   - User memory covered by CPD entries owned by 'mm_p' are not coherent with
 *     caches.
 * Action;
 *   - If any CPD entries are tagged with 'mm_p's domain, if any, clean/flush
 *     all cache entries.
 */
void
cpd_cache_flush_mm(struct mm_struct* mm_p)
{
	if (!cpd_is_mm_cache_coherent(mm_p)) {
		cpd_cache_clean();
	}
}

/* 
 * cpd_cache_flush_range: Ensures coherence of all cache entries owned
 *                        by 'mm_p' in the range 'va_start' to 'va_end'.
 * Assumptions:
 *   - Kernel memory coherent with caches.
 *   - Cache entries covered by CPD entries owned by 'mm_p' are not coherent
 *     over the range.
 * Action:
 *   - If any entries in the CPD covered by the range are tagged with 'mm_p's
 *     domain, clean/flush all cache entries.
 * Notes:
 *   - cpd_cache_clean() is used instead of cpu_cache_clean_invalidate_range()
 *     since it can mark the caches as clean. This should maybe be changed to 
 *     only do cpd_cache_clean() if the range is a significant portion of the
 *     cache.
 *   - The range is specified in VA's while the flushing call and CPD accesses
 *     use MVA's.
 */
void
cpd_cache_flush_range(struct mm_struct* mm_p,
                      unsigned long va_start, unsigned long va_end)
{
	int domain;
	unsigned long mva_start = va_to_mva(va_start, mm_p);
	unsigned long mva_end = va_to_mva(va_end, mm_p);
	pmd_t* cpd_end_p = pmd_offset(pgd_offset_k(mva_end), mva_end);
	pmd_t* cpd_start_p = pmd_offset(pgd_offset_k(mva_start), mva_start);

	if (!cpd_is_mm_cache_coherent(mm_p)) { /* 'mm_p' has dirty CPD entries */ 
		while (cpd_start_p <= cpd_end_p) {

			domain = pmd_domain(*cpd_start_p);

			if (!cpd_is_domain_cache_coherent(domain) &&
			    domain_active(mm_p->context.dacr, domain)) {

				cpd_cache_clean();
				return; /* Flush caches only once */
			}
			cpd_start_p++;
		}
	}
}

/* 
 * cpd_cache_flush_page: Ensures coherency of cache entries owned 
 *			 by 'vma_p's mm for 'va_page'.
 * Assumptions:
 *   - Kernel memory coherent with caches.
 *   - User caches entries covered by 'va_page' are not coherent iff covered by
 *     a CPD entry owned by the mm associated with 'vma_p'.
 * Action:
 *   - if CPD covering 'va_page' is owned by 'vma_p's mm, invalidate caches
 *     entries.
 * Notes:
 *   - The page is specified by a VA while the flushing call and CPD access
 *     uses a MVA.
 */
void
cpd_cache_flush_page(struct vm_area_struct* vma_p, unsigned long va_page)
{
	pmd_t cpd;
	int domain;
	unsigned long mva_page = va_to_mva(va_page, vma_p->vm_mm);

	/* Does 'vma_p's mm have any incoherencies? */
	if (!cpd_is_mm_cache_coherent(vma_p->vm_mm)) {
		cpd = *pmd_offset(pgd_offset_k(mva_page), mva_page);
		domain = pmd_domain(cpd);

		/* Is CPD entry's domain incoherent and active in 'vma_p's mm? */
		if (!cpd_is_domain_cache_coherent(domain) &&
		    domain_active(vma_p->vm_mm->context.dacr, domain)) {
			cpu_cache_clean_invalidate_range(mva_page, mva_page + PAGE_SIZE,
							 vma_p->vm_flags & VM_EXEC);
		}
	}
}

/*
 * dump_cpd: Dumps the entire CPD for debugging.
 *
 * Notes:
 *   - user = 0, all entries dumped; user = 1 only user entries dumped.
 *     user = -1, dump domain 0 entries only
 */
void
dump_cpd(int user)
{
	int i;
	int domain;
	pmd_t *cpd_p = pmd_offset(pgd_offset_k(0), 0); /* Get CPD Address */

	for (i = 0; i < PTRS_PER_PGD; i++) {
		domain = pmd_domain(cpd_p[i]);
		if ((user == -1) && (domain == 0) && pmd_val(cpd_p[i])) {
			printk("** dump_cpd() cpd[%d] 0x%x domain 0 **\n",
			       i, (unsigned int)pmd_val(cpd_p[i]));
			continue;
		}
		if (!user || (domain >= DOMAIN_START && domain <= DOMAIN_END)) {
			printk("** dump_cpd() cpd[%d] 0x%x domain tag %d **\n",
			       i, (unsigned int)pmd_val(cpd_p[i]), domain);
		}
	}
	printk("\n");
}

/* dump_cpd_stat: Dumps info from a cpd_struct */
void
dump_cpd_stat(struct cpd_struct* cpd_stat_p)
{
	printk("** dump_cpd_stat: cpd_stat_p 0x%x collisions %d **\n",
	       (unsigned int)cpd_stat_p, (unsigned int)cpd_stat_p->collisions);
	printk("** dump_cpd_stat: cpd_next 0x%x cpd_prev 0x%x **\n",
	       (unsigned int)cpd_stat_p->cpd_next,
	       (unsigned int)cpd_stat_p->cpd_prev);

}

/* dump_cpd_stats: Dump all cpd_stats associated with a domain */
void
dump_cpd_stats(int domain)
{
	struct cpd_struct* cpd_stat_p = domains[domain].cpd_head;

	printk("** dump_cpd_stats: domain %d **\n", domain);

	while (cpd_stat_p != NULL) {
		dump_cpd_stat(cpd_stat_p);

		cpd_stat_p = cpd_stat_p->cpd_next;
	}
}

/* dump_vma_single: Dumps info for a vm_area struct. */
void
dump_vma_single(struct vm_area_struct* vma_p)
{
	fassert(vma_p);

	printk("** dump_vma_single: start 0x%x end 0x%x **\n",
	       (unsigned int)vma_p->vm_start, (unsigned int)vma_p->vm_end);

}

/* dump_vma: Dump all vma's in a address-space */
void
dump_vma(struct mm_struct* mm_p)
{
	struct vm_area_struct* vma_p;

	printk("** dump_vma: mm_p 0x%x **\n", (unsigned int)mm_p);
	fassert(mm_p);

	for (vma_p = mm_p->mmap; vma_p; vma_p = vma_p->vm_next) {
		dump_vma_single(vma_p);
	}

}
