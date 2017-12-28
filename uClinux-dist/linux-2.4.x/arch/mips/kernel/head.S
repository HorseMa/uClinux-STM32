/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1994, 1995 Waldorf Electronics
 * Written by Ralf Baechle and Andreas Busse
 * Copyright (C) 1995 - 1999 Ralf Baechle
 * Copyright (C) 1996 Paul M. Antoine
 * Modified for DECStation and hence R3000 support by Paul M. Antoine
 * Further modifications by David S. Miller and Harald Koerfgen
 * Copyright (C) 1999 Silicon Graphics, Inc.
 *
 * Kevin Kissell, kevink@mips.com and Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 2000 MIPS Technologies, Inc.  All rights reserved.
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/threads.h>

#include <asm/asm.h>
#include <asm/current.h>
#include <asm/offset.h>
#include <asm/pgtable-bits.h>
#include <asm/processor.h>
#include <asm/regdef.h>
#include <asm/cachectl.h>
#include <asm/mipsregs.h>
#include <asm/stackframe.h>

		.text
		/*
		 * Reserved space for exception handlers.
		 * Necessary for machines which link their kernels at KSEG0.
		 */
		jal	kernel_entry
		.balign  0x400

		/* The following two symbols are used for kernel profiling. */
		EXPORT(stext)
		EXPORT(_stext)

		__INIT

		/* Cache Error */
		LEAF(except_vec2_generic)
		.set	noreorder
		.set	noat
		.set    mips0
		/*
		 * This is a very bad place to be.  Our cache error
		 * detection has triggered.  If we have write-back data
		 * in the cache, we may not be able to recover.  As a
		 * first-order desperate measure, turn off KSEG0 cacheing.
		 */
		mfc0	k0,CP0_CONFIG
		li	k1,~CONF_CM_CMASK
		and	k0,k0,k1
		ori	k0,k0,CONF_CM_UNCACHED
		mtc0	k0,CP0_CONFIG
		/* Give it a few cycles to sink in... */
		nop
		nop
		nop

		j	cache_parity_error
		 nop
		END(except_vec2_generic)

		.set	at

		/*
		 * Special interrupt vector for embedded MIPS.  This is a
		 * dedicated interrupt vector which reduces interrupt processing
		 * overhead.  The jump instruction will be inserted here at
		 * initialization time.  This handler may only be 8 bytes in
		 * size!
		 */
		NESTED(except_vec4, 0, sp)
1:		j	1b			/* Dummy, will be replaced */
		 nop
		END(except_vec4)

		/*
		 * EJTAG debug exception handler.
		 * The EJTAG debug exception entry point is 0xbfc00480, which
		 * normally is in the boot PROM, so the boot PROM must do a
		 * unconditional jump to this vector.
		 */
		NESTED(except_vec_ejtag_debug, 0, sp)
		j	ejtag_debug_handler
		 nop
		END(except_vec_ejtag_debug)

		__FINIT

		/*
		 * EJTAG debug exception handler.
		 */
		NESTED(ejtag_debug_handler, PT_SIZE, sp)
		.set	noat
		.set	noreorder
		mtc0	k0, CP0_DESAVE
		mfc0	k0, CP0_DEBUG

		sll	k0, k0, 30	# Check for SDBBP.
		bgez	k0, ejtag_return

		 la	k0, ejtag_debug_buffer
		sw	k1, 0(k0)
		SAVE_ALL
		jal	ejtag_exception_handler
		 move	a0, sp
		RESTORE_ALL
		la	k0, ejtag_debug_buffer
		lw	k1, 0(k0)

ejtag_return:
		mfc0	k0, CP0_DESAVE
		.set	mips32
		deret
		.set	mips0
		 nop
		.set	at
		END(ejtag_debug_handler)

		__INIT

		/*
		* NMI debug exception handler for MIPS reference boards.
		* The NMI debug exception entry point is 0xbfc00000, which
		* normally is in the boot PROM, so the boot PROM must do a
		* unconditional jump to this vector.
		*/
		NESTED(except_vec_nmi, 0, sp)
		j       nmi_handler
		 nop
		END(except_vec_nmi)

		__FINIT

		NESTED(nmi_handler, PT_SIZE, sp)
		.set    noat
		.set    noreorder
		.set    mips0
		SAVE_ALL
		jal     nmi_exception_handler
		 move   a0, sp
		RESTORE_ALL
		.set    mips3
		eret
		.set    at
		.set    mips0
		END(nmi_handler)

		__INIT

		/*
		 * Kernel entry point
		 */
		NESTED(kernel_entry, 16, sp)
		.set	noreorder

		/*
		 * The firmware/bootloader passes argc/argp/envp
		 * to us as arguments.  But clear bss first because
		 * the romvec and other important info is stored there
		 * by prom_init().
		 */
		mtc0	zero, CP0_STATUS
		nop
		la	t0, _edata
		sw	zero, (t0)
		la	t1, (_end - 4)
1:
		addiu	t0, 4
		bne	t0, t1, 1b
		 sw	zero, (t0)

		/*
		 * Stack for kernel and init, current variable
		 */
		la	$28, init_task_union
		addiu	t0, $28, KERNEL_STACK_SIZE-32
		subu	sp, t0, 4*SZREG
		sw	t0, kernelsp

		jal		_rom_flush_cache
		nop

		/*--- flush the write buffer*/
		jal		_rom_wbflush
		nop



		jal	init_arch
		 nop
		END(kernel_entry)
		
		.text

#ifdef CONFIG_RTL865XC
	LEAF(_rom_flush_cache)
	.set noreorder
	/*--- transition to kseg1 from undetermined kernel segment*/
	la	$9,2f
	or	$9,0xa0000000
	jr	$9

	.set	noreorder
	

2:	
	#--- initialize and start COP3
	mfc0	$8,$12
	nop
	nop
	or		$8,0x80000000
	mtc0	$8,$12
	nop
	nop
	
	#--- invalidate the IRAM with a 0->1 transition
	mtc0	$0, $20	# CCTL
	nop
	nop
	li		$8,0x00000020 # IRAM Off
	mtc0	$8, $20
	nop
	nop
	
	#--- invalidate the icache and dcache with a 0->1 transition
	mtc0	$0, $20	# CCTL
	nop
	nop
	li		$8,0x00000202 # Invalid ICACHE and DCACHE
	mtc0	$8, $20
	nop
	nop

	#--- load iram base and top
	la		$8,__iram
	la		$9,0x0fffc000
	and		$8,$8,$9
	mtc3	$8,$0								# IW bas
	nop
	nop
	addiu	$8,$8,0x3fff
	mtc3	$8,$1								# IW top
	nop
	nop

	#--- Refill the IRAM with a 0->1 transition
	mtc0	$0, $20	# CCTL
	nop
	nop
	li		$8,0x00000010 # IRAM Fill
	mtc0	$8, $20
	nop
	nop

	#--- load dram base and top
	la		$8,__dram_start
	la		$9,__dram_end
	beq		$8,$9,skip_dramInit
	la		$9,0x0fffe000
	and		$8,$8,$9
	mtc3	$8,$4								# DW bas
	nop
	nop
	addiu	$8,$8,0x1fff
	mtc3	$8,$5								# DW top
	nop
	nop

skip_dramInit:
	#--- enable icache and dcache
	mtc0	$0, $20	# CCTL
	nop
	nop

	.set	reorder
	j		$31


	END(_rom_flush_cache)
#endif /* CONFIG_RTL865XC */

#ifdef CONFIG_RTL865XB
		LEAF(_rom_flush_cache)
		.set noreorder
	/*--- transition to kseg1 from undetermined kernel segment*/
	la	$9,2f
	or	$9,0xa0000000
	jr	$9

	.set	noreorder
	

2:	
	#--- initialize and start COP3
	mfc0	$8,$12
	nop
	nop
	or		$8,0x80000000
	mtc0	$8,$12
	nop
	nop
	
	#--- invalidate the IRAM with a 0->1 transition
	mtc0	$0, $20	# CCTL
	nop
	nop
	li		$8,0x00000020 # IRAM Off
	mtc0	$8, $20
	nop
	nop
	
	#--- invalidate the icache and dcache with a 0->1 transition
	mtc0	$0, $20	# CCTL
	nop
	nop
	li		$8,0x00000003 # Invalid ICACHE and DCACHE
	mtc0	$8, $20
	nop
	nop

#if 1
	#--- load iram base and top
	la		$8,__iram
#	la		$8,0x0ffff000
	la		$9,0x0fffe000
	and		$8,$8,$9
	mtc3	$8,$0								# IW bas
	nop
	nop
	addiu	$8,$8,0x1fff
	mtc3	$8,$1								# IW top
	nop
	nop

	#--- Refill the IRAM with a 0->1 transition
	mtc0	$0, $20	# CCTL
	nop
	nop
	li		$8,0x00000010 # IRAM Fill
	mtc0	$8, $20
	nop
	nop

	#--- load dram base and top
	la		$8,__dram_start
	la		$9,__dram_end
	beq		$8,$9,skip_dramInit
#	la		$8,0x0ffff000
	la		$9,0x0ffff000
	and		$8,$8,$9
	mtc3	$8,$4								# DW bas
	nop
	nop
	addiu	$8,$8,0x0fff
	mtc3	$8,$5								# DW top
	nop
	nop
skip_dramInit:
#endif

	#--- enable icache and dcache
	mtc0	$0, $20	# CCTL
	nop
	nop

	.set	reorder
	j		$31


	END(_rom_flush_cache)
#endif /* CONFIG_RTL865XB */


	LEAF(_rom_wbflush)
_rom_wbflush:
	li		$8, 0xa0000000				#/* load uncached address	*/
	lw		$8, 0($8)					#/* read in order to flush 	*/
	j		$31							#/* return to caller		*/
	END(_rom_wbflush)


#if 0 // not needed DAVIDM
#/*-------------------------------------------------------------------
#**
#** void lx5280_getCCTL( uint32* pCCTL )
#**
#**-------------------------------------------------------------------
#*/
	.text
	.globl	lx5280_getCCTL
	.ent	lx5280_getCCTL
lx5280_getCCTL:
	.set	noreorder

	mfc0	$8, $20	# CCTL
	nop
	nop
	sw		$8, 0($4)
	nop

	jr		$31
	nop
	.set reorder
	.end lx5280_getCCTL

	
#/*-------------------------------------------------------------------
#**
#** void lx5280_getIram( void** pIW_base, void** pIW_top )
#**
#**		void**	pIW_base	address to store IW_base
#**		void** 	pIW_top		address to store IW_top
#**-------------------------------------------------------------------
#*/
	.text
	.globl	lx5280_getIram
	.ent	lx5280_getIram
lx5280_getIram:
	.set	noreorder

	#--- initialize and start COP3
	mfc0	$8,$12
	nop
	nop
	or		$8,0x80000000
	mtc0	$8,$12
	nop
	nop
	
	mfc3	$8,$0								# IW bas
	nop
	nop
	sw		$8, 0($4)
	nop
	
	mfc3	$8,$1								# IW top
	nop
	nop
	sw		$8, 0($5)
	nop
	
	jr		$31
	nop
	.set reorder
	.end lx5280_getIram

	
#/*-------------------------------------------------------------------
#**
#** void lx5280_setIram( void* IW_base, uint32 size )
#**
#**		void*	IW_base		1K-alignment address
#**		uint32	size		in bytes.
#**-------------------------------------------------------------------
#*/
	.text
	.globl	lx5280_setIram
	.ent	lx5280_setIram
lx5280_setIram:
	.set	noreorder

	
	#--- initialize and start COP3
	mfc0	$8,$12
	nop
	nop
	or		$8,0x80000000
	mtc0	$8,$12
	nop
	nop
	
	#--- invalidate the IRAM with a 0->1 transition
	mtc0	$0, $20	# CCTL
	nop
	nop
	li		$8,0x00000020 # IRAM Off
	mtc0	$8, $20
	nop
	nop
	
	#--- invalidate the icache with a 0->1 transition
	mtc0	$0, $20	# CCTL
	nop
	nop
   #ifdef CONFIG_RTL865XC
	li		$8,0x00000202 # Invalid ICACHE
   #endif
   #ifdef CONFIG_RTL865XB
	li		$8,0x00000003 # Invalid ICACHE
   #endif
	mtc0	$8, $20
	nop
	nop
	
	#--- load iram base and top
	move	$8,$4
	#ifdef CONFIG_RTL865XC
	la		$9,0x0fffc000
	#endif
	#ifdef CONFIG_RTL865XB
	la		$9,0x0fffe000
	#endif
	and		$8,$8,$9
	mtc3	$8,$0								# IW bas
	nop
	nop
	add		$8,$8,$5
	addiu	$8,$8,-1
	mtc3	$8,$1								# IW top
	nop
	nop
	
	#--- Refill the IRAM with a 0->1 transition
	mtc0	$0, $20	# CCTL
	nop
	nop
	li		$8,0x00000010 # IRAM Fill
	mtc0	$8, $20
	nop
	nop
	
	jr		$31
	nop
	.set reorder
	.end lx5280_setIram
	

#/*-------------------------------------------------------------------
#**
#** void lx5280_getDram( void** pDW_base, void** pDW_top )
#**
#**		void**	pDW_base	address to store DW_base
#**		void**	pDW_top		address to store DW_top
#**-------------------------------------------------------------------
#*/
	.text
	.globl	lx5280_getDram
	.ent	lx5280_getDram
lx5280_getDram:
	.set	noreorder

	#--- initialize and start COP3
	mfc0	$8,$12
	nop
	nop
	or		$8,0x80000000
	mtc0	$8,$12
	nop
	nop
	
	mfc3	$8,$4								# DW bas
	nop
	nop
	sw		$8, 0($4)
	nop
	
	mfc3	$8,$5								# DW top
	nop
	nop
	sw		$8, 0($5)
	nop
	
	jr		$31
	nop
	.set reorder
	.end lx5280_getDram

	
#/*-------------------------------------------------------------------
#**
#** void lx5280_setDram( void* IW_base, uint32 size )
#**
#**		void*	DW_base		1K-alignment address
#**		uint32	size		in bytes.
#**-------------------------------------------------------------------
#*/
	.text
	.globl	lx5280_setDram
	.ent	lx5280_setDram
lx5280_setDram:
	.set	noreorder

	
	#--- invalidate the icache and dcache with a 0->1 transition
	mtc0	$0, $20	# CCTL
	nop
	nop
	#ifdef CONFIG_RTL865XC
	li		$8,0x00000202
	#endif
	#ifdef CONFIG_RTL865XB
	li		$8,0x00000003
	#endif
	mtc0	$8, $20
	nop
	nop
	
	#--- initialize and start COP3
	mfc0	$8,$12
	nop
	nop
	or		$8,0x80000000
	mtc0	$8,$12
	nop
	nop
	
	#--- load dram base and top
	move	$8,$4
	#ifdef CONFIG_RTL865XC
	la		$9,0x0fffe000
	#endif
	#ifdef CONFIG_RTL865XB
	la		$9,0x0ffff000
	#endif
	and		$8,$8,$9
	mtc3	$8,$4								# DW bas
	nop
	nop
	add		$8,$8,$5
	addiu	$8,$8,-1
	mtc3	$8,$5								# DW top
	nop
	nop
	
	#--- enable icache and dcache
	mtc0	$0, $20	# CCTL
	nop
	nop
	
	jr		$31
	nop
	.set reorder
	.end lx5280_setDram
	
	

#if 0
#define DONT_PUSH_LEXRA_REGS_INTO_STACK
#ifdef DONT_PUSH_LEXRA_REGS_INTO_STACK
	.data
context_m0h_upper:
	.word	0
context_m0h_lower:
	.word	0
context_m0l_upper:
	.word	0
context_m0l_lower:
	.word	0
context_m3h_upper:
	.word	0
context_m3h_lower:
	.word	0
context_m3l_upper:
	.word	0
context_m3l_lower:
	.word	0
#endif
	
	.text
	.globl	lexra_saveRadiaxRegs
	.ent	lexra_saveRadiaxRegs
lexra_saveRadiaxRegs:
	.set	noreorder
#ifndef DONT_PUSH_LEXRA_REGS_INTO_STACK
	mfa k0, m0h
	nop
	nop
	nop
	sw	k0, PT_RADIAX_M0H_L(sp)
	mfa	k1, m0h, 8
	nop
	nop
	nop
	sw	k1, PT_RADIAX_M0H_U(sp)
	mfa k0, m0l
	nop
	nop
	nop
	sw	k0, PT_RADIAX_M0L_L(sp)
	mfa	k1, m0l, 8  
	nop
	nop
	nop
	sw	k1, PT_RADIAX_M0L_U(sp)
	#
	mfa k0, m3h
	nop
	nop
	nop
	sw	k0, PT_RADIAX_M3H_L(sp)
	mfa	k1, m3h, 8
	nop
	nop
	nop
	sw	k1, PT_RADIAX_M3H_U(sp)
	mfa k0, m3l
	nop
	nop
	nop
	sw	k0, PT_RADIAX_M3L_L(sp)
	mfa	k1, m3l, 8  
	nop
	nop
	nop
	sw	k1, PT_RADIAX_M3L_U(sp)
#else
		mfa k0, m0h
		nop
		nop
		sw	k0, context_m0h_lower
		mfa	k1, m0h, 8
		nop
		nop
		sw	k1, context_m0h_upper
		mfa k0, m0l
		nop
		nop
		sw	k0, context_m0l_lower
		mfa	k1, m0l, 8
		nop
		nop
		sw	k1, context_m0l_upper
		#
		mfa k0, m3h
		nop
		nop
		sw	k0, context_m3h_lower
		mfa	k1, m3h, 8
		nop
		nop
		sw	k1, context_m3h_upper
		mfa k0, m3l
		nop
		nop
		sw	k0, context_m3l_lower
		mfa	k1, m3l, 8
		nop
		nop
		sw	k1, context_m3l_upper
#endif //DONT_PUSH_LEXRA_REGS_INTO_STACK
	jr	ra
	nop
	.set reorder
	.end lexra_saveRadiaxRegs
	
	
	
	.text
	.globl	lexra_restoreRadiaxRegs
	.ent	lexra_restoreRadiaxRegs
lexra_restoreRadiaxRegs:
	.set	noreorder
#ifndef DONT_PUSH_LEXRA_REGS_INTO_STACK
	lw	k0, PT_RADIAX_M0H_L(sp)
	mta2	k0, m0h
	nop
	nop
	nop
	lw	k0, PT_RADIAX_M0L_L(sp)
	mta2	k0, m0l
	nop
	nop
	nop
	lw	k0, PT_RADIAX_M0H_U(sp)
	mta2.G	k0, m0h
	nop
	nop
	nop
	lw	k0, PT_RADIAX_M0L_U(sp)
	mta2.G	k0, m0l
	nop
	nop
	nop
	#
	lw	k0, PT_RADIAX_M3H_L(sp)
	mta2	k0, m3h
	nop
	nop
	nop
	lw	k0, PT_RADIAX_M3L_L(sp)
	mta2	k0, m3l
	nop
	nop
	nop
	lw	k0, PT_RADIAX_M3H_U(sp)
	mta2.G	k0, m3h
	nop
	nop
	nop
	lw	k0, PT_RADIAX_M3L_U(sp)
	mta2.G	k0, m3l
#else
		lw	k0, context_m0h_lower
		mta2	k0, m0h
		nop
		nop
		lw	k1, context_m0l_lower
		mta2	k1, m0l
		nop
		nop
		lw	k0, context_m0h_upper
		mta2.G	k0, m0h
		nop
		nop
		lw	k1, context_m0l_upper
		mta2.G	k1, m0l
		nop
		nop
		#
		lw	k0, context_m3h_lower
		mta2	k0, m3h
		nop
		nop
		lw	k1, context_m3l_lower
		mta2	k1, m3l
		nop
		nop
		lw	k0, context_m3h_upper
		mta2.G	k0, m3h
		nop
		nop
		lw	k1, context_m3l_upper
		mta2.G	k1, m3l
		nop
		nop
#endif
	jr	ra
	nop
	.set reorder
	.end lexra_restoreRadiaxRegs
#endif
#endif
	



#ifdef CONFIG_SMP

/*
 * SMP slave cpus entry point.  Board specific code for bootstrap calls this
 * function after setting up the stack and gp registers.
 */
		LEAF(smp_bootstrap)
		.set push
		.set noreorder
		mtc0	zero, CP0_WIRED
		CLI
		mfc0	t0, CP0_STATUS
		li	t1, ~(ST0_CU1|ST0_CU2|ST0_CU3|ST0_KX|ST0_SX|ST0_UX)
		and	t0, t1
		or	t0, (ST0_CU0);
		jal	start_secondary
		 mtc0	t0, CP0_STATUS
		.set pop
		END(smp_bootstrap)
#endif

		__FINIT

		/*
		 * This buffer is reserved for the use of the EJTAG debug
		 * handler.
		 */
		.data
		EXPORT(ejtag_debug_buffer)
		.fill	4

		.comm	kernelsp,    NR_CPUS * 8, 8
		.comm	pgd_current, NR_CPUS * 8, 8

	.macro	page name, order=0
	.globl	\name
\name:	.size	\name, (_PAGE_SIZE << \order)
	.org	. + (_PAGE_SIZE << \order)
	.type	\name, @object
	.endm

	.data
	.align	12

	page	swapper_pg_dir, _PGD_ORDER
	page	empty_bad_page, 0
	page	empty_bad_page_table, 0
	page	invalid_pte_table, 0
