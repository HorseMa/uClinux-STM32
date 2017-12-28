/* pgalloc.h: Page allocation and TLB flushing routines for FR-V
 *
 * Copyright (C) 2004 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * Derived from:
 *	include/asm-m68knommu/pgalloc.h
 *	include/asm-i386/pgalloc.h
 */
#ifndef _ASM_PGALLOC_H
#define _ASM_PGALLOC_H

#include <linux/config.h>
#include <asm/setup.h>
#include <asm/virtconvert.h>

#ifndef CONFIG_UCLINUX

#define pgd_quicklist		(current_cpu_data.pgd_quick)
#define pmd_quicklist		NULL
#define pte_quicklist		(current_cpu_data.pte_quick)
#define pgtable_cache_size	(current_cpu_data.pgtable_cache_sz)

#define pmd_populate(mm, pmd, pte)  __set_pmd(pmd, __pa(pte) | _PAGE_TABLE)

/*
 * Allocate and free page tables.
 */
extern pgd_t *get_pgd_slow(void);

static inline pgd_t *get_pgd_fast(void)
{
	unsigned long *ret;

	if ((ret = pgd_quicklist) != NULL) {
		pgd_quicklist = (unsigned long *) *ret;
		ret[0] = 0;
		asm volatile ("dcf %M0" :: "m"(ret[0]));
		pgtable_cache_size--;
	}
	else {
		ret = (unsigned long *) get_pgd_slow();
	}

	return (pgd_t *) ret;
}

static inline void free_pgd_fast(pgd_t *pgd)
{
	*(unsigned long *) pgd = (unsigned long) pgd_quicklist;
	pgd_quicklist = (unsigned long *) pgd;
	pgtable_cache_size++;
}

#define free_pgd_slow(pgd) do { free_page((unsigned long) (pgd)); } while(0)

extern pte_t *pte_alloc_one(struct mm_struct *mm, unsigned long address);

static inline pte_t *pte_alloc_one_fast(struct mm_struct *mm, unsigned long address)
{
	unsigned long *ret;

	if ((ret = (unsigned long *) pte_quicklist) != NULL) {
		pte_quicklist = (unsigned long *) *ret;
		ret[0] = ret[1];
		asm volatile ("dcf %M0" :: "m"(ret[0]));
		pgtable_cache_size--;
	}
	return (pte_t *) ret;
}

static inline void pte_free_fast(pte_t *pte)
{
	*(unsigned long *) pte = (unsigned long) pte_quicklist;
	pte_quicklist = (unsigned long *) pte;
	pgtable_cache_size++;
}

#define pte_free_slow(pte) do { free_page((unsigned long) (pte)); } while(0)

#define pte_free(pte)	pte_free_fast(pte)
#define pgd_free(pgd)	free_pgd_slow(pgd)
#define pgd_alloc(mm)	get_pgd_fast()

/*
 * allocating and freeing a pmd is trivial: the 1-entry pmd is
 * inside the pgd, so has no extra memory associated with it.
 * (In the PAE case we free the pmds as part of the pgd.)
 */
#define pmd_alloc_one_fast(mm, addr)	({ BUG(); ((pmd_t *) 1); })
#define pmd_alloc_one(mm, addr)		({ BUG(); ((pmd_t *) 2); })
#define pmd_free_slow(x)		do { } while (0)
#define pmd_free_fast(x)		do { } while (0)
#define pmd_free(x)			do { } while (0)
#define pgd_populate(mm, pmd, pte)	BUG()

extern int do_check_pgt_cache(int, int);

#endif /* !CONFIG_UCLINUX */

/*
 * Cache handling functions
 */
/* Those implemented in arch/frvnommu/lib/cache.S: */
extern void frv_dcache_writeback(unsigned long start, unsigned long size);
extern void frv_cache_invalidate(unsigned long start, unsigned long size);
extern void frv_icache_invalidate(unsigned long start, unsigned long size);
extern void frv_cache_wback_inv(unsigned long start, unsigned long size);

static inline void __flush_cache_all(void)
{
	asm volatile("	dcef	@(gr0,gr0),#1	\n"
		     "	icei	@(gr0,gr0),#1	\n"
		     "	membar			\n"
		     : : : "memory"
		     );
}

/* Needed only for virtually-indexed caches */
#define flush_cache_all()			do {} while(0)
#define flush_cache_mm(mm)			do {} while(0)
#define flush_cache_range(mm, start, end)	do {} while(0)
#define flush_cache_page(vma, vmaddr)		do {} while(0)

/* dcache/icache coherency... */
#ifndef CONFIG_UCLINUX
extern void flush_dcache_page(struct page *page);
#else
static inline void flush_dcache_page(struct page *page)
{
	unsigned long addr = page_to_phys(page);
	frv_dcache_writeback(addr, addr + PAGE_SIZE);
}
#endif

static inline void flush_page_to_ram(struct page *page)
{
	flush_dcache_page(page);
}

static inline void flush_icache(void)
{
	__flush_cache_all();
}

static inline void flush_icache_range(unsigned long start, unsigned long end)
{
	frv_cache_wback_inv(start, end);
}

#ifndef CONFIG_UCLINUX
extern void flush_icache_user_range(struct vm_area_struct *vma, struct page *page,
				    unsigned long start, unsigned long len);
#else
static inline void flush_icache_user_range(struct vm_area_struct *vma, struct page *page,
					   unsigned long start, unsigned long len)
{
	frv_cache_wback_inv(start, start + len);
}
#endif

static inline void flush_icache_page(struct vm_area_struct *vma, struct page *page)
{
	flush_icache_user_range(vma, page, page_to_phys(page), PAGE_SIZE);
}

/*
 * TLB flushing functions
 */
#ifndef CONFIG_UCLINUX
#define flush_tlb()				flush_tlb_all()

static inline void flush_tlb_all(void)
{
	__flush_tlb_all();
}

static inline void flush_tlb_mm(struct mm_struct *mm)
{
	__flush_tlb_mm(mm->context.id);
}

static inline void flush_tlb_range(struct mm_struct *mm,
	unsigned long start, unsigned long end)
{
	__flush_tlb_range(mm->context.id, start, end);
}

static inline void flush_tlb_page(struct vm_area_struct *vma,
	unsigned long addr)
{
	__flush_tlb_page(vma->vm_mm->context.id, addr);
}

#define flush_tlb_pgtables(mm,start,end)	asm volatile("movgs gr0,scr0 ! movgs gr0,scr1");

#else
#define flush_tlb()				BUG();
#define flush_tlb_all()				BUG();
#define flush_tlb_page(vma,addr)		BUG();
#define flush_tlb_range(mm,start,end)		BUG();
#define flush_tlb_pgtables(mm,start,end)	BUG();

#endif

#endif /* _ASM_PGALLOC_H */
