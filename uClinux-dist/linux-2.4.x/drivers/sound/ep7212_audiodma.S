/*
 * CL-PS7212/empeg Audio Device Driver - "DMA" (fiq!) section
 *
 * (C)2000 empeg ltd, http://www.empeg.com
 *
 * linux/drivers/char/ep7212_audiodma.S
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Authors:
 *   Hugo Fiennes, <hugo@empeg.com>
 *   Umar Qureshey, <uqureshey@cs.ucr.edu>
 *   Petko Manolov, <petkan@nucleusys.com>
 *
 * Version history:
 *   See ep7212_audio.c
 */

#if !defined (__ASSEMBLY__)
#define	__ASSEMBLY__
#endif
#include <linux/linkage.h>
#include <asm/assembler.h>
#define	DO_MONO

		.text

.set		BUFFERS_USED,		0
.set		BUFFERS_FREE,		4
.set		BUFFERS_HEAD,		8
.set		BUFFERS_TAIL,		12
.set		BUFFERS_POINTER,	16
.set		BUFFERS_MODE,		20
.set		BUFFERS_STATE,		24
.set		BUFFERS_ZERO,		28
.set		BUFFERS_VBASE,		32
.set		BUFFERS_COUNT,		32

		.global	SYMBOL_NAME(audio_7212_fiqend)
		.global SYMBOL_NAME(audio_7212_fiq)
		
audio_7212_fiq:
		#   Everything here is designed for the quickest possible
		#   "normal" fiq service, ie when we are filling from a known
		#   buffer
		#   r8  - scratch
	        #   r9  - pointer to audio control structure
		#   r10 - pointer to DAI registers (0xd0002000)
		#   r11 - current player buffer
		#   r12 - number of samples left in buffer
		#   r13	- CPU base address (used to be stack pointer)

		# Do we have data in the buffer?

		cmp	r12,#0
		beq	fiq_nextbuffer
		
newbuffers:	# Read four samples from the play buffer and write them to the
		# DAI FIFO.
#ifdef DO_MONO
		ldr	r8, [r9, #BUFFERS_MODE]
		cmp	r8, #0
		mov	r13, #0
		beq	mono
#endif
		ldr     r8,[r11],#4
		str     r8,[r10,#0x0080]
		mov	r8,r8,lsr#16
		str     r8,[r10,#0x0040]

		ldr     r8,[r11],#4
		str     r8,[r10,#0x0080]
		mov	r8,r8,lsr#16
		str     r8,[r10,#0x0040]

		ldr     r8,[r11],#4
		str     r8,[r10,#0x0080]
		mov	r8,r8,lsr#16
		str     r8,[r10,#0x0040]

		ldr     r8,[r11],#4
		str     r8,[r10,#0x0080]
		mov	r8,r8,lsr#16
		str     r8,[r10,#0x0040]

		# Decrement bytes left count
		sub	r12,r12,#16
mono_out0:
		# Clear RX fifos (euck!)
		ldr	r8,[r10,#0x0080]
		ldr	r8,[r10,#0x0040]
		ldr	r8,[r10,#0x0080]
		ldr	r8,[r10,#0x0040]
		ldr	r8,[r10,#0x0080]
		ldr	r8,[r10,#0x0040]
		ldr	r8,[r10,#0x0080]
		ldr	r8,[r10,#0x0040]

		mvn	r8,#0
		str	r8,[r10,#0x0100]

		# All done, return
		subs	pc,lr,#4

fiq_nextbuffer:

		# Are we supposed to be consuming data yet?
		ldr	r8,[r9,#BUFFERS_STATE]
		cmp	r8,#0
		beq	zerofill

		# Check to see if there are any buffers left
		ldr	r8,[r9,#BUFFERS_USED]
		cmp	r8,#0
		bne	fiq_nextbuffer1

		# No buffers left; we are empty. Suspend playing until user
		# task at least half-fills the audio buffer
		mov	r8,#0
		str	r8,[r9,#BUFFERS_STATE]

zerofill:
		# Load from zero buffer
		ldr	r11,[r9,#BUFFERS_ZERO]
		bal	fiq_newbuffer

fiq_nextbuffer1:
		# Twiddle buffer counts
		sub	r8,r8,#1
		str	r8,[r9,#BUFFERS_USED]
		ldr	r8,[r9,#BUFFERS_FREE]
		add	r8,r8,#1
		str	r8,[r9,#BUFFERS_FREE]

		# Get next buffer
		ldr	r8,[r9,#BUFFERS_TAIL]
		ldr	r11,[r9,#BUFFERS_POINTER]
		add	r11,r11,r8,lsl#12	/* x4096 */
		add	r11,r11,r8,lsl#9	/* x512 = 4608 */
		add	r8,r8,#1
		cmp	r8,#BUFFERS_COUNT
		moveq	r8,#0
		str	r8,[r9,#BUFFERS_TAIL]

fiq_newbuffer:
		# Buffer is now full
		mov	r12,#4608
#ifdef DO_MONO
		ldr	r8, [r9, #BUFFERS_MODE]
		cmp	r8, #0
		mov	r13, #1
		beq	mono
#endif
		ldr     r8,[r11],#4
		str     r8,[r10,#0x0080]
		mov	r8,r8,lsr#16
		str     r8,[r10,#0x0040]

		ldr     r8,[r11],#4
		str     r8,[r10,#0x0080]
		mov	r8,r8,lsr#16
		str     r8,[r10,#0x0040]

		ldr     r8,[r11],#4
		str     r8,[r10,#0x0080]
		mov	r8,r8,lsr#16
		str     r8,[r10,#0x0040]

		ldr     r8,[r11],#4
		str     r8,[r10,#0x0080]
		mov	r8,r8,lsr#16
		str     r8,[r10,#0x0040]

		sub	r12,r12,#16
mono_out1:
		# Clear RX fifos (euck!)
		ldr	r8,[r10,#0x0080]
		ldr	r8,[r10,#0x0040]
		ldr	r8,[r10,#0x0080]
		ldr	r8,[r10,#0x0040]
		ldr	r8,[r10,#0x0080]
		ldr	r8,[r10,#0x0040]
		ldr	r8,[r10,#0x0080]
		ldr	r8,[r10,#0x0040]

		mvn	r8,#0
		str	r8,[r10,#0x0100]

		# Run bottom half (no need to save r4-r11)
		#stmfd	r13!,{r0-r3,r12,r14}
		#mov	r0,r9
		#mov	lr,pc
		#ldr	pc,[r9,#BUFFERS_CALL]
		#ldmia	r13!,{r0-r3,r12,r14}


		#Generate bottom-half IRQ
		ldr	r13, [r9, #BUFFERS_VBASE]
		ldr	r8, [r13, #0x0280]
		orr	r8, r8, #0x100
		str	r8, [r13, #0x0280]
		str	r8, [r13, #0x06C0]
		mov	r8, #1
		str	r8, [r13, #0x0300]
		
		subs	pc,lr,#4
		
mono:
		ldr	r8, [r11], #4
		str	r8, [r10, #0x0080]
		str	r8, [r10, #0x0040]
		mov	r8, r8, lsr #16
		str	r8, [r10, #0x0080]
		str	r8, [r10, #0x0040]
		ldr	r8, [r11], #4
		str	r8, [r10, #0x0080]
		str	r8, [r10, #0x0040]
		mov	r8, r8, lsr #16
		str	r8, [r10, #0x0080]
		str	r8, [r10, #0x0040]

		sub	r12, r12, #8
		cmp	r13, #0
		beq	mono_out0
		b	mono_out1

SYMBOL_NAME(audio_7212_fiqend):
