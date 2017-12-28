/*
 *  linux/arch/arm/mm/proc-oki_ml67400x.S: MMU functions for OKI_ML67400X
 *
 *  Copyright (C) 2000 Steve Hill (sjhill@cotw.com)
 *                     Rob Scott (rscott@mtrob.fdns.net)
 *  Copyright (C) 2000 ARM Limited, Deep Blue Solutions Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 * These are the low level assembler for performing cache and TLB
 * functions on the ARM720T.  The ARM720T has a writethrough IDC
 * cache, so we don't need to clean it.
 *
 *  Changelog:
 *   05-09-2000 SJH	Created by moving 720 specific functions
 *			out of 'proc-arm6,7.S' per RMK discussion
 *   07-25-2000 SJH	Added idle function.
 *   08-25-2000	DBS	Updated for integration of ARM Ltd version.
 *   12-03-2001 FG      Forked 740 function from 720
 *   10-04-2003 BJD     Forked Oki from 740
 */
#include <linux/linkage.h>
#include <asm/assembler.h>
#include <asm/constants.h>
#include <asm/procinfo.h>
#include <asm/hardware.h>

/*
 * Function: oki_ml67400x_cache_clean_invalidate_all (void)
 *	   : oki_ml67400x_cache_clean_invalidate_page (unsigned long address, int size,
 *                                    int flags)
 *
 * Params  : address	Area start address
 *	   : size	size of area
 *	   : flags	b0 = I cache as well
 *
 * Purpose : Flush all cache lines
 */
ENTRY(cpu_oki_ml67400x_cache_clean_invalidate_all)
ENTRY(cpu_oki_ml67400x_cache_clean_invalidate_range)
ENTRY(cpu_oki_ml67400x_icache_invalidate_range)
ENTRY(cpu_oki_ml67400x_icache_invalidate_page)
ENTRY(cpu_oki_ml67400x_dcache_invalidate_range)
		mov	pc, lr

/*
 * These just expect cache lines to be cleaned.  Since we have a writethrough
 * cache, we never have any dirty cachelines to worry about.
 */
ENTRY(cpu_oki_ml67400x_dcache_clean_range)
ENTRY(cpu_oki_ml67400x_dcache_clean_page)
ENTRY(cpu_oki_ml67400x_dcache_clean_entry)
ENTRY(cpu_oki_ml67400x_flush_ram_page)
		mov	pc, lr

/*
 * Function: oki_ml67400x_tlb_invalidate_all (void)
 *
 * Purpose : flush all TLB entries in all caches
 */
ENTRY(cpu_oki_ml67400x_tlb_invalidate_all)
		mov	r0, #0
		mcr	p15, 0, r0, c8, c7, 0		@ flush TLB (v4)
		mov	pc, lr

/*
 * Function: oki_ml67400x_tlb_invalidate_page (unsigned long address, int end, int flags)
 *
 * Params  : address	Area start address
 *	   : end	Area end address
 *	   : flags	b0 = I cache as well
 *
 * Purpose : flush a TLB entry
 */
ENTRY(cpu_oki_ml67400x_tlb_invalidate_range)
		mov	pc, lr

/*
 * Function: oki_ml67400x_tlb_invalidate_page (unsigned long address, int flags)
 *
 * Params  : address	Address
 *	   : flags	b0 = I-TLB as well
 *
 * Purpose : flush a TLB entry
 */
ENTRY(cpu_oki_ml67400x_tlb_invalidate_page)
		mcr	p15, 0, r0, c8, c7, 1		@ flush TLB (v4)
		mov	pc, lr

/*
 * Function: oki_ml67400x_data_abort ()
 *
 * Params  : r0 = address of aborted instruction
 *
 * Purpose : obtain information about current aborted instruction
 *
 * Returns : r0 = address of abort
 *	   : r1 != 0 if writing
 *	   : r3 = FSR
 *	   : sp = pointer to registers
 */

Ldata_ldmstm:	tst	r4, #1 << 21			@ check writeback bit
		beq	Ldata_simple
		mov	r7, #0x11
		orr	r7, r7, r7, lsl #8
		and	r0, r4, r7
		and	r2, r4, r7, lsl #1
		add	r0, r0, r2, lsr #1
		and	r2, r4, r7, lsl #2
		add	r0, r0, r2, lsr #2
		and	r2, r4, r7, lsl #3
		add	r0, r0, r2, lsr #3
		add	r0, r0, r0, lsr #8
		add	r0, r0, r0, lsr #4
		and	r7, r0, #15			@ r7 = no. of registers to transfer.
		and	r5, r4, #15 << 16		@ Get Rn
		ldr	r0, [sp, r5, lsr #14]		@ Get register
		tst	r4, #1 << 23			@ U bit
		subne	r7, r0, r7, lsl #2
		addeq	r7, r0, r7, lsl #2		@ Do correction (signed)
Ldata_saver7:	str	r7, [sp, r5, lsr #14]		@ Put register
Ldata_simple:	mrc	p15, 0, r0, c6, c0, 0		@ get FAR
		mrc	p15, 0, r3, c5, c0, 0		@ get FSR
		and	r3, r3, #255
		mov	pc, lr

ENTRY(cpu_oki_ml67400x_data_abort)
		ldr	r4, [r0]			@ read instruction causing problem
		tst	r4, r4, lsr #21			@ C = bit 20
		sbc	r1, r1, r1			@ r1 = C - 1
		and	r2, r4, #15 << 24
		add	pc, pc, r2, lsr #22		@ Now branch to the relevent processing routine
		movs	pc, lr

		b	Ldata_lateldrhpost		@ ldrh	rd, [rn], #m/rm
		b	Ldata_lateldrhpre		@ ldrh	rd, [rn, #m/rm]
		b	Ldata_unknown
		b	Ldata_unknown
		b	Ldata_lateldrpostconst		@ ldr	rd, [rn], #m
		b	Ldata_lateldrpreconst		@ ldr	rd, [rn, #m] 
		b	Ldata_lateldrpostreg		@ ldr	rd, [rn], rm
		b	Ldata_lateldrprereg		@ ldr	rd, [rn, rm]
		b	Ldata_ldmstm			@ ldm*a	rn, <rlist>
		b	Ldata_ldmstm			@ ldm*b	rn, <rlist>
		b	Ldata_unknown
		b	Ldata_unknown
		b	Ldata_simple			@ ldc	rd, [rn], #m	@ Same as ldr	rd, [rn], #m
		b	Ldata_simple			@ ldc	rd, [rn, #m]
		b	Ldata_unknown

Ldata_unknown:	@ Part of jumptable
		mov	r0, r2
		mov	r1, r4
		mov	r2, r3
		bl	baddataabort
		b	ret_from_exception

Ldata_lateldrhpre:
		tst	r4, #1 << 21			@ check writeback bit
		beq	Ldata_simple
Ldata_lateldrhpost:
		and	r5, r4, #0x00f			@ get Rm / low nibble of immediate value
		tst	r4, #1 << 22			@ if (immediate offset)
		andne	r2, r4, #0xf00			@ { immediate high nibble
		orrne	r2, r5, r2, lsr #4		@   combine nibbles } else
		ldreq	r2, [sp, r5, lsl #2]		@ { load Rm value }
		and	r5, r4, #15 << 16		@ get Rn
		ldr	r0, [sp, r5, lsr #14]		@ load Rn value
		tst	r4, #1 << 23			@ U bit
		subne	r7, r0, r2
		addeq	r7, r0, r2
		b	Ldata_saver7

Ldata_lateldrpreconst:
		tst	r4, #1 << 21			@ check writeback bit
		beq	Ldata_simple
Ldata_lateldrpostconst:
		movs	r2, r4, lsl #20			@ Get offset
		beq	Ldata_simple
		and	r5, r4, #15 << 16		@ Get Rn
		ldr	r0, [sp, r5, lsr #14]
		tst	r4, #1 << 23			@ U bit
		subne	r7, r0, r2, lsr #20
		addeq	r7, r0, r2, lsr #20
		b	Ldata_saver7

Ldata_lateldrprereg:
		tst	r4, #1 << 21			@ check writeback bit
		beq	Ldata_simple
Ldata_lateldrpostreg:
		and	r5, r4, #15
		ldr	r2, [sp, r5, lsl #2]		@ Get Rm
		mov	r3, r4, lsr #7
		ands	r3, r3, #31
		and	r6, r4, #0x70
		orreq	r6, r6, #8
		add	pc, pc, r6
		mov	r0, r0

		mov	r2, r2, lsl r3			@ 0: LSL #!0
		b	1f
		b	1f				@ 1: LSL #0
		mov	r0, r0
		b	1f				@ 2: MUL?
		mov	r0, r0
		b	1f				@ 3: MUL?
		mov	r0, r0
		mov	r2, r2, lsr r3			@ 4: LSR #!0
		b	1f
		mov	r2, r2, lsr #32			@ 5: LSR #32
		b	1f
		b	1f				@ 6: MUL?
		mov	r0, r0
		b	1f				@ 7: MUL?
		mov	r0, r0
		mov	r2, r2, asr r3			@ 8: ASR #!0
		b	1f
		mov	r2, r2, asr #32			@ 9: ASR #32
		b	1f
		b	1f				@ A: MUL?
		mov	r0, r0
		b	1f				@ B: MUL?
		mov	r0, r0
		mov	r2, r2, ror r3			@ C: ROR #!0
		b	1f
		mov	r2, r2, rrx			@ D: RRX
		b	1f
		mov	r0, r0				@ E: MUL?
		mov	r0, r0
		mov	r0, r0				@ F: MUL?


1:		and	r5, r4, #15 << 16		@ Get Rn
		ldr	r0, [sp, r5, lsr #14]
		tst	r4, #1 << 23			@ U bit
		subne	r7, r0, r2
		addeq	r7, r0, r2
		b	Ldata_saver7

/*
 * Function: oki_ml67400x_check_bugs (void)
 *	   : oki_ml67400x_proc_init (void)
 *	   : oki_ml67400x_proc_fin (void)
 *
 * Notes   : This processor does not require these
 */
ENTRY(cpu_oki_ml67400x_check_bugs)
		mrs	ip, cpsr
		bic	ip, ip, #F_BIT
		msr	cpsr, ip
		mov	pc, lr

ENTRY(cpu_oki_ml67400x_proc_init)
		mov	pc, lr

ENTRY(cpu_oki_ml67400x_proc_fin)
		stmfd	sp!, {lr}
		ldmfd	sp!, {pc}

/*
 * Function: oki_ml67400x_proc_do_idle(void)
 * Params  : r0 = unused
 * Purpose : put the processer in proper idle mode
 */
ENTRY(cpu_oki_ml67400x_do_idle)
		mov	pc, lr

/*
 * Function: oki_ml67400x_set_pgd(unsigned long pgd_phys)
 * Params  : pgd_phys	Physical address of page table
 * Purpose : Perform a task switch, saving the old process' state and restoring
 *	     the new.
 */

ENTRY(cpu_oki_ml67400x_set_pgd)
	mov	pc, r14

/*
 * Function: oki_ml67400x_set_pmd ()
 *
 * Params  : r0 = Address to set
 *	   : r1 = value to set
 *
 * Purpose : Set a PMD and flush it out of any WB cache
 */
ENTRY(cpu_oki_ml67400x_set_pmd)
		tst	r1, #3
		orrne	r1, r1, #16			@ Updatable bit is
		str	r1, [r0]			@ always set on OKI_ML67X
		mov	pc, lr
	

/*
 * Function: oki_ml67400x_set_pte(pte_t *ptep, pte_t pte)
 * Params  : r0 = Address to set
 *	   : r1 = value to set
 * Purpose : Set a PTE and flush it out of any WB cache
 */
		.align	5
ENTRY(cpu_oki_ml67400x_set_pte)
		str	r1, [r0], #-1024		@ linux version

		eor	r1, r1, #LPTE_PRESENT | LPTE_YOUNG | LPTE_WRITE | LPTE_DIRTY

		bic	r2, r1, #0xff0
		bic	r2, r2, #3
		orr	r2, r2, #HPTE_TYPE_SMALL

		tst	r1, #LPTE_USER | LPTE_EXEC	@ User or Exec?
		orrne	r2, r2, #HPTE_AP_READ

		tst	r1, #LPTE_WRITE | LPTE_DIRTY	@ Write and Dirty?
		orreq	r2, r2, #HPTE_AP_WRITE

		tst	r1, #LPTE_PRESENT | LPTE_YOUNG	@ Present and Young
		movne	r2, #0

		str	r2, [r0]			@ hardware version
		mov	pc, lr

/*
 * Function: oki_ml67400x_reset
 * Params  : r0 = address to jump to
 * Notes   : This sets up everything for a reset
 */
ENTRY(cpu_oki_ml67400x_reset)
		mov	ip, #0

		mov	pc, r0

	
cpu_armvlsi_name:
		.asciz	"OKI"
cpu_oki_ml67400x_name:
		.asciz	"ML67400X"
		.align

		.section ".text.init", #alloc, #execinstr

__oki_ml67400x_setup:

@setup memory regions
@ 	
		mov	pc, lr				@ __ret (head-armv.S)

/*
 * Purpose : Function pointers used to access above functions - all calls
 *	     come through these
 */
		.type	oki_ml674001_processor_functions, #object
ENTRY(oki_ml67400x_processor_functions)
		.word	cpu_oki_ml67400x_data_abort
		.word	cpu_oki_ml67400x_check_bugs
		.word	cpu_oki_ml67400x_proc_init
		.word	cpu_oki_ml67400x_proc_fin
		.word	cpu_oki_ml67400x_reset
		.word	cpu_oki_ml67400x_do_idle

		/* cache */
		.word	cpu_oki_ml67400x_cache_clean_invalidate_all
		.word	cpu_oki_ml67400x_cache_clean_invalidate_range
		.word	cpu_oki_ml67400x_flush_ram_page

		/* dcache */
		.word	cpu_oki_ml67400x_dcache_invalidate_range
		.word	cpu_oki_ml67400x_dcache_clean_range
		.word	cpu_oki_ml67400x_dcache_clean_page
		.word	cpu_oki_ml67400x_dcache_clean_entry

		/* icache */
		.word	cpu_oki_ml67400x_icache_invalidate_range
		.word	cpu_oki_ml67400x_icache_invalidate_page

		/* tlb */
		.word	cpu_oki_ml67400x_tlb_invalidate_all
		.word	cpu_oki_ml67400x_tlb_invalidate_range
		.word	cpu_oki_ml67400x_tlb_invalidate_page

		/* pgtable */
		.word	cpu_oki_ml67400x_set_pgd
		.word	cpu_oki_ml67400x_set_pmd
		.word	cpu_oki_ml67400x_set_pte
		.size	oki_ml67400x_processor_functions, . - oki_ml67400x_processor_functions

		.type	cpu_oki_ml67400x_info, #object
cpu_oki_ml67400x_info:
		.long	cpu_armvlsi_name
		.long	cpu_oki_ml67400x_name
		.size	cpu_oki_ml67400x_info, . - cpu_oki_ml67400x_info
	
		.type	cpu_arch_name, #object
cpu_arch_name:	.asciz	"armv4"
		.size	cpu_arch_name, . - cpu_arch_name

		.type	cpu_elf_name, #object
cpu_elf_name:	.asciz	"v4"
		.size	cpu_elf_name, . - cpu_elf_name
		.align

/*
 * See /include/asm-arm for a definition of this structure.
 */
	
		.section ".proc.info", #alloc, #execinstr

		.type	__oki_ml67400x_proc_info, #object
__oki_ml67400x_proc_info:
		.long	0x00674000				@ cpu_val
		.long	0xfffffff0				@ cpu_mask
		.long	0x00000c1e				@ section_mmu_flags
		b	__oki_ml67400x_setup				@ cpu_flush
		.long	cpu_arch_name				@ arch_name
		.long	cpu_elf_name				@ elf_name
		.long	HWCAP_SWP | HWCAP_HALF | HWCAP_26BIT	@ elf_hwcap
		.long	cpu_oki_ml67400x_info				@ info
		.long	oki_ml67400x_processor_functions
		.size	__oki_ml67400x_proc_info, . - __oki_ml67400x_proc_info
