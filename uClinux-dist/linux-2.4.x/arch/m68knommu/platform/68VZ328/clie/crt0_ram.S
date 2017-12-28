#define IMM #
#define DEBUG(x) \
	moveb	IMM x, %d7; \
	moveb	%d7,0xfffff907; \
    1:  movew	0xfffff906, %d7; \
	andw	IMM 0x2000, %d7; \
	beq	1b



	.global _start
	.global _stext
	.global _ramstart
	.global _ramend
	.global _rambase
	.global _ramvec



	.data
_ramstart:
.long   0
_ramend:
.long   0
_rambase:
.long	0
_ramvec:
.long	0



	.text
_start:
_stext:



	movew	#0x2700, %sr
        moveb	#0x00,   0xfffffb0b	/* Watchdog off */
        moveb	#0x00,   0xfffff000	/* SCR */
        movew	#0x2483, 0xfffff200	/* PLLCR */
        movew	#0x0347, 0xfffff202	/* PLLFSR */
	movew	#32767, %d0		/* PLL settle wait loop */
plllp:	subq	#1, %d0
	bne	plllp



	/* UART1 setup */
	orib	#4,      0xfffff431	/* Magic taken from PalmOS ROM. I'm gonna think this pin is in GPIO mode, even though I can't prove that */
	moveb	#0xcf,   0xfffff423	/* Dedicated IO */
	moveb	#0x08,   0xfffff906	/* Transmitter settings */
	movew	#0x0080, 0xfffff908	/* Misc settings */
	movew	#0x326,  0xfffff902	/* Baud rate */
	movew	#0xe100, 0xfffff900	/* Enable */



	/* Make sure console works */
	DEBUG('A');



        /* My parts are scattered, %a0 has a list.  Make me whole */
	DEBUG('B');
	moveal	#0x5000, %a2		/* FIXME: Hard coded load address */
	moveal	%a0@+, %a1		/* Move to page 1 (this is page 0) */
mvlp0:
	movew	#1024, %d6
	moveal	%a0@+, %a1
	movel	%a1, %d0
	beq	mvdone
mvlp1:
	movel	%a1@+, %d0
	movel	%d0, %a2@+
	subw	#1, %d6
        bne	mvlp1
        bra	mvlp0
mvdone:



	/* Stack */
	moveal	#__ramend - 0x10, %sp



	/* Move romfs beyond BSS */
	DEBUG('C')
	moveal	#_sbss, %a0
	moveal	#_ebss, %a1
	movel	%a0, %a2

	movel	%a0@(8), %d0		/* romfs size */
	addl	#8, %d0			/* plus header */
        andl	#0xfffffffc, %d0	/* Whole words */
	addl	%d0, %a0
	addl	%d0, %a1
	movel	%a1, _ramstart		/* set to the END of romfs, not _ebss! */

romlp:  movel	%a0@-, %d0		/* Copy dword */
        movel	%d0, %a1@-
        cmpl	%a0, %a2		/* Check if at end */
        bne	romlp



	/* Clear bss  */
	DEBUG('D');
        moveal	#_sbss, %a0
        moveal	#_ebss, %a1
        /* Copy 0 to %a0 until %a0 == %a1 */
bsslp:	clrl    %a0@+
        cmpal   %a0, %a1
        bne     bsslp



	/* Fill vars */
	DEBUG('E')
	movel   #_sdata, %d0
	movel   %d0, _rambase
	movel   #__ramend, %d0
	movel   %d0, _ramend
        movel   #__ramvec, %d0
	movel   %d0, _ramvec



	/* Load the current task pointer and stack */
	DEBUG('F')
        lea	init_task_union, %a0
        movel	%a0, _current_task
        lea	0x2000(%a0), %sp

 

	/* Start kernel */
        DEBUG('G')
        DEBUG('\n')
        DEBUG('\r')
stlp:   jsr	start_kernel
        DEBUG('!')
        bra stlp
 


_exit:
	jmp	_exit

	.data
env:
	.long   0
	.text
