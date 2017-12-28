/* $Id: head.S,v 1.1.1.1.2.1 2002/01/25 00:51:42 gniibe Exp $
 *
 *  arch/sh/kernel/head.S
 *
 *  Copyright (C) 1999, 2000  Niibe Yutaka & Kaz Kojima
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Head.S contains the SH exception handlers and startup code.
 *
 * Modified by Rahul Chaturvedi on 22/06/2004 (justanotheraliasforrahul@yahoo.com)
 * to add support for booting the kernel from ROM
 *
 */
#include <linux/linkage.h>

	.section	.empty_zero_page, "aw"
ENTRY(empty_zero_page)

#ifdef CONFIG_BLK_DEV_INITRD
	.long	1		/* MOUNT_ROOT_RDONLY */
	.long	0x4000		/* RAMDISK_FLAGS */
	.long	0x0100		/* ORIG_ROOT_DEV */
	.long	1		/* LOADER_TYPE */
#if defined(CONFIG_SH_SNAPGEAR)
	.long	_end		/* INITRD_START */
	.long	0x000000	/* INITRD_SIZE */
#else
	.long	0x00360000	/* INITRD_START */
	.long	0x000a0000	/* INITRD_SIZE */
#endif
	.long	0
#else
	.long	1		/* MOUNT_ROOT_RDONLY */
	.long	0		/* RAMDISK_FLAGS */
	.long	0x0200		/* ORIG_ROOT_DEV */
	.long	1		/* LOADER_TYPE */
	.long	0x00360000	/* INITRD_START */
	.long	0x000a0000	/* INITRD_SIZE */
	.long	0
#endif
#if defined(CONFIG_SH_SNAPGEAR)
	.balign 256,0,256
cmdline:
#if defined(CONFIG_SH_SECUREEDGE5410)
# if defined(CONFIG_SH_CONSOLE_TTYSC0)
	.ascii	"mem=16M console=ttySC0,57600\0"
# else
	.ascii	"mem=16M console=ttySC1,57600\0"
# endif
#else
	.ascii	"mem=32M console=ttySC1,57600\0"
#endif
#endif

	.balign 4096,0,4096
	.text	
/*
 * Condition at the entry of _stext:
 *
 *   BSC has already been initialized.
 *   INTC may or may not be initialized.
 *   VBR may or may not be initialized.
 *   MMU may or may not be initialized.
 *   Cache may or may not be initialized.
 *   Hardware (including on-chip modules) may or may not be initialized. 
 *
 */
ENTRY(_stext)

#if defined(CONFIG_SH_ROMBOOT)
!			Copy zero page to RAM

	mov.l	11f, r1		! r1 = src (zero page in ROM - _rom_store)
	mov.l	12f, r2		! r2 = dest (zero page dest in RAM - _mem_start)

	mov.l	13f, r0		! r0 = sizeof zero page (4k)
	add	r2, r0		! r0 = end of dest 

10:
	mov.b	@r1, r3
	mov.b	r3, @r2

	add #1, r1
	add #1, r2
	
	cmp/eq r2, r0
	bf 10b



!			Copy everything from _etext to __bss_start to RAM

	mov.l	7f, r2		! r2 = dest (_etext)
	mov	r2, r1
	mov.l	12f, r3	
	sub	r3, r1 		! r1 = _etext - _mem_start
	mov.l	11f, r3	
	add	r3, r1		! r1 = _etext - (_mem_start - _rom_store) = src
	
	mov.l	3f, r0		! r0 = end of dest (__bss_start)

14:
	mov.b	@r1, r3
	mov.b	r3, @r2

	add #1, r1
	add #1, r2
	
	cmp/eq r2, r0
	bf 14b

#endif

	!			Initialize Status Register
	mov.l	1f, r0		! MD=1, RB=0, BL=0, IMASK=0xF
	ldc	r0, sr
	!			Initialize global interrupt mask
	mov	#0, r0
	ldc	r0, r6_bank
	!
	mov.l	2f, r0
	mov	r0, r15		! Set initial r15 (stack pointer)
	mov	#0x20, r1	!
	shll8	r1		! r1 = 8192
	sub	r1, r0		!
	ldc	r0, r7_bank	! ... and init_task
	!
	!			Enable cache
	mov.l	6f, r0
	jsr	@r0
	nop

#if defined(CONFIG_SH_SNAPGEAR)
	! (re)set the peripheral bus dividor to 1/2
	mov.l   FRQCR, r1
	mov.w   FRQCR_data, r0
	mov.w   r0, @r1
	! move the romfs to after the bss
	mov.l	fix_romfs, r0
	jsr @r0
#endif
	nop


	!			Clear BSS area
	mov.l	3f, r1
	add	#4, r1
	mov.l	4f, r2
	mov	#0, r0
9:	cmp/hs	r2, r1
	bf/s	9b		! while (r1 < r2)
	mov.l	r0,@-r2

	!			Start kernel
	mov.l	5f, r0
	jmp	@r0
	nop

	.balign 4
1:	.long	0x400080F0		! MD=1, RB=0, BL=0, FD=1, IMASK=0xF
2:	.long	SYMBOL_NAME(stack)
3:	.long	SYMBOL_NAME(__bss_start)
4:	.long	SYMBOL_NAME(_end)
5:	.long	SYMBOL_NAME(start_kernel)
6:	.long	SYMBOL_NAME(cache_init)


#if defined(CONFIG_SH_ROMBOOT)
7:	.long	SYMBOL_NAME(_etext);
11:	.long	SYMBOL_NAME(_rom_store)	
12:	.long	SYMBOL_NAME(_mem_start)
13:	.long	0x1000
#endif

#if defined(CONFIG_SH_SNAPGEAR)
fix_romfs: .long copy_romfs
FRQCR:     .long 0xffc00000
FRQCR_data:.word 0x0e08
           .word 0x0000
#endif
