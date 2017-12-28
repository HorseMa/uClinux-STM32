/* pgtable.h: FR-V page table mangling
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
 *	include/asm-m68knommu/pgtable.h
 *	include/asm-i386/pgtable.h
 */

#ifndef _ASM_PGTABLE_H
#define _ASM_PGTABLE_H

#include <linux/config.h>
#include <asm/setup.h>


#ifdef CONFIG_UCLINUX

#define pgd_present(pgd)	(1)		/* pages are always present on NO_MM */
#define kern_addr_valid(addr)	(1)

#define PAGE_NONE		__pgprot(0)	/* these mean nothing to NO_MM */
#define PAGE_SHARED		__pgprot(0)	/* these mean nothing to NO_MM */
#define PAGE_COPY		__pgprot(0)	/* these mean nothing to NO_MM */
#define PAGE_READONLY		__pgprot(0)	/* these mean nothing to NO_MM */
#define PAGE_KERNEL		__pgprot(0)	/* these mean nothing to NO_MM */
#else /* CONFIG_UCLINUX */

#ifndef __ASSEMBLY__
extern void asmlinkage __flush_tlb_all(void);
extern void asmlinkage __flush_tlb_mm(unsigned long contextid);
extern void asmlinkage __flush_tlb_page(unsigned long contextid, unsigned long start);
extern void asmlinkage __flush_tlb_range(unsigned long contextid,
					 unsigned long start, unsigned long end);
#endif /* !__ASSEMBLY__ */

/*
 * ZERO_PAGE is a global shared page that is always zero: used
 * for zero-mapped memory areas etc..
 */
#ifndef __ASSEMBLY__
extern unsigned long empty_zero_page;
#define ZERO_PAGE(vaddr)	virt_to_page(empty_zero_page)
#endif

/*
 * we use 2-level page tables, folding the PMD (mid-level table) into the PGE (top-level entry)
 * [see Documentation/fujitsu/frv/mmu-layout.txt]
 *
 * Page Directory:
 *  - Size: 16Kb
 *  - 64 PGEs per PGD
 *  - Each PGE holds 1 PMD and covers 64Mb
 *
 * Page Mid-Level Directory
 *  - 1 PME per PMD
 *  - Each PME holds 64 STEs, all of which point to separate chunks of the same Page Table
 *  - All STEs are instantiated at the same time
 *  - Size: 256b
 *
 * Page Table
 *  - Size: 16Kb
 *  - 4096 PTEs per PT
 *  - Each Linux PT is subdivided into 64 FR451 PT's, each of which holds 64 entries
 *
 * total PTEs
 *	= 64 PGEs * 1 PMEs * 4096 PTEs
 *	= 64 PGEs * 64 STEs * 64 PTEs/FR451-PT
 *	= 262144 (or 256 * 1024)
 */
#define PGDIR_SHIFT		26
#define PGDIR_SIZE		(1UL << PGDIR_SHIFT)
#define PGDIR_MASK		(~(PGDIR_SIZE - 1))
#define PTRS_PER_PGD		64

#define PMD_SHIFT		26
#define PMD_SIZE		(1UL << PMD_SHIFT)
#define PMD_MASK		(~(PMD_SIZE - 1))
#define PTRS_PER_PMD		1
#define PME_SIZE		256

#define __frv_PT_SIZE		256

#define PTRS_PER_PTE		4096

#define USER_PTRS_PER_PGD	(TASK_SIZE / PGDIR_SIZE)
#define FIRST_USER_PGD_NR	0

#define USER_PGD_PTRS		(PAGE_OFFSET >> PGDIR_SHIFT)
#define KERNEL_PGD_PTRS		(PTRS_PER_PGD - USER_PGD_PTRS)

#define TWOLEVEL_PGDIR_SHIFT	26
#define BOOT_USER_PGD_PTRS	(__PAGE_OFFSET >> TWOLEVEL_PGDIR_SHIFT)
#define BOOT_KERNEL_PGD_PTRS	(PTRS_PER_PGD - BOOT_USER_PGD_PTRS)

#ifndef __ASSEMBLY__

extern pgd_t swapper_pg_dir[PTRS_PER_PGD];

#define pte_ERROR(e) \
	printk("%s:%d: bad pte %08lx.\n", __FILE__, __LINE__, (e).pte)
#define pmd_ERROR(e) \
	printk("%s:%d: bad pmd %08lx.\n", __FILE__, __LINE__, pmd_val(e))
#define pgd_ERROR(e) \
	printk("%s:%d: bad pgd %08lx.\n", __FILE__, __LINE__, pmd_val(pgd_val(e)))

/*
 * The "pgd_xxx()" functions here are trivial for a folded two-level
 * setup: the pgd is never bad, and a pmd always exists (as it's folded
 * into the pgd entry)
 */
static inline int pgd_none(pgd_t pgd)		{ return 0; }
static inline int pgd_bad(pgd_t pgd)		{ return 0; }
static inline int pgd_present(pgd_t pgd)	{ return 1; }
#define pgd_clear(xp)				do { } while (0)

/*
 * Certain architectures need to do special things when PTEs
 * within a page table are directly modified.  Thus, the following
 * hook is made available.
 */
#define set_pte(pteptr, pteval)				\
do {							\
	*(pteptr) = (pteval);				\
	asm volatile("dcf %M0" :: "U"(*pteptr));	\
} while(0)

#define set_pte_atomic(pteptr, pteval)		set_pte((pteptr), (pteval))

/*
 * (pmds are folded into pgds so this doesn't get actually called,
 * but the define is needed for a generic inline function.)
 */
#define set_pgd(pgdptr, pgdval)			(*(pgdptr) = pgdval)

extern void __set_pmd(pmd_t *pmdptr, unsigned long __pmd);

#define set_pmd(pmdptr, pmdval)			\
do {						\
	__set_pmd((pmdptr), (pmdval).ste[0]);	\
} while(0)

#define pgd_page(pgd)				((unsigned long) __va(pgd_val(pgd) & PAGE_MASK))

#define __pmd_index(address)			0

static inline pmd_t *pmd_offset(pgd_t *dir, unsigned long address)
{
	return (pmd_t *) dir + __pmd_index(address);
}

#define pte_same(a, b)		((a).pte == (b).pte)
#define pte_page(x)		(mem_map + ((unsigned long)(((x).pte >> PAGE_SHIFT))))
#define pte_none(x)		(!(x).pte)
#define __mk_pte(page_nr,pgprot) __pte(((page_nr) << PAGE_SHIFT) | pgprot_val(pgprot))

#define VMALLOC_VMADDR(x)	((unsigned long) (x))

#endif /* !__ASSEMBLY__ */

/*
 * control flags in AMPR registers and TLB entries
 */
#define _PAGE_BIT_PRESENT	xAMPRx_V_BIT
#define _PAGE_BIT_WP		DAMPRx_WP_BIT
#define _PAGE_BIT_NOCACHE	xAMPRx_C_BIT
#define _PAGE_BIT_SUPER		xAMPRx_S_BIT
#define _PAGE_BIT_ACCESSED	xAMPRx_RESERVED8_BIT
#define _PAGE_BIT_DIRTY		xAMPRx_M_BIT
#define _PAGE_BIT_NOTGLOBAL	xAMPRx_NG_BIT

#define _PAGE_PRESENT		xAMPRx_V
#define _PAGE_WP		DAMPRx_WP
#define _PAGE_NOCACHE		xAMPRx_C
#define _PAGE_SUPER		xAMPRx_S
#define _PAGE_ACCESSED		xAMPRx_RESERVED8	/* accessed if set */
#define _PAGE_DIRTY		xAMPRx_M
#define _PAGE_NOTGLOBAL		xAMPRx_NG

#define _PAGE_RESERVED_MASK	(xAMPRx_RESERVED8 | xAMPRx_RESERVED13)

#define _PAGE_PROTNONE		0x000	/* If not present */

#define _PAGE_CHG_MASK		(PTE_MASK | _PAGE_ACCESSED | _PAGE_DIRTY)

#define __PGPROT_BASE \
	(_PAGE_PRESENT | xAMPRx_SS_16Kb | xAMPRx_D | _PAGE_NOTGLOBAL | _PAGE_ACCESSED)

#define PAGE_NONE	__pgprot(_PAGE_PROTNONE | _PAGE_ACCESSED)
#define PAGE_SHARED	__pgprot(__PGPROT_BASE)
#define PAGE_COPY	__pgprot(__PGPROT_BASE | _PAGE_WP)
#define PAGE_READONLY	__pgprot(__PGPROT_BASE | _PAGE_WP)

#define __PAGE_KERNEL		(__PGPROT_BASE | _PAGE_SUPER | _PAGE_DIRTY)
#define __PAGE_KERNEL_NOCACHE	(__PGPROT_BASE | _PAGE_SUPER | _PAGE_DIRTY | _PAGE_NOCACHE)
#define __PAGE_KERNEL_RO	(__PGPROT_BASE | _PAGE_SUPER | _PAGE_DIRTY | _PAGE_WP)

#define MAKE_GLOBAL(x) __pgprot((x) & ~_PAGE_NOTGLOBAL)

#define PAGE_KERNEL		MAKE_GLOBAL(__PAGE_KERNEL)
#define PAGE_KERNEL_RO		MAKE_GLOBAL(__PAGE_KERNEL_RO)
#define PAGE_KERNEL_NOCACHE	MAKE_GLOBAL(__PAGE_KERNEL_NOCACHE)

#define _PAGE_TABLE		xAMPRx_V

#ifndef __ASSEMBLY__

/*
 * The FR451 can do execute protection by virtue of having separate TLB miss handlers for
 * instruction access and for data access. However, we don't have enough reserved bits to say
 * "execute only", so we don't bother. If you can read it, you can execute it and vice versa.
 */
#define __P000	PAGE_NONE
#define __P001	PAGE_READONLY
#define __P010	PAGE_COPY
#define __P011	PAGE_COPY
#define __P100	PAGE_READONLY
#define __P101	PAGE_READONLY
#define __P110	PAGE_COPY
#define __P111	PAGE_COPY

#define __S000	PAGE_NONE
#define __S001	PAGE_READONLY
#define __S010	PAGE_SHARED
#define __S011	PAGE_SHARED
#define __S100	PAGE_READONLY
#define __S101	PAGE_READONLY
#define __S110	PAGE_SHARED
#define __S111	PAGE_SHARED

/*
 * Define this to warn about kernel memory accesses that are
 * done without a 'verify_area(VERIFY_WRITE,..)'
 */
#undef TEST_VERIFY_AREA

#define pte_present(x)	(pte_val(x) & _PAGE_PRESENT)
#define pte_clear(xp)	do { set_pte(xp, __pte(0)); } while (0)

#define pmd_none(x)	(!pmd_val(x))
#define pmd_present(x)	(pmd_val(x) & _PAGE_PRESENT)
#define	pmd_bad(x)	(pmd_val(x) & 0xf0)
#define pmd_clear(xp)	do { __set_pmd(xp, 0); } while(0)


#define pages_to_mb(x) ((x) >> (20-PAGE_SHIFT))

/*
 * The following only work if pte_present() is true.
 * Undefined behaviour if not..
 */
static inline int pte_read(pte_t pte)		{ return !((pte).pte & _PAGE_SUPER); }
static inline int pte_exec(pte_t pte)		{ return !((pte).pte & _PAGE_SUPER); }
static inline int pte_dirty(pte_t pte)		{ return (pte).pte & _PAGE_DIRTY; }
static inline int pte_young(pte_t pte)		{ return (pte).pte & _PAGE_ACCESSED; }
static inline int pte_write(pte_t pte)		{ return !((pte).pte & _PAGE_WP); }

static inline pte_t pte_rdprotect(pte_t pte)	{ (pte).pte |= _PAGE_SUPER; return pte; }
static inline pte_t pte_exprotect(pte_t pte)	{ (pte).pte |= _PAGE_SUPER; return pte; }
static inline pte_t pte_mkclean(pte_t pte)	{ (pte).pte &= ~_PAGE_DIRTY; return pte; }
static inline pte_t pte_mkold(pte_t pte)	{ (pte).pte &= ~_PAGE_ACCESSED; return pte; }
static inline pte_t pte_wrprotect(pte_t pte)	{ (pte).pte |= _PAGE_WP; return pte; }
static inline pte_t pte_mkread(pte_t pte)	{ (pte).pte &= ~_PAGE_SUPER; return pte; }
static inline pte_t pte_mkexec(pte_t pte)	{ (pte).pte &= ~_PAGE_SUPER; return pte; }
static inline pte_t pte_mkdirty(pte_t pte)	{ (pte).pte |= _PAGE_DIRTY; return pte; }
static inline pte_t pte_mkyoung(pte_t pte)	{ (pte).pte |= _PAGE_ACCESSED; return pte; }
static inline pte_t pte_mkwrite(pte_t pte)	{ (pte).pte &= ~_PAGE_WP; return pte; }

static inline int ptep_test_and_clear_dirty(pte_t *ptep)
{
	int i = test_and_clear_bit(_PAGE_BIT_DIRTY, ptep);
	asm volatile("dcf %M0" :: "U"(*ptep));
	return i;
}

static inline int ptep_test_and_clear_young(pte_t *ptep)
{
	int i = test_and_clear_bit(_PAGE_BIT_ACCESSED, ptep);
	asm volatile("dcf %M0" :: "U"(*ptep));
	return i;
}

static inline pte_t ptep_get_and_clear(pte_t *ptep)
{
	unsigned long x = xchg(&ptep->pte, 0);
	asm volatile("dcf %M0" :: "U"(*ptep));
	return __pte(x);
}

static inline void ptep_set_wrprotect(pte_t *ptep)
{
	set_bit(_PAGE_BIT_WP, ptep);
	asm volatile("dcf %M0" :: "U"(*ptep));
}

static inline void ptep_mkdirty(pte_t *ptep)
{
	set_bit(_PAGE_BIT_DIRTY, ptep);
	asm volatile("dcf %M0" :: "U"(*ptep));
}

/*
 * Conversion functions: convert a page and protection to a page entry,
 * and a page entry and page directory to the page they refer to.
 */

#define mk_pte(page, pgprot)	__mk_pte((page) - mem_map, (pgprot))

/* This takes a physical page address that is used by the remapping functions */
#define mk_pte_phys(physpage, pgprot)	__mk_pte((physpage) >> PAGE_SHIFT, pgprot)

static inline pte_t pte_modify(pte_t pte, pgprot_t newprot)
{
	pte.pte &= _PAGE_CHG_MASK;
	pte.pte |= pgprot_val(newprot);
	return pte;
}

#define page_pte(page)	page_pte_prot(page, __pgprot(0))

#define pmd_page(pmd) ((unsigned long) __va(pmd_val(pmd) & PAGE_MASK))

/* to find an entry in a page-table-directory. */
#define pgd_index(address) ((address >> PGDIR_SHIFT) & (PTRS_PER_PGD - 1))

#define __pgd_offset(address) pgd_index(address)

#define pgd_offset(mm, address) ((mm)->pgd + pgd_index(address))

/* to find an entry in a kernel page-table-directory */
#define pgd_offset_k(address) pgd_offset(&init_mm, address)

/* Find an entry in the third-level page table.. */
#define __pte_index(address) ((address >> PAGE_SHIFT) & (PTRS_PER_PTE - 1))
#define pte_offset(dir, address) ((pte_t *) pmd_page(*(dir)) + __pte_index(address))

/*
 * preload information about a newly instantiated PTE into the SCR0/SCR1 PGE cache
 */
static inline void update_mmu_cache(struct vm_area_struct *vma, unsigned long address, pte_t pte)
{
	unsigned long ampr;
	pmd_t *pme = pmd_offset(pgd_offset(current->mm, address), address);

	ampr = pme->ste[0] & 0xffffff00;
	ampr |= xAMPRx_L | xAMPRx_SS_16Kb | xAMPRx_S | xAMPRx_C | xAMPRx_V;

	asm volatile("movgs %0,scr0\n"
		     "movgs %0,scr1\n"
		     "movgs %1,dampr4\n"
		     "movgs %1,dampr5\n"
		     :
		     : "r"(address), "r"(ampr)
		     );
}

#ifdef CONFIG_PROC_FS
extern char *proc_pid_status_frv_cxnr(struct mm_struct *mm, char *buffer);
#endif

/*
 * Encode and de-code a swap entry (swap entries have xAMPR.V bits clear)
 */
#define SWP_TYPE(x)			(((x).val >> 1) & 0x3f)
#define SWP_OFFSET(x)			((x).val >> 8)
#define SWP_ENTRY(type, offset)		((swp_entry_t) { ((type) << 1) | ((offset) << 8) })
#define pte_to_swp_entry(xpte)		((swp_entry_t) { (xpte).pte })
#define swp_entry_to_pte(x)		((pte_t) { (x).val })

struct page;
int change_page_attr(struct page *, int, pgprot_t prot);

/* Needs to be defined here and not in linux/mm.h, as it is arch dependent */
#define PageSkip(page)		(0)
#define kern_addr_valid(addr)	(1)

#define io_remap_page_range	remap_page_range

#endif /* !__ASSEMBLY__ */
#endif /* CONFIG_UCLINUX */

/*
 * No page table caches to initialise
 */
#define pgtable_cache_init()   do { } while (0)

#ifndef __ASSEMBLY__
extern void paging_init(void);
#endif /* !__ASSEMBLY__ */

#endif /* _ASM_PGTABLE_H */
