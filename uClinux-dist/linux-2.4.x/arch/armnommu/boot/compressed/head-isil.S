/* $Header$
 * linux/arch/arm/boot/compressed/head-poldhu.S
 *
 * Copyright (C) 1996-1999 Russell King
 * Copyright (C) 2002 Intersil Americas Inc.
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License
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
 */
#include <linux/linkage.h>
#include <linux/config.h>

/* 
 * The 3893 has a problem with LDMs that cross a 4 kbyte boundary. Therefore
 * we need to make all LDMs single LDRs.
 */
#define ISL3893_NO_LDM

		.section ".start", #alloc, #execinstr
/*
 * NOTE: All code in the 'start' section should be position independent
 *       Code in this section relocates the decompression code
 *       to the address it is linked on.
 */
		.align
start:
		.type	start,#function
		.long	0x59414B4F		@ "OKAY" to identify an image
		b	relocate
		.long	0x54535953		@ "SYST"
		.long	1			@ length = 1
		.long	3			@ flash status, prefered + valid

/*
 * Between the .start and the .main section, the boot record
 * area is located. This is defined in the file bootrec.c and
 * is linked to the correct location as defined in the linker
 * script vmlinux.lds.in
 */

		.section ".main", #alloc, #execinstr

/*
 * Memory Layout of a compressed kernel image:
 *
 * _start ----------------> *=======================* -+
 * (CONFIG_ZIMAGE_TEXTADDR) |     section "start"   |  |
 *                          |-----------------------|  |
 *                          |   Boot Record Area    |  | Position Independent Code
 * relocate --------------> |-----------------------|  |
 *                          |    relocation code    |  |
 * _piggy_start ----------> |-----------------------| -+
 *                          |                       |
 *                          |                       |
 *                          |   Compressed Linux    |
 *                          |    Image ("piggy")    |
 *                          |                       |
 *                          |                       |
 * _piggy_end ------------> |-----------------------|
 * _reloc_start-----------> |-----------------------| -+
 *                          |   decompression code  |  | NOT Position Independent
 * _reloc_end  -----------> *=======================* -+
 *
 * The entry point is at _start + 4. _start is the address that the compressed
 * kernel image is linked to, but this can be different from the address that
 * we are currently executing (we can be anywhere in memory). Our real start address
 * is passed in r0. So (_start - r0) is the offset between where we are executing and
 * where we are linked. This offset can be used to find out where the code is really
 * located (in flash or SDRAM). So to find where _piggy_end really is we calculate
 * _piggy_end - (the offset) = _piggy_end - (_start - r0) = _piggy_end - _start + r0.
 * NB r0 is stored in r7 in the code below.
 *
 * The decompression code is the only part that is not Position Independent (PIC).
 * Therefore, it must be relocated. Note that we get an error if our real
 * start address would be larger that our link start address. In that case, the
 * relocated decompression code would overwrite our "piggy".
 *
 * The kernel is eventually decompressed to the link address of the kernel itself
 * (TEXTADDR), which is defined in arch/armnommu/Makefile.
 */


/*
 * r0 (saved r7): Our own start address
 * r1 (saved r8): Address of boot struct
 * r2 (saved r9): Address of boot parameter block
 */
 
relocate:
		mov	r7, r0			@ Save r0, r1, r2
		mov	r8, r1
		mov	r9, r2
		bl	init_uart

	mov	r0, #'O'
	bl	printch

		ldr	r0, [r8]
		ldr	r1, =0x59414B4F		@ Check for Boot Magic "OKAY"
		cmp	r0, r1
		beq	boot_magic_ok
		ldr	r1, =0x32765342		@ Check for Boot Magic "BSv2"
		cmp	r0, r1
		beq	boot_magic_ok
		b	boot_magic_error

boot_magic_ok:
		mov r0, #'K'
		bl printch
		adr	r1, LC0
#ifdef ISL3893_NO_LDM
		ldr	r2, [r1], #4
		ldr	r3, [r1], #4
		ldr	r4, [r1], #4
		ldr	lr, [r1], #4
#else
		ldmia	r1, {r2, r3, r4, lr}
#endif
/* r2 = _start
 * r3 = _reloc_start
 * r4 = _reloc_end
 * lr = decomp_entry label
 */

		cmp	r2, r7			@ Do not relocate if _start == our own start addr
		moveq	pc, lr
/* Check if the relocated code overwrites the image that might be stored
 * in SDRAM (i.e. same address space as where the relocated code ends up).
 * For the following two conditions we have an error because the relocation
 * code overwrites the image.
 * a) If the end of the relocation code < the real image start address
 *    we can start the relocation code without problems
 * b) Otherwise if the start of the relocation code <= the real image end
 *    the relocation code will overwrite the image so jump to reloc error
 *    otherwise fall through to the relocation code.
 * For a) the calculation is if reloc_end < _start - ( _start - r0 ) 
 * with results in if reloc_end < r0 then goto the relocation code
 * For b) the calculation is if reloc_start <= _reloc_end - ( _start - r0 )
 * or rewritten as reloc_start <= _reloc_end - _start + r0 then goto the
 * relocation error code otherwise: start the relocation code.
 */
		cmp	r4, r7			@ NB: reloc_end == r4
		blo	reloc			@ r4 < r0 goto relocation code
		sub	r5, r4, r2		@ reloc_end - _start
		add	r5, r5, r7		@ + r0
		cmp	r4, r5			@ If reloc_start <=
		bls	reloc_error		@ goto reloc_error

reloc:		sub	r5, r4, r2		@ source of relocation: 
		add	r5, r5, r7		@ _reloc_end - _start + real_start

1:		.rept	4
#ifdef ISL3893_NO_LDM
		ldr	sp, [r5, #-4]!
		ldr	r12, [r5, #-4]!
		ldr	r11, [r5, #-4]!
		ldr	r10, [r5, #-4]!
		str	sp, [r4, #-4]!
		str	r12, [r4, #-4]!
		str	r11, [r4, #-4]!
		str	r10, [r4, #-4]!
#else
		ldmdb	r5!, {r10 - r12, sp}	@ relocate code, 64 bytes each time
		stmdb	r4!, {r10 - r12, sp}
#endif
		.endr
		sub	r6, r4, r3
		cmp	r6, #64
		bgt	1b
#ifdef ISL3893_NO_LDM
1:		ldr	r6, [r5, #-4]!
		str	r6, [r4, #-4]!
#else
1:		ldmdb	r5!, {r6}		@ copy the remaining bytes
		stmdb	r4!, {r6}
#endif
		cmp	r4, r3
		bgt	1b

		mov	pc, lr			@ call relocated code

reloc_error:
		mov	r4, r0
		adr	r0, Reloc_Err
		bl	printascii

		mov	r0, r4			@ Print our real start address
		bl	printhex8
loop:		b	loop

LC0:		.word	_start			@ linker generated start address
		.word	_reloc_start		@ destination for relocation
		.word	_reloc_end		@ end of relocation
		.word	decomp_entry

Reloc_Err:	.ascii	"\nFatal error: Relocation overwrites compressed kernel image. r0: 0x\0"
		.align

		.text
/*
 * Now we are relocated to the correct address, so we can set up BSS, stack and
 * malloc area for the code that is going to decompress the kernel.
 */
decomp_entry:
		mov r0, #'A'
		bl printch

		mrc	p15, 0, r6, c0, c0	@ get processor ID

		adr	r1, LC1

#ifdef ISL3893_NO_LDM
		ldr	r2, [r1], #4
		ldr	r3, [r1], #4
		ldr	r4, [r1], #4
		ldr	r5, [r1], #4
		ldr	sp, [r1], #4
#else
		ldmia	r1, {r2, r3, r4, r5, sp}
#endif
/*
 * r2	= __bss_start
 * r3	= _end
 * r4	= _load_addr
 * r5	= _start
 * sp	= user_stack+4096
 */

		mov	r0, #0
1:		str	r0, [r2], #4		@ clear bss
		str	r0, [r2], #4
		str	r0, [r2], #4
		str	r0, [r2], #4
		cmp	r2, r3
		blt	1b

/* Calculate the real start and end address of our compressed kernel and store that
 * in the input_data variables for the decompression algorithm. This should be done
 * after zeroing BSS!
 * 	input_data = _piggy_start - link_address of _start label + load_address of _start label
 */
		ldr	r1, =input_data
		ldr	r2, =_piggy_start
		sub	r2, r2, r5
		add	r2, r2, r7
		str	r2, [r1]

		ldr	r1, =input_data_end
		ldr	r2, =_piggy_end
		sub	r2, r2, r5
		add	r2, r2, r7
		str	r2, [r1]

/* Boot struct:
 * {
 * 	unsigned long Magic = "OKAY";
 * 	unsigned long RAMbase;
 *	unsigned long RAMsize;
 * }
 * Boot struct v2:
 * {
 * 	unsigned long Magic = "BSv2";
 *	void 	    (*reset)(void);
 *	int	     *BIS_info;
 * 	unsigned long RAMbase;
 *	unsigned long RAMsize;
 * }
 */

		ldr	r0, [r8]
		ldr	r1, =0x59414B4F		@ Check for Boot Magic "OKAY"
		cmp	r0, r1
		beq	bootstruct_v1
		ldr	r1, =0x32765342		@ Check for Boot Magic "BSv2"
		cmp	r0, r1
		beq	bootstruct_v2
		b	boot_magic_error

bootstruct_v1:
		add	r5, r8, #4		@ pointer to RAM base
		add	r6, r8, #8		@ pointer to RAM size
		b	1f

bootstruct_v2:
		add	r5, r8, #12		@ pointer to RAM base
		add	r6, r8, #16		@ pointer to RAM size

1:		ldr	r0, [r5]		@ get RAM Base
		ldr	r1, [r6]		@ get RAM size

		cmp	r4, r0			@ Compare load address with RAM Base
		beq	1f
		blo	loader_error		@ If the actual load address is before RAM Base
						@ we're in trouble!

		subs	r0, r4, r0		@ We load ourselfs higher than the RAM
		subs	r1, r1, r0		@ Base address, so we have less RAM size
		str	r4, [r5]		@ Store the new RAM base (ie. our load address)
		str	r1, [r6]		@ Store the new RAM size

1:		add	r2, r4, r1		@ malloc space ends at highest free RAM adress
		mov	r1, sp			@ malloc space starts above stack

		mov	r0, r4
		ldr	r3, =_reloc_start

/*
 * Parameters for 'decompress kernel'
 * r0   = startaddress for decompressed kernel
 * r1   = start of malloc area
 * r2   = end of malloc area
 * r3   = highest allowed end address for decompressed kernel. This is the start address
 *	  of this loader, to make sure that we don't overwrite ourselves.
 * return:
 * r0	= decompressed kernel length
 */

		bl	SYMBOL_NAME(decompress_kernel)

/*
 * Kernel Calling Convention:
 * r0	= Address where the kernel is loaded
 * r1	= Pointer to the boot structure
 * r2   = Pointer to the boot parameter block
 */

call_kernel:	mov	r0, r4
		mov	r1, r8			@ restore pointer to boot structure
		mov	r2, r9			@ restore pointer to boot parameter block
		mov	pc, r4			@ call kernel

loader_error:	mov	r5, r0			@ r4 = load_addr, r5 = RAM base, r6 = RAM size
		mov	r6, r1
		adr	r0, Fatal_1
		bl	printascii
		mov	r0, r4			@ load address
		bl	printhex8
		mov	r0, #' '
		bl	printch
		mov	r0, r5			@ RAM base
		bl	printhex8
		mov	r0, #' '
		bl	printch
		mov	r0, r6			@ RAM size
		bl	printhex8

		b	loop_forever

boot_magic_error:
		mov	r4, r0
		adr	r0, Boot_Magic_Err
		bl	printascii
		mov	r0, r4
		bl	printhex8
		b	loop_forever

loop_forever:
		mov r0, #'?'
		bl printch
		b	loop_forever

/*
 * DEBUGGING SUPPORT. Stolen from arch/armnommu/kernel/debug-armv.S
 */
#if defined(CONFIG_ARCH_POLDHU)
#include <asm/arch/hardware.h>
#include <asm/arch/gpio.h>
#include <asm/arch/serial.h>
ENTRY(init_uart)
        mov     r1, #PERIPH_BASE
        mov     r0, #0x18
        orr     r0, r0, r0, lsl #16
        str     r0, [r1, #uGPIOFSEL - PERIPH_BASE]
	mov	r0, #0
	str	r0, [r1, #uUARTCR - PERIPH_BASE]
	mov	r0, #47					@ baud_div & UARTLCR_LMask
	str	r0, [r1, #uUARTLCR_L - PERIPH_BASE]
	mov	r0, #0					@ (baud_div >> 8) & UARTLCR_MMask;
	str	r0, [r1, #uUARTLCR_M - PERIPH_BASE]
	mov	r0, #96					@ (3 << UARTLCR_HWLENShift);
	str	r0, [r1, #uUARTLCR_H - PERIPH_BASE]
	mov	r0, #UARTCRUARTEN
	str	r0, [r1, #uUARTCR - PERIPH_BASE]
        mov     pc, lr

	.macro	addruart,rx
	mov	\rx, #PERIPH_BASE
	.endm

	.macro	senduart,rd,rx
	str	\rd, [\rx, #uUARTDR - PERIPH_BASE]
	.endm

	.macro	waituart,rd,rx
1001:	ldr	\rd, [\rx, #uUARTFR - PERIPH_BASE]
	tst	\rd, #UARTFRTXF
	bne	1001b
	.endm

	.macro	busyuart,rd,rx
1002:	ldr	\rd, [\rx, #uUARTFR - PERIPH_BASE]
	tst	\rd, #UARTFRBUSY
	bne	1002b
	.endm
#elif defined(CONFIG_ARCH_ISL3893)
#include <asm/arch/hardware.h>
#include <asm/arch/serial.h>
#ifdef ISL3893_SIMSONLY
/* We only use the Tube here */
ENTRY(init_uart)
	/* Nothing to do */
        mov     pc, lr

	.macro	addruart,rx
	ldr	\rx, =uTUBEData
	.endm

	.macro	senduart,rd,rx
	strb	\rd, [\rx]
	.endm

	.macro	waituart,rd,rx
	/* Nothing to do here */
	.endm

	.macro	busyuart,rd,rx
	/* Nothing to do here */
	.endm
#else /* ISL3893_SIMSONLY */
ENTRY(init_uart)
	/* FIXME: Need to fill this in !! */
        mov     pc, lr

	.macro	addruart,rx
	mov	\rx, #PERIPH_BASE
	.endm

	.macro	senduart,rd,rx
	str	\rd, [\rx, #aUARTRegister0]
	.endm

	.macro	waituart,rd,rx
1001:		ldr	\rd, [\rx, #aUARTLineStatus]
	tst	\rd, #UARTLineStatusTHRE
	beq	1001b
	.endm

	.macro	busyuart,rd,rx
	/* Nothing to do here */
	.endm
#endif /* ISL3893_SIMSONLY */
#else
#error Unknown architecture
#endif

/*
 * Useful debugging routines
 */
ENTRY(printhex8)
		mov	r1, #8
		b	printhex

ENTRY(printhex4)
		mov	r1, #4
		b	printhex

ENTRY(printhex2)
		mov	r1, #2
printhex:	ldr	r2, =hexbuf
		add	r3, r2, r1
		mov	r1, #0
		strb	r1, [r3]
1:		and	r1, r0, #15
		mov	r0, r0, lsr #4
		cmp	r1, #10
		addlt	r1, r1, #'0'
		addge	r1, r1, #'a' - 10
		strb	r1, [r3, #-1]!
		teq	r3, r2
		bne	1b
		mov	r0, r2
		b	printascii

		.ltorg

ENTRY(printascii)
		addruart r3
		b	2f
1:		waituart r2, r3
		senduart r1, r3
		busyuart r2, r3
		teq	r1, #'\n'
		moveq	r1, #'\r'
		beq	1b
2:		teq	r0, #0
		ldrneb	r1, [r0], #1
		teqne	r1, #0
		bne	1b
		mov	pc, lr

ENTRY(printch)
		addruart r3
		mov	r1, r0
		mov	r0, #0
		b	1b

hexbuf:		.space 32


LC1:		.word	__bss_start
		.word	_end
		.word	_load_addr
		.word	_start
		.word	user_stack+4096
		.align

Fatal_1:	.ascii	"\nFatal error: Kernel start address is not in free RAM area. \0"
		.align

Boot_Magic_Err:	.ascii	"\nFatal error: Bootstruct has wrong Magic: 0x\0"
		.align

		.section	".stack"
/* ISL3893_NO_LDM
 * Need to align the stack on a 4k boundary because of LDM problem
 * when crossing a 4k boundary
 */
		.align	12
user_stack:	.space	4096


