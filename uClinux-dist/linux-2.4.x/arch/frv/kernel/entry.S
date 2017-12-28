/* entry.S: FR-V entry
 *
 * Copyright (C) 2003, 2004 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 *
 * Entry to the kernel is "interesting":
 *  (1) There are no stack pointers, not even for the kernel
 *  (2) General Registers should not be clobbered
 *  (3) There are no kernel-only data registers
 *  (4) Since all addressing modes are wrt to a General Register, no global
 *      variables can be reached
 *
 * We deal with this by declaring that we shall kill GR28 on entering the
 * kernel from userspace
 *
 * However, since break interrupts can interrupt the CPU even when PSR.ET==0,
 * they can't rely on GR28 to be anything useful, and so need to clobber a
 * separate register (GR31). Break interrupts are managed in break.S
 *
 * GR29 _is_ saved, and holds the current task pointer globally
 *
 */

#include <linux/sys.h>
#include <linux/config.h>
#include <linux/linkage.h>
#include <asm/setup.h>
#include <asm/segment.h>
#include <asm/ptrace.h>
#include <asm/errno.h>
#include <asm/cache.h>
#include <asm/spr-regs.h>

sigpending	=	0x00000008
need_resched	=	0x00000014
tsk_ptrace	=	0x00000018

#define PT_TRACESYS	0x00000002

        .file		"entry.S"
	.text
	.balign		4

.macro LEDS val
#	sethi.p		%hi(0xe1200004),gr30
#	setlo		%lo(0xe1200004),gr30
#	setlos		#~\val,gr31
#	st		gr31,@(gr30,gr0)
#	membar
.endm

###############################################################################
#
# entry point for External interrupts received whilst executing userspace code
#
###############################################################################
	.globl		__entry_uspace_external_interrupt
__entry_uspace_external_interrupt:
	LEDS		0x6200
	sethi.p		%hi(__kernel_frame0_ptr),gr28
	setlo		%lo(__kernel_frame0_ptr),gr28
	ldi		@(gr28,#0),gr28

	# handle h/w single-step through exceptions
	sti		gr0,@(gr28,#REG__STATUS)

	.globl		__entry_uspace_external_interrupt_reentry
__entry_uspace_external_interrupt_reentry:
	LEDS		0x6201

	setlos		#REG__END,gr30
	dcpl		gr28,gr30,#0

	# finish building the exception frame
	sti		sp,  @(gr28,#REG_SP)
	stdi		gr2, @(gr28,#REG_GR(2))
	stdi		gr4, @(gr28,#REG_GR(4))
	stdi		gr6, @(gr28,#REG_GR(6))
	stdi		gr8, @(gr28,#REG_GR(8))
	stdi		gr10,@(gr28,#REG_GR(10))
	stdi		gr12,@(gr28,#REG_GR(12))
	stdi		gr14,@(gr28,#REG_GR(14))
	stdi		gr16,@(gr28,#REG_GR(16))
	stdi		gr18,@(gr28,#REG_GR(18))
	stdi		gr20,@(gr28,#REG_GR(20))
	stdi		gr22,@(gr28,#REG_GR(22))
	stdi		gr24,@(gr28,#REG_GR(24))
	stdi		gr26,@(gr28,#REG_GR(26))
	sti		gr0, @(gr28,#REG_GR(28))
	sti		gr29,@(gr28,#REG_GR(29))
	stdi.p		gr30,@(gr28,#REG_GR(30))

	# set up the kernel stack pointer
	ori		gr28,0,sp

	movsg		tbr ,gr20
	movsg		psr ,gr22
	movsg		pcsr,gr21
	movsg		isr ,gr23
	movsg		ccr ,gr24
	movsg		cccr,gr25
	movsg		lr  ,gr26
	movsg		lcr ,gr27

	setlos.p	#-1,gr4
	andi		gr22,#PSR_PS,gr5		/* try to rebuild original PSR value */
	andi.p		gr22,#~(PSR_PS|PSR_S),gr6
	slli		gr5,#1,gr5
	or		gr6,gr5,gr5
	andi		gr5,#~PSR_ET,gr5

	sti		gr20,@(gr28,#REG_TBR)
	sti		gr21,@(gr28,#REG_PC)
	sti		gr5 ,@(gr28,#REG_PSR)
	sti		gr23,@(gr28,#REG_ISR)
	stdi		gr24,@(gr28,#REG_CCR)
	stdi		gr26,@(gr28,#REG_LR)
	sti		gr4 ,@(gr28,#REG_SYSCALLNO)

	movsg		iacc0h,gr4
	movsg		iacc0l,gr5
	stdi		gr4,@(gr28,#REG_IACC0)

	movsg		gner0,gr4
	movsg		gner1,gr5
	stdi		gr4,@(gr28,#REG_GNER0)

	# set up kernel global registers
	sethi.p		%hi(__kernel_current_task),gr5
	setlo		%lo(__kernel_current_task),gr5
	sethi.p		%hi(_gp),gr16
	setlo		%lo(_gp),gr16
	ldi.p		@(gr5,#0),gr29

	# make sure we (the kernel) get div-zero and misalignment exceptions
	setlos		#ISR_EDE|ISR_DTT_DIVBYZERO|ISR_EMAM_EXCEPTION,gr5
	movgs		gr5,isr

	# switch to the kernel trap table
	sethi.p		%hi(__entry_kerneltrap_table),gr6
	setlo		%lo(__entry_kerneltrap_table),gr6
	movgs		gr6,tbr

	# set the return address
	sethi.p		%hi(__entry_return_from_exception),gr4
	setlo		%lo(__entry_return_from_exception),gr4
	movgs		gr4,lr

	# raise the minimum interrupt priority to 15 (NMI only) and enable exceptions
	movsg		psr,gr4

	ori		gr4,#PSR_PIL,gr4
	movgs		gr4,psr
	ori		gr4,#PSR_PIL|PSR_ET,gr4
	movgs		gr4,psr

	LEDS		0x6202
	bra		do_IRQ


###############################################################################
#
# entry point for External interrupts received whilst executing kernel code
# - on arriving here, the following registers should already be set up:
#	GR16	- kernel GP-REL pointer
#	GR29	- current task struct pointer
#	TBR	- kernel trap vector table
#	ISR	- kernel's preferred integer controls
#
###############################################################################
	.globl		__entry_kernel_external_interrupt
__entry_kernel_external_interrupt:
	LEDS		0x6210

	# set up the stack pointer
	or.p		sp,gr0,gr30
	subi		sp,#REG__END,sp
	sti		gr30,@(sp,#REG_SP)

	# handle h/w single-step through exceptions
	sti		gr0,@(sp,#REG__STATUS)

	.globl		__entry_kernel_external_interrupt_reentry
__entry_kernel_external_interrupt_reentry:
	LEDS		0x6211

	# set up the exception frame
	setlos		#REG__END,gr30
	dcpl		sp,gr30,#0

	sti.p		gr28,@(sp,#REG_GR(28))
	ori		sp,0,gr28

	# finish building the exception frame
	stdi		gr2,@(gr28,#REG_GR(2))
	stdi		gr4,@(gr28,#REG_GR(4))
	stdi		gr6,@(gr28,#REG_GR(6))
	stdi		gr8,@(gr28,#REG_GR(8))
	stdi		gr10,@(gr28,#REG_GR(10))
	stdi		gr12,@(gr28,#REG_GR(12))
	stdi		gr14,@(gr28,#REG_GR(14))
	stdi		gr16,@(gr28,#REG_GR(16))
	stdi		gr18,@(gr28,#REG_GR(18))
	stdi		gr20,@(gr28,#REG_GR(20))
	stdi		gr22,@(gr28,#REG_GR(22))
	stdi		gr24,@(gr28,#REG_GR(24))
	stdi		gr26,@(gr28,#REG_GR(26))
	sti		gr29,@(gr28,#REG_GR(29))
	stdi		gr30,@(gr28,#REG_GR(30))

	movsg		tbr ,gr20
	movsg		psr ,gr22
	movsg		pcsr,gr21
	movsg		isr ,gr23
	movsg		ccr ,gr24
	movsg		cccr,gr25
	movsg		lr  ,gr26
	movsg		lcr ,gr27

	setlos.p	#-1,gr4
	andi		gr22,#PSR_PS,gr5		/* try to rebuild original PSR value */
	andi.p		gr22,#~(PSR_PS|PSR_S),gr6
	slli		gr5,#1,gr5
	or		gr6,gr5,gr5
	andi.p		gr5,#~PSR_ET,gr5

	# set CCCR.CC3 to Undefined to abort atomic-modify completion inside the kernel
	# - for an explanation of how it works, see: Documentation/fujitsu/frv/atomic-ops.txt
	andi		gr25,#~0xc0,gr25

	sti		gr20,@(gr28,#REG_TBR)
	sti		gr21,@(gr28,#REG_PC)
	sti		gr5 ,@(gr28,#REG_PSR)
	sti		gr23,@(gr28,#REG_ISR)
	stdi		gr24,@(gr28,#REG_CCR)
	stdi		gr26,@(gr28,#REG_LR)
	sti		gr4 ,@(gr28,#REG_SYSCALLNO)

	movsg		iacc0h,gr4
	movsg		iacc0l,gr5
	stdi		gr4,@(gr28,#REG_IACC0)

	movsg		gner0,gr4
	movsg		gner1,gr5
	stdi		gr4,@(gr28,#REG_GNER0)

	# set the return address
	sethi.p		%hi(__entry_return_from_exception),gr4
	setlo		%lo(__entry_return_from_exception),gr4
	movgs		gr4,lr

	# clear power-saving mode flags
	movsg		hsr0,gr4
	andi		gr4,#~HSR0_PDM,gr4
	movgs		gr4,hsr0

	# raise the minimum interrupt priority to 15 (NMI only) and enable exceptions
	movsg		psr,gr4
	ori		gr4,#PSR_PIL,gr4
	movgs		gr4,psr
	ori		gr4,#PSR_ET,gr4
	movgs		gr4,psr

	LEDS		0x6212
	bra		do_IRQ


###############################################################################
#
# entry point for Software and Progam interrupts generated whilst executing userspace code
#
###############################################################################
	.globl		__entry_uspace_softprog_interrupt
	.globl		__entry_uspace_handle_mmu_fault
__entry_uspace_softprog_interrupt:
	LEDS		0x6000
#ifndef CONFIG_UCLINUX
	movsg		ear0,gr28
__entry_uspace_handle_mmu_fault:
	movgs		gr28,scr2
#endif
	sethi.p		%hi(__kernel_frame0_ptr),gr28
	setlo		%lo(__kernel_frame0_ptr),gr28
	ldi		@(gr28,#0),gr28

	# handle h/w single-step through exceptions
	sti		gr0,@(gr28,#REG__STATUS)

	.globl		__entry_uspace_softprog_interrupt_reentry
__entry_uspace_softprog_interrupt_reentry:
	LEDS		0x6001

	setlos		#REG__END,gr30
	dcpl		gr28,gr30,#0

	# set up the kernel stack pointer
	sti.p		sp,@(gr28,#REG_SP)
	ori		gr28,0,sp
	sti		gr0,@(gr28,#REG_GR(28))

	stdi		gr20,@(gr28,#REG_GR(20))
	stdi		gr22,@(gr28,#REG_GR(22))

	movsg		tbr,gr20
	movsg		pcsr,gr21
	movsg		psr,gr22
	bra		__entry_common

	# single-stepping was disabled on entry to a TLB handler that then faulted
#ifndef CONFIG_UCLINUX
	.globl		__entry_uspace_handle_mmu_fault_sstep
__entry_uspace_handle_mmu_fault_sstep:
	movgs		gr28,scr2
	sethi.p		%hi(__kernel_frame0_ptr),gr28
	setlo		%lo(__kernel_frame0_ptr),gr28
	ldi		@(gr28,#0),gr28

	# flag single-step re-enablement
	sti		gr0,@(gr28,#REG__STATUS)
	bra		__entry_uspace_softprog_interrupt_reentry
#endif


###############################################################################
#
# entry point for Software and Progam interrupts generated whilst executing kernel code
#
###############################################################################
	.globl		__entry_kernel_softprog_interrupt
__entry_kernel_softprog_interrupt:
	LEDS		0x6004

#ifndef CONFIG_UCLINUX
	movsg		ear0,gr30
	movgs		gr30,scr2
#endif

	.globl		__entry_kernel_handle_mmu_fault
__entry_kernel_handle_mmu_fault:
	# set up the stack pointer
	subi		sp,#REG__END,sp
	sti		sp,@(sp,#REG_SP)
	sti		sp,@(sp,#REG_SP-4)
	andi		sp,#~7,sp

	# handle h/w single-step through exceptions
	sti		gr0,@(sp,#REG__STATUS)

	.globl		__entry_kernel_softprog_interrupt_reentry
__entry_kernel_softprog_interrupt_reentry:
	LEDS		0x6005

	setlos		#REG__END,gr30
	dcpl		sp,gr30,#0

	# set up the exception frame
	sti.p		gr28,@(sp,#REG_GR(28))
	ori		sp,0,gr28

	stdi		gr20,@(gr28,#REG_GR(20))
	stdi		gr22,@(gr28,#REG_GR(22))

	ldi		@(sp,#REG_SP),gr22		/* reconstruct the old SP */
	addi		gr22,#REG__END,gr22
	sti		gr22,@(sp,#REG_SP)

	# set CCCR.CC3 to Undefined to abort atomic-modify completion inside the kernel
	# - for an explanation of how it works, see: Documentation/fujitsu/frv/atomic-ops.txt
	movsg		cccr,gr20
	andi		gr20,#~0xc0,gr20
	movgs		gr20,cccr

	movsg		tbr,gr20
	movsg		pcsr,gr21
	movsg		psr,gr22

	bra		__entry_common

	# single-stepping was disabled on entry to a TLB handler that then faulted
#ifndef CONFIG_UCLINUX
	.globl		__entry_kernel_handle_mmu_fault_sstep
__entry_kernel_handle_mmu_fault_sstep:
	# set up the stack pointer
	subi		sp,#REG__END,sp
	sti		sp,@(sp,#REG_SP)
	sti		sp,@(sp,#REG_SP-4)
	andi		sp,#~7,sp

	# flag single-step re-enablement
	sethi		#REG__STATUS_STEP,gr30
	sti		gr30,@(sp,#REG__STATUS)
	bra		__entry_kernel_softprog_interrupt_reentry
#endif


###############################################################################
#
# the rest of the kernel entry point code
# - on arriving here, the following registers should be set up:
#	GR1	- kernel stack pointer
#	GR7	- syscall number (trap 0 only)
#	GR8-13	- syscall args (trap 0 only)
#	GR20	- saved TBR
#	GR21	- saved PC
#	GR22	- saved PSR
#	GR28	- exception frame on stack
#	SCR2	- saved EAR0 where applicable (clobbered by ICI & ICEF insns on FR451)
#	PSR	- PSR.S 1, PSR.ET 0
#
###############################################################################
	.globl		__entry_common
__entry_common:
	LEDS		0x6008

	# finish building the exception frame
	stdi		gr2,@(gr28,#REG_GR(2))
	stdi		gr4,@(gr28,#REG_GR(4))
	stdi		gr6,@(gr28,#REG_GR(6))
	stdi		gr8,@(gr28,#REG_GR(8))
	stdi		gr10,@(gr28,#REG_GR(10))
	stdi		gr12,@(gr28,#REG_GR(12))
	stdi		gr14,@(gr28,#REG_GR(14))
	stdi		gr16,@(gr28,#REG_GR(16))
	stdi		gr18,@(gr28,#REG_GR(18))
	stdi		gr24,@(gr28,#REG_GR(24))
	stdi		gr26,@(gr28,#REG_GR(26))
	sti		gr29,@(gr28,#REG_GR(29))
	stdi		gr30,@(gr28,#REG_GR(30))

	movsg		isr ,gr23
	movsg		ccr ,gr24
	movsg		cccr,gr25
	movsg		lr  ,gr26
	movsg		lcr ,gr27

	setlos.p	#-1,gr4
	andi		gr22,#PSR_PS,gr5		/* try to rebuild original PSR value */
	andi.p		gr22,#~(PSR_PS|PSR_S),gr6
	slli		gr5,#1,gr5
	or		gr6,gr5,gr5
	andi		gr5,#~PSR_ET,gr5

	sti		gr20,@(gr28,#REG_TBR)
	sti		gr21,@(gr28,#REG_PC)
	sti		gr5 ,@(gr28,#REG_PSR)
	sti		gr23,@(gr28,#REG_ISR)
	stdi		gr24,@(gr28,#REG_CCR)
	stdi		gr26,@(gr28,#REG_LR)
	sti		gr4 ,@(gr28,#REG_SYSCALLNO)

	movsg		iacc0h,gr4
	movsg		iacc0l,gr5
	stdi		gr4,@(gr28,#REG_IACC0)

	movsg		gner0,gr4
	movsg		gner1,gr5
	stdi		gr4,@(gr28,#REG_GNER0)

	# set up kernel global registers
	sethi.p		%hi(__kernel_current_task),gr5
	setlo		%lo(__kernel_current_task),gr5
	sethi.p		%hi(_gp),gr16
	setlo		%lo(_gp),gr16
	ldi		@(gr5,#0),gr29

	# switch to the kernel trap table
	sethi.p		%hi(__entry_kerneltrap_table),gr6
	setlo		%lo(__entry_kerneltrap_table),gr6
	movgs		gr6,tbr

	# make sure we (the kernel) get div-zero and misalignment exceptions
	setlos		#ISR_EDE|ISR_DTT_DIVBYZERO|ISR_EMAM_EXCEPTION,gr5
	movgs		gr5,isr

	# clear power-saving mode flags
	movsg		hsr0,gr4
	andi		gr4,#~HSR0_PDM,gr4
	movgs		gr4,hsr0

	# multiplex again using old TBR as a guide
	setlos.p	#TBR_TT,gr3
	sethi		%hi(__entry_vector_table),gr6
	and.p		gr20,gr3,gr5
	setlo		%lo(__entry_vector_table),gr6
	srli.p		gr5,#2,gr5
	sethi		%hi(__entry_return_from_exception),gr4
	ld.p		@(gr5,gr6),gr5
	setlo		%lo(__entry_return_from_exception),gr4
	movgs		gr4,lr

	LEDS		0x6009
	jmpl		@(gr5,gr0)


###############################################################################
#
# handle instruction MMU fault
#
###############################################################################
#ifndef CONFIG_UCLINUX
	.globl		__entry_insn_mmu_fault
__entry_insn_mmu_fault:
	LEDS		0x6010
	setlos		#0,gr8
	movsg		esr0,gr9
	movsg		scr2,gr10

	# now that we've accessed the exception regs, we can enable exceptions
	movsg		psr,gr4
	ori		gr4,#PSR_ET,gr4
	movgs		gr4,psr

	sethi.p		%hi(do_page_fault),gr5
	setlo		%lo(do_page_fault),gr5
	jmpl		@(gr5,gr0)	; call do_page_fault(0,esr0,ear0)
#endif

###############################################################################
#
# handle instruction access error
#
###############################################################################
	.globl		__entry_insn_access_error
__entry_insn_access_error:
	LEDS		0x6011
	sethi.p		%hi(insn_access_error),gr5
	setlo		%lo(insn_access_error),gr5
	movsg		esfr1,gr8
	movsg		epcr0,gr9
	movsg		esr0,gr10

	# now that we've accessed the exception regs, we can enable exceptions
	movsg		psr,gr4
	ori		gr4,#PSR_ET,gr4
	movgs		gr4,psr
	jmpl		@(gr5,gr0)	; call insn_access_error(esfr1,epcr0,esr0)

###############################################################################
#
# handle various instructions of dubious legality
#
###############################################################################
	.globl		__entry_unsupported_trap
	.globl		__entry_illegal_instruction
	.globl		__entry_privileged_instruction
	.globl		__entry_debug_exception
__entry_unsupported_trap:
	subi		gr21,#4,gr21
	sti		gr21,@(gr28,#REG_PC)
__entry_illegal_instruction:
__entry_privileged_instruction:
__entry_debug_exception:
	LEDS		0x6012
	sethi.p		%hi(illegal_instruction),gr5
	setlo		%lo(illegal_instruction),gr5
	movsg		esfr1,gr8
	movsg		epcr0,gr9
	movsg		esr0,gr10

	# now that we've accessed the exception regs, we can enable exceptions
	movsg		psr,gr4
	ori		gr4,#PSR_ET,gr4
	movgs		gr4,psr
	jmpl		@(gr5,gr0)	; call ill_insn(esfr1,epcr0,esr0)

###############################################################################
#
# handle media exception
#
###############################################################################
	.globl		__entry_media_exception
__entry_media_exception:
	LEDS		0x6013
	sethi.p		%hi(media_exception),gr5
	setlo		%lo(media_exception),gr5
	movsg		msr0,gr8
	movsg		msr1,gr9

	# now that we've accessed the exception regs, we can enable exceptions
	movsg		psr,gr4
	ori		gr4,#PSR_ET,gr4
	movgs		gr4,psr
	jmpl		@(gr5,gr0)	; call media_excep(msr0,msr1)

###############################################################################
#
# handle data MMU fault
# handle data DAT fault (write-protect exception)
#
###############################################################################
#ifndef CONFIG_UCLINUX
	.globl		__entry_data_mmu_fault
__entry_data_mmu_fault:
	.globl		__entry_data_dat_fault
__entry_data_dat_fault:
	LEDS		0x6014
	setlos		#1,gr8
	movsg		esr0,gr9
	movsg		scr2,gr10	; saved EAR0

	# now that we've accessed the exception regs, we can enable exceptions
	movsg		psr,gr4
	ori		gr4,#PSR_ET,gr4
	movgs		gr4,psr

	sethi.p		%hi(do_page_fault),gr5
	setlo		%lo(do_page_fault),gr5
	jmpl		@(gr5,gr0)	; call do_page_fault(1,esr0,ear0)
#endif

###############################################################################
#
# handle data and instruction access exceptions
#
###############################################################################
	.globl		__entry_insn_access_exception
	.globl		__entry_data_access_exception
__entry_insn_access_exception:
__entry_data_access_exception:
	LEDS		0x6016
	sethi.p		%hi(memory_access_exception),gr5
	setlo		%lo(memory_access_exception),gr5
	movsg		esr0,gr8
	movsg		scr2,gr9	; saved EAR0
	movsg		epcr0,gr10

	# now that we've accessed the exception regs, we can enable exceptions
	movsg		psr,gr4
	ori		gr4,#PSR_ET,gr4
	movgs		gr4,psr
	jmpl		@(gr5,gr0)	; call memory_access_error(esr0,ear0,epcr0)

###############################################################################
#
# handle data access error
#
###############################################################################
	.globl		__entry_data_access_error
__entry_data_access_error:
	LEDS		0x6016
	sethi.p		%hi(data_access_error),gr5
	setlo		%lo(data_access_error),gr5
	movsg		esfr1,gr8
	movsg		esr15,gr9
	movsg		ear15,gr10

	# now that we've accessed the exception regs, we can enable exceptions
	movsg		psr,gr4
	ori		gr4,#PSR_ET,gr4
	movgs		gr4,psr
	jmpl		@(gr5,gr0)	; call data_access_error(esfr1,esr15,ear15)

###############################################################################
#
# handle data store error
#
###############################################################################
	.globl		__entry_data_store_error
__entry_data_store_error:
	LEDS		0x6017
	sethi.p		%hi(data_store_error),gr5
	setlo		%lo(data_store_error),gr5
	movsg		esfr1,gr8
	movsg		esr14,gr9

	# now that we've accessed the exception regs, we can enable exceptions
	movsg		psr,gr4
	ori		gr4,#PSR_ET,gr4
	movgs		gr4,psr
	jmpl		@(gr5,gr0)	; call data_store_error(esfr1,esr14)

###############################################################################
#
# handle division exception
#
###############################################################################
	.globl		__entry_division_exception
__entry_division_exception:
	LEDS		0x6018
	sethi.p		%hi(division_exception),gr5
	setlo		%lo(division_exception),gr5
	movsg		esfr1,gr8
	movsg		esr0,gr9
	movsg		isr,gr10

	# now that we've accessed the exception regs, we can enable exceptions
	movsg		psr,gr4
	ori		gr4,#PSR_ET,gr4
	movgs		gr4,psr
	jmpl		@(gr5,gr0)	; call div_excep(esfr1,esr0,isr)

###############################################################################
#
# handle compound exception
#
###############################################################################
	.globl		__entry_compound_exception
__entry_compound_exception:
	LEDS		0x6019
	sethi.p		%hi(compound_exception),gr5
	setlo		%lo(compound_exception),gr5
	movsg		esfr1,gr8
	movsg		esr0,gr9
	movsg		esr14,gr10
	movsg		esr15,gr11
	movsg		msr0,gr12
	movsg		msr1,gr13

	# now that we've accessed the exception regs, we can enable exceptions
	movsg		psr,gr4
	ori		gr4,#PSR_ET,gr4
	movgs		gr4,psr
	jmpl		@(gr5,gr0)	; call comp_excep(esfr1,esr0,esr14,esr15,msr0,msr1)

###############################################################################
#
# handle interrupts and NMIs
#
###############################################################################
	.globl		__entry_do_IRQ
__entry_do_IRQ:
	LEDS		0x6020

	# we can enable exceptions
	movsg		psr,gr4
	ori		gr4,#PSR_ET,gr4
	movgs		gr4,psr
	bra		do_IRQ

	.globl		__entry_do_NMI
__entry_do_NMI:
	LEDS		0x6021

	# we can enable exceptions
	movsg		psr,gr4
	ori		gr4,#PSR_ET,gr4
	movgs		gr4,psr
	bra		do_NMI

###############################################################################
#
# the return path for a newly forked child process
# - __switch_to() saved the old current pointer in GR8 for us
#
###############################################################################
	.globl		ret_from_fork
ret_from_fork:
	LEDS		0x6100
	call		SYMBOL_NAME(schedule_tail)

	# rebuild saved psr
	ldi		@(gr28,#REG_PSR),gr22
	srli		gr22,#1,gr5
	andi.p		gr22,#~PSR_PS,gr22
	andi		gr5,#PSR_PS,gr5
	or		gr5,gr22,gr22
	ori		gr22,#PSR_S,gr22

	# fork & co. return 0 to child
	setlos.p	#0,gr8
	bra		__syscall_exit

###################################################################################################
#
# Return to user mode is not as complex as all this looks,
# but we want the default path for a system call return to
# go as quickly as possible which is why some of this is
# less clear than it otherwise should be.
#
###################################################################################################
	.globl		system_call
system_call:
	LEDS		0x6101
	movsg		psr,gr4			; enable exceptions
	ori		gr4,#PSR_ET,gr4
	movgs		gr4,psr

	sti		gr7,@(gr28,#REG_SYSCALLNO)
	sti		gr8,@(gr28,#REG_ORIG_GR8)

	ldi		@(gr29,#tsk_ptrace),gr4
	andicc		gr4,#PT_TRACESYS,gr0,icc0
	bne		icc0,#0,__syscall_trace
//	bra		__syscall_trace

__syscall_call:
	subicc		gr7,#NR_syscalls,gr0,icc0
	bnc		icc0,#0,__syscall_badsys

	slli.p		gr7,#2,gr7
	sethi		%hi(sys_call_table),gr5
	setlo		%lo(sys_call_table),gr5
	ld		@(gr5,gr7),gr4
	calll		@(gr4,gr0)

	# rebuild saved psr - execve will change it for init/main.c
	ldi		@(gr28,#REG_PSR),gr22
	srli		gr22,#1,gr5
	andi.p		gr22,#~PSR_PS,gr22
	andi		gr5,#PSR_PS,gr5
	or		gr5,gr22,gr22
	ori		gr22,#PSR_S,gr22

__syscall_exit:
	LEDS		0x6102
	sti		gr8,@(gr28,#REG_GR(8))	; save return value
	ldi		@(gr29,#tsk_ptrace),gr4
	andicc		gr4,#PT_TRACESYS,gr0,icc0
	bne		icc0,#0,__syscall_trace_exit
//	bra		__syscall_trace_exit

###############################################################################
#
# return to interrupted process
#
###############################################################################
__entry_return_from_exception:
	LEDS		0x6103

	# don't resched or process signals if returning to kernel space
	andicc		gr22,#PSR_PS,gr0,icc0
	bne		icc0,#0,__entry_return_direct

	# need_resched and signals must be tested atomically
	movsg		psr,gr4
	ori		gr4,#PSR_PIL_15,gr4
	movgs		gr4,psr

	ldi		@(gr29,#need_resched),gr4
	ldi.p		@(gr29,#sigpending),gr5
	andcc		gr4,gr4,gr0,icc0
	andcc.p		gr5,gr5,gr0,icc1

	bne		icc0,#0,__entry_reschedule
	bne		icc1,#0,__entry_process_signal

__entry_return_direct:
	LEDS		0x6104

	andi		gr22,#~PSR_ET,gr22
	movgs		gr22,psr

	ldi		@(gr28,#REG_ISR),gr23
	lddi		@(gr28,#REG_CCR),gr24
	lddi		@(gr28,#REG_LR) ,gr26
	ldi		@(gr28,#REG_PC) ,gr21
	ldi		@(gr28,#REG_TBR),gr20

	movgs		gr20,tbr
	movgs		gr21,pcsr
	movgs		gr23,isr
	movgs		gr24,ccr
	movgs		gr25,cccr
	movgs		gr26,lr
	movgs		gr27,lcr

	lddi		@(gr28,#REG_GNER0),gr4
	movgs		gr4,gner0
	movgs		gr5,gner1

	lddi		@(gr28,#REG_IACC0),gr4
	movgs		gr4,iacc0h
	movgs		gr5,iacc0l

	lddi		@(gr28,#REG_GR(4)) ,gr4
	lddi		@(gr28,#REG_GR(6)) ,gr6
	lddi		@(gr28,#REG_GR(8)) ,gr8
	lddi		@(gr28,#REG_GR(10)),gr10
	lddi		@(gr28,#REG_GR(12)),gr12
	lddi		@(gr28,#REG_GR(14)),gr14
	lddi		@(gr28,#REG_GR(16)),gr16
	lddi		@(gr28,#REG_GR(18)),gr18
	lddi		@(gr28,#REG_GR(20)),gr20
	lddi		@(gr28,#REG_GR(22)),gr22
	lddi		@(gr28,#REG_GR(24)),gr24
	lddi		@(gr28,#REG_GR(26)),gr26
	ldi		@(gr28,#REG_GR(29)),gr29
	lddi		@(gr28,#REG_GR(30)),gr30

	# check to see if a debugging return is required
	LEDS		0x67f0
	movsg		ccr,gr2
	ldi		@(gr28,#REG__STATUS),gr3
	andicc		gr3,#REG__STATUS_STEP,gr0,icc0
	bne		icc0,#0,__entry_return_singlestep
	movgs		gr2,ccr

	ldi		@(gr28,#REG_SP)    ,sp
	lddi		@(gr28,#REG_GR(2)) ,gr2
	ldi		@(gr28,#REG_GR(28)),gr28

	LEDS		0x67fe

#if 0
	# store the current frame in the workram on the FR451
	movgs		gr28,scr2
	sethi.p		%hi(0xfe800000),gr28
	setlo		%lo(0xfe800000),gr28

	stdi		gr2,@(gr28,#REG_GR(2))
	stdi		gr4,@(gr28,#REG_GR(4))
	stdi		gr6,@(gr28,#REG_GR(6))
	stdi		gr8,@(gr28,#REG_GR(8))
	stdi		gr10,@(gr28,#REG_GR(10))
	stdi		gr12,@(gr28,#REG_GR(12))
	stdi		gr14,@(gr28,#REG_GR(14))
	stdi		gr16,@(gr28,#REG_GR(16))
	stdi		gr18,@(gr28,#REG_GR(18))
	stdi		gr24,@(gr28,#REG_GR(24))
	stdi		gr26,@(gr28,#REG_GR(26))
	sti		gr29,@(gr28,#REG_GR(29))
	stdi		gr30,@(gr28,#REG_GR(30))

	movsg		tbr ,gr30
	sti		gr30,@(gr28,#REG_TBR)
	movsg		pcsr,gr30
	sti		gr30,@(gr28,#REG_PC)
	movsg		psr ,gr30
	sti		gr30,@(gr28,#REG_PSR)
	movsg		isr ,gr30
	sti		gr30,@(gr28,#REG_ISR)
	movsg		ccr ,gr30
	movsg		cccr,gr31
	stdi		gr30,@(gr28,#REG_CCR)
	movsg		lr  ,gr30
	movsg		lcr ,gr31
	stdi		gr30,@(gr28,#REG_LR)
	sti		gr0 ,@(gr28,#REG_SYSCALLNO)
	movsg		scr2,gr28
#endif

	rett		#0

__entry_return_singlestep:
	movgs		gr2,ccr
	lddi		@(gr28,#REG_GR(2)) ,gr2
	ldi		@(gr28,#REG_SP)    ,sp
	ldi		@(gr28,#REG_GR(28)),gr28
	LEDS		0x67ff
	break
	.globl		__entry_return_singlestep_breaks_here
__entry_return_singlestep_breaks_here:
	nop

__entry_reschedule:
	LEDS		0x6700
	call		SYMBOL_NAME(schedule)
	bra		__entry_return_from_exception

__entry_process_signal:
	LEDS		0x6701

	# we can get here from an interrupt handler
	movsg		psr,gr4
	andi		gr4,#~PSR_PIL_15,gr4
	movgs		gr4,psr

	ori.p		gr28,#0,gr8
	xor		gr9,gr9,gr9
	call		SYMBOL_NAME(do_signal)
	bra		__entry_return_direct

__syscall_badsys:
	setlos.p	#-ENOSYS,gr4
	bra		__syscall_exit

__syscall_trace:
	setlos.p	#0,gr8
	call		SYMBOL_NAME(syscall_trace)

	ldi		@(gr28,#REG_SYSCALLNO),gr7
	lddi		@(gr28,#REG_GR(8)) ,gr8
	lddi		@(gr28,#REG_GR(10)),gr10
	lddi.p		@(gr28,#REG_GR(12)),gr12
	bra		__syscall_call

__syscall_trace_exit:
	setlos.p	#1,gr8
	call		SYMBOL_NAME(syscall_trace)
	bra		__entry_return_from_exception

###############################################################################
#
# syscall vector table
#
###############################################################################
	.section .rodata
ALIGN
	.globl		sys_call_table
SYMBOL_NAME_LABEL(sys_call_table)
	.long SYMBOL_NAME(sys_ni_syscall)	/* 0  -  old "setup()" system call*/
	.long SYMBOL_NAME(sys_exit)
	.long SYMBOL_NAME(sys_fork)
	.long SYMBOL_NAME(sys_read)
	.long SYMBOL_NAME(sys_write)
	.long SYMBOL_NAME(sys_open)		/* 5 */
	.long SYMBOL_NAME(sys_close)
	.long SYMBOL_NAME(sys_waitpid)
	.long SYMBOL_NAME(sys_creat)
	.long SYMBOL_NAME(sys_link)
	.long SYMBOL_NAME(sys_unlink)		/* 10 */
	.long SYMBOL_NAME(sys_execve)
	.long SYMBOL_NAME(sys_chdir)
	.long SYMBOL_NAME(sys_time)
	.long SYMBOL_NAME(sys_mknod)
	.long SYMBOL_NAME(sys_chmod)		/* 15 */
	.long SYMBOL_NAME(sys_lchown16)
	.long SYMBOL_NAME(sys_ni_syscall)			/* old break syscall holder */
	.long SYMBOL_NAME(sys_stat)
	.long SYMBOL_NAME(sys_lseek)
	.long SYMBOL_NAME(sys_getpid)		/* 20 */
	.long SYMBOL_NAME(sys_mount)
	.long SYMBOL_NAME(sys_oldumount)
	.long SYMBOL_NAME(sys_setuid16)
	.long SYMBOL_NAME(sys_getuid16)
	.long SYMBOL_NAME(sys_stime)		/* 25 */
	.long SYMBOL_NAME(sys_ptrace)
	.long SYMBOL_NAME(sys_alarm)
	.long SYMBOL_NAME(sys_fstat)
	.long SYMBOL_NAME(sys_ni_syscall)			/* pause */
	.long SYMBOL_NAME(sys_utime)		/* 30 */
	.long SYMBOL_NAME(sys_ni_syscall)			/* old stty syscall holder */
	.long SYMBOL_NAME(sys_ni_syscall)			/* old gtty syscall holder */
	.long SYMBOL_NAME(sys_access)
	.long SYMBOL_NAME(sys_nice)
	.long SYMBOL_NAME(sys_ni_syscall)	/* 35 */	/* old ftime syscall holder */
	.long SYMBOL_NAME(sys_sync)
	.long SYMBOL_NAME(sys_kill)
	.long SYMBOL_NAME(sys_rename)
	.long SYMBOL_NAME(sys_mkdir)
	.long SYMBOL_NAME(sys_rmdir)		/* 40 */
	.long SYMBOL_NAME(sys_dup)
	.long SYMBOL_NAME(sys_pipe)
	.long SYMBOL_NAME(sys_times)
	.long SYMBOL_NAME(sys_ni_syscall)			/* old prof syscall holder */
	.long SYMBOL_NAME(sys_brk)		/* 45 */
	.long SYMBOL_NAME(sys_setgid16)
	.long SYMBOL_NAME(sys_getgid16)
	.long SYMBOL_NAME(sys_signal)
	.long SYMBOL_NAME(sys_geteuid16)
	.long SYMBOL_NAME(sys_getegid16)	/* 50 */
	.long SYMBOL_NAME(sys_acct)
	.long SYMBOL_NAME(sys_umount)				/* recycled never used phys() */
	.long SYMBOL_NAME(sys_ni_syscall)			/* old lock syscall holder */
	.long SYMBOL_NAME(sys_ioctl)
	.long SYMBOL_NAME(sys_fcntl)		/* 55 */
	.long SYMBOL_NAME(sys_ni_syscall)			/* old mpx syscall holder */
	.long SYMBOL_NAME(sys_setpgid)
	.long SYMBOL_NAME(sys_ni_syscall)			/* old ulimit syscall holder */
	.long SYMBOL_NAME(sys_ni_syscall)			/* old old uname syscall */
	.long SYMBOL_NAME(sys_umask)		/* 60 */
	.long SYMBOL_NAME(sys_chroot)
	.long SYMBOL_NAME(sys_ustat)
	.long SYMBOL_NAME(sys_dup2)
	.long SYMBOL_NAME(sys_getppid)
	.long SYMBOL_NAME(sys_getpgrp)		/* 65 */
	.long SYMBOL_NAME(sys_setsid)
	.long SYMBOL_NAME(sys_sigaction)
	.long SYMBOL_NAME(sys_sgetmask)
	.long SYMBOL_NAME(sys_ssetmask)
	.long SYMBOL_NAME(sys_setreuid16)	/* 70 */
	.long SYMBOL_NAME(sys_setregid16)
	.long SYMBOL_NAME(sys_sigsuspend)
	.long SYMBOL_NAME(sys_sigpending)
	.long SYMBOL_NAME(sys_sethostname)
	.long SYMBOL_NAME(sys_setrlimit)	/* 75 */
	.long SYMBOL_NAME(sys_old_getrlimit)
	.long SYMBOL_NAME(sys_getrusage)
	.long SYMBOL_NAME(sys_gettimeofday)
	.long SYMBOL_NAME(sys_settimeofday)
	.long SYMBOL_NAME(sys_getgroups16)	/* 80 */
	.long SYMBOL_NAME(sys_setgroups16)
	.long SYMBOL_NAME(sys_ni_syscall)			/* old_select slot */
	.long SYMBOL_NAME(sys_symlink)
	.long SYMBOL_NAME(sys_lstat)
	.long SYMBOL_NAME(sys_readlink)		/* 85 */
	.long SYMBOL_NAME(sys_uselib)
	.long SYMBOL_NAME(sys_swapon)
	.long SYMBOL_NAME(sys_reboot)
	.long SYMBOL_NAME(old_readdir)
	.long SYMBOL_NAME(sys_ni_syscall)	/* 90 */	/* old_mmap slot */
	.long SYMBOL_NAME(sys_munmap)
	.long SYMBOL_NAME(sys_truncate)
	.long SYMBOL_NAME(sys_ftruncate)
	.long SYMBOL_NAME(sys_fchmod)
	.long SYMBOL_NAME(sys_fchown16)		/* 95 */
	.long SYMBOL_NAME(sys_getpriority)
	.long SYMBOL_NAME(sys_setpriority)
	.long SYMBOL_NAME(sys_ni_syscall)			/* old profil syscall holder */
	.long SYMBOL_NAME(sys_statfs)
	.long SYMBOL_NAME(sys_fstatfs)		/* 100 */
	.long SYMBOL_NAME(sys_ni_syscall)			/* ioperm for i386 */
	.long SYMBOL_NAME(sys_socketcall)
	.long SYMBOL_NAME(sys_syslog)
	.long SYMBOL_NAME(sys_setitimer)
	.long SYMBOL_NAME(sys_getitimer)	/* 105 */
	.long SYMBOL_NAME(sys_newstat)
	.long SYMBOL_NAME(sys_newlstat)
	.long SYMBOL_NAME(sys_newfstat)
	.long SYMBOL_NAME(sys_ni_syscall)	/* obsolete olduname() syscall */
	.long SYMBOL_NAME(sys_ni_syscall)	/* iopl for i386 */ /* 110 */
	.long SYMBOL_NAME(sys_vhangup)
	.long SYMBOL_NAME(sys_ni_syscall)	/* obsolete idle() syscall */
	.long SYMBOL_NAME(sys_ni_syscall)	/* vm86old for i386 */
	.long SYMBOL_NAME(sys_wait4)
	.long SYMBOL_NAME(sys_swapoff)		/* 115 */
	.long SYMBOL_NAME(sys_sysinfo)
	.long SYMBOL_NAME(sys_ipc)
	.long SYMBOL_NAME(sys_fsync)
	.long SYMBOL_NAME(sys_sigreturn)
	.long SYMBOL_NAME(sys_clone)		/* 120 */
	.long SYMBOL_NAME(sys_setdomainname)
	.long SYMBOL_NAME(sys_newuname)
	.long SYMBOL_NAME(sys_cacheflush)	/* modify_ldt for i386 */
	.long SYMBOL_NAME(sys_adjtimex)
	.long SYMBOL_NAME(sys_mprotect)		/* 125 */
	.long SYMBOL_NAME(sys_sigprocmask)
	.long SYMBOL_NAME(sys_create_module)
	.long SYMBOL_NAME(sys_init_module)
	.long SYMBOL_NAME(sys_delete_module)
	.long SYMBOL_NAME(sys_get_kernel_syms)	/* 130 */
	.long SYMBOL_NAME(sys_quotactl)
	.long SYMBOL_NAME(sys_getpgid)
	.long SYMBOL_NAME(sys_fchdir)
	.long SYMBOL_NAME(sys_bdflush)
	.long SYMBOL_NAME(sys_sysfs)		/* 135 */
	.long SYMBOL_NAME(sys_personality)
	.long SYMBOL_NAME(sys_ni_syscall)	/* for afs_syscall */
	.long SYMBOL_NAME(sys_setfsuid16)
	.long SYMBOL_NAME(sys_setfsgid16)
	.long SYMBOL_NAME(sys_llseek)		/* 140 */
	.long SYMBOL_NAME(sys_getdents)
	.long SYMBOL_NAME(sys_select)
	.long SYMBOL_NAME(sys_flock)
	.long SYMBOL_NAME(sys_msync)
	.long SYMBOL_NAME(sys_readv)		/* 145 */
	.long SYMBOL_NAME(sys_writev)
	.long SYMBOL_NAME(sys_getsid)
	.long SYMBOL_NAME(sys_fdatasync)
	.long SYMBOL_NAME(sys_sysctl)
	.long SYMBOL_NAME(sys_mlock)		/* 150 */
	.long SYMBOL_NAME(sys_munlock)
	.long SYMBOL_NAME(sys_mlockall)
	.long SYMBOL_NAME(sys_munlockall)
	.long SYMBOL_NAME(sys_sched_setparam)
	.long SYMBOL_NAME(sys_sched_getparam)   /* 155 */
	.long SYMBOL_NAME(sys_sched_setscheduler)
	.long SYMBOL_NAME(sys_sched_getscheduler)
	.long SYMBOL_NAME(sys_sched_yield)
	.long SYMBOL_NAME(sys_sched_get_priority_max)
	.long SYMBOL_NAME(sys_sched_get_priority_min)  /* 160 */
	.long SYMBOL_NAME(sys_sched_rr_get_interval)
	.long SYMBOL_NAME(sys_nanosleep)
	.long SYMBOL_NAME(sys_mremap)
	.long SYMBOL_NAME(sys_setresuid16)
	.long SYMBOL_NAME(sys_getresuid16)	/* 165 */
	.long SYMBOL_NAME(sys_ni_syscall)	/* for vm86 */
	.long SYMBOL_NAME(sys_query_module)
	.long SYMBOL_NAME(sys_poll)
	.long SYMBOL_NAME(sys_nfsservctl)
	.long SYMBOL_NAME(sys_setresgid16)	/* 170 */
	.long SYMBOL_NAME(sys_getresgid16)
	.long SYMBOL_NAME(sys_prctl)
	.long SYMBOL_NAME(sys_rt_sigreturn)
	.long SYMBOL_NAME(sys_rt_sigaction)
	.long SYMBOL_NAME(sys_rt_sigprocmask)	/* 175 */
	.long SYMBOL_NAME(sys_rt_sigpending)
	.long SYMBOL_NAME(sys_rt_sigtimedwait)
	.long SYMBOL_NAME(sys_rt_sigqueueinfo)
	.long SYMBOL_NAME(sys_rt_sigsuspend)
	.long SYMBOL_NAME(sys_pread)		/* 180 */
	.long SYMBOL_NAME(sys_pwrite)
	.long SYMBOL_NAME(sys_chown16)
	.long SYMBOL_NAME(sys_getcwd)
	.long SYMBOL_NAME(sys_capget)
	.long SYMBOL_NAME(sys_capset)           /* 185 */
	.long SYMBOL_NAME(sys_sigaltstack)
	.long SYMBOL_NAME(sys_sendfile)
	.long SYMBOL_NAME(sys_ni_syscall)		/* streams1 */
	.long SYMBOL_NAME(sys_ni_syscall)		/* streams2 */
	.long SYMBOL_NAME(sys_vfork)            /* 190 */
	.long SYMBOL_NAME(sys_getrlimit)
	.long SYMBOL_NAME(sys_mmap2)
	.long SYMBOL_NAME(sys_truncate64)
	.long SYMBOL_NAME(sys_ftruncate64)
	.long SYMBOL_NAME(sys_stat64)		/* 195 */
	.long SYMBOL_NAME(sys_lstat64)
	.long SYMBOL_NAME(sys_fstat64)
	.long SYMBOL_NAME(sys_lchown)
	.long SYMBOL_NAME(sys_getuid)
	.long SYMBOL_NAME(sys_getgid)		/* 200 */
	.long SYMBOL_NAME(sys_geteuid)
	.long SYMBOL_NAME(sys_getegid)
	.long SYMBOL_NAME(sys_setreuid)
	.long SYMBOL_NAME(sys_setregid)
	.long SYMBOL_NAME(sys_getgroups)	/* 205 */
	.long SYMBOL_NAME(sys_setgroups)
	.long SYMBOL_NAME(sys_fchown)
	.long SYMBOL_NAME(sys_setresuid)
	.long SYMBOL_NAME(sys_getresuid)
	.long SYMBOL_NAME(sys_setresgid)	/* 210 */
	.long SYMBOL_NAME(sys_getresgid)
	.long SYMBOL_NAME(sys_chown)
	.long SYMBOL_NAME(sys_setuid)
	.long SYMBOL_NAME(sys_setgid)
	.long SYMBOL_NAME(sys_setfsuid)		/* 215 */
	.long SYMBOL_NAME(sys_setfsgid)
	.long SYMBOL_NAME(sys_pivot_root)
	.long SYMBOL_NAME(sys_mincore)
	.long SYMBOL_NAME(sys_madvise)
	.long SYMBOL_NAME(sys_getdents64)	/* 220 */
	.long SYMBOL_NAME(sys_fcntl64)
	.long SYMBOL_NAME(sys_ni_syscall)	/* reserved for TUX */
	.long SYMBOL_NAME(sys_ni_syscall)	/* Reserved for Security */
	.long SYMBOL_NAME(sys_gettid)
	.long SYMBOL_NAME(sys_readahead)	/* 225 */
	.long SYMBOL_NAME(sys_setxattr)
	.long SYMBOL_NAME(sys_lsetxattr)
	.long SYMBOL_NAME(sys_fsetxattr)
	.long SYMBOL_NAME(sys_getxattr)
	.long SYMBOL_NAME(sys_lgetxattr)	/* 230 */
	.long SYMBOL_NAME(sys_fgetxattr)
	.long SYMBOL_NAME(sys_listxattr)
	.long SYMBOL_NAME(sys_llistxattr)
	.long SYMBOL_NAME(sys_flistxattr)
	.long SYMBOL_NAME(sys_removexattr)	/* 235 */
	.long SYMBOL_NAME(sys_lremovexattr)
	.long SYMBOL_NAME(sys_fremovexattr)
 	.long SYMBOL_NAME(sys_tkill)
	.long SYMBOL_NAME(sys_sendfile64)
#ifndef CONFIG_UCLINUX
	.long SYMBOL_NAME(sys_ni_syscall)	/* 240 */	/* futex */
	.long SYMBOL_NAME(sys_ni_syscall)			/* sched_setaffinity */
	.long SYMBOL_NAME(sys_ni_syscall)			/* sched_getaffinity */
#else
	.long SYMBOL_NAME(sys_ni_syscall)	/* 240 reserved for futex */
	.long SYMBOL_NAME(sys_ni_syscall)	/* reserved for sched_setaffinity */
	.long SYMBOL_NAME(sys_ni_syscall)	/* reserved for sched_getaffinity */
#endif
	.long SYMBOL_NAME(sys_ni_syscall)	/* sys_set_thread_area */
	.long SYMBOL_NAME(sys_ni_syscall)	/* sys_get_thread_area */
	.long SYMBOL_NAME(sys_ni_syscall)	/* 245 sys_io_setup */
	.long SYMBOL_NAME(sys_ni_syscall)	/* sys_io_destroy */
	.long SYMBOL_NAME(sys_ni_syscall)	/* sys_io_getevents */
	.long SYMBOL_NAME(sys_ni_syscall)	/* sys_io_submit */
	.long SYMBOL_NAME(sys_ni_syscall)	/* sys_io_cancel */
	.long SYMBOL_NAME(sys_ni_syscall)	/* 250 sys_alloc_hugepages */
	.long SYMBOL_NAME(sys_ni_syscall)	/* sys_free_hugepages */
	.long SYMBOL_NAME(sys_ni_syscall)	/* sys_exit_group */
	.long SYMBOL_NAME(sys_ni_syscall)	/* sys_lookup_dcookie */
	.long SYMBOL_NAME(sys_ni_syscall)	/* sys_epoll_create */
	.long SYMBOL_NAME(sys_ni_syscall)	/* sys_epoll_ctl 255 */
	.long SYMBOL_NAME(sys_ni_syscall)	/* sys_epoll_wait */
 	.long SYMBOL_NAME(sys_ni_syscall)	/* sys_remap_file_pages */
 	.long SYMBOL_NAME(sys_ni_syscall)	/* sys_set_tid_address */

	.long SYMBOL_NAME(sys_ni_syscall)	/* sys_timer_create */
	.long SYMBOL_NAME(sys_ni_syscall)	/* sys_timer_settime */		/* 260 */
	.long SYMBOL_NAME(sys_ni_syscall)	/* sys_timer_gettime */
	.long SYMBOL_NAME(sys_ni_syscall)	/* sys_timer_getoverrun */
	.long SYMBOL_NAME(sys_ni_syscall)	/* sys_timer_delete */
	.long SYMBOL_NAME(sys_ni_syscall)	/* sys_clock_settime */
	.long SYMBOL_NAME(sys_ni_syscall)	/* sys_clock_gettime */		/* 265 */
	.long SYMBOL_NAME(sys_ni_syscall)	/* sys_clock_getres */
	.long SYMBOL_NAME(sys_ni_syscall)	/* sys_clock_nanosleep */

	.rept NR_syscalls-(.-sys_call_table)/4
		.long SYMBOL_NAME(sys_ni_syscall)
	.endr
