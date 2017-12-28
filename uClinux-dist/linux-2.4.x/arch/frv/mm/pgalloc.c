/* pgalloc.c: page directory & page table allocation
 *
 * Copyright (C) 2004 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <asm/pgalloc.h>

pgd_t swapper_pg_dir[PTRS_PER_PGD] __attribute__((aligned(PAGE_SIZE)));

pgd_t *get_pgd_slow(void)
{
	struct page *page;
	pgd_t *pgd;

	page = alloc_pages(GFP_KERNEL, 0);
	if (!page)
		return 0;

	pgd = (pgd_t *) page_address(page);
	clear_page(pgd);

	memset(pgd, 0, USER_PTRS_PER_PGD * sizeof(pgd_t));
	memcpy(pgd + USER_PTRS_PER_PGD,
	       swapper_pg_dir + USER_PTRS_PER_PGD,
	       (PTRS_PER_PGD - USER_PTRS_PER_PGD) * sizeof(pgd_t));

	flush_dcache_page(page);
	return pgd;
}

pte_t *pte_alloc_one(struct mm_struct *mm, unsigned long address)
{
	struct page *page;
	pte_t *pte;

	page = alloc_pages(GFP_KERNEL, 0);
	if (!page)
		return 0;

	pte = (pte_t *) page_address(page);
	clear_page(pte);
	flush_dcache_page(page);
	return pte;
}

int do_check_pgt_cache(int low, int high)
{
	int freed = 0;

	if (pgtable_cache_size > high) {
		do {
			if (pgd_quicklist) {
				free_pgd_slow(get_pgd_fast());
				freed++;
			}
			if (pmd_quicklist) {
				pmd_free_slow(pmd_alloc_one_fast(NULL, 0));
				freed++;
			}
			if (pte_quicklist) {
				pte_free_slow(pte_alloc_one_fast(NULL, 0));
				freed++;
			}
		} while(pgtable_cache_size > low);
	}

	return freed;
}

void __set_pmd(pmd_t *pmdptr, unsigned long __pmd)
{
	unsigned long *__ste_p = pmdptr->ste;
	int loop;

	if (!__pmd) {
		memset(__ste_p, 0, PME_SIZE);
	}
	else {
		for (loop = PME_SIZE; loop > 0; loop -= 4) {
			*__ste_p++ = __pmd;
			__pmd += __frv_PT_SIZE;
		}
	}

	frv_dcache_writeback((unsigned long) pmdptr, (unsigned long) (pmdptr + 1));
}
