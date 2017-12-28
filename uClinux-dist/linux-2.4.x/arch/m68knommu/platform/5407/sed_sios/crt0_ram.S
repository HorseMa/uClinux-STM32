/*****************************************************************************/

/*
 *	crt0_ram.S -- startup code for Motorola 5407 eval board.
 *
 *	(C) Copyright 1999-2002, Greg Ungerer (gerg@snapgear.com).
 *	(C) Copyright 2000, Lineo (www.lineo.com).
 *
 * 	1999/02/24 Modified for the 5307 processor David W. Miller
 */

/*****************************************************************************/

#include "linux/autoconf.h"
#include "asm/coldfire.h"
#include "asm/mcfsim.h"

/*****************************************************************************/

/*
 *	Board setup info.
 */
#define	MEM_BASE		0x00000000	/* Memory base at address 0 */
#define	VBR_BASE		MEM_BASE	/* Vector address */
#define MEM_SIZE		0x01000000
#define INT_RAM_SUB_SIZE	2048
#define INT_RAM_SIZE		4096
#define MCF_MBAR		0x10000000

/*****************************************************************************/

.global	_start
.global _rambase
.global _ramvec
.global	_ramstart
.global	_ramend
.global __init_begin
.global __init_end
.global __kernel_init_image_start

/*****************************************************************************/

.data

/*
 *	Set up the usable of RAM stuff. Size of RAM is determined then
 *	an initial stack set up at the end.
 */
_rambase:
.long	0
_ramvec:
.long	0
_ramstart:
.long	0
_ramend:
.long	0

/*****************************************************************************/

.text
.section ".text.bootrom","awx"
/*
 *	This is the codes first entry point. This is where it all
 *	begins...
 */

_start:
	nop					/* Filler */
	move.w	#0x2700, %sr			/* No interrupts */

	/* Setup internal ram as the stack and for the internalsram section */
	move.l  #(MEM_SIZE+1),      %d0
	move.c  %d0,            %RAMBAR0
	move.l  #(MEM_SIZE+INT_RAM_SUB_SIZE+1), %d0
	move.c  %d0,            %RAMBAR1
	move.l  #(MEM_SIZE+INT_RAM_SIZE), %d0
	move.l  %d0,    %a7

	/* Setup the MBAR */
	move.l  #(MCF_MBAR+1),  %d0
	move.c  %d0,        %MBAR


	/* Setup CS2 - CPLD registers */
	move.w  #0x8000,	%d0
	move.w  %d0,		MCF_MBAR+MCFSIM_CSAR2
	move.w  #0x0d00,	%d0
	move.w  %d0,		MCF_MBAR+MCFSIM_CSCR2
	move.l  #0x00000037,	%d0
	move.l  %d0,		MCF_MBAR+MCFSIM_CSMR2

	/* Setup CS1 - External UART */
	move.w  #0x8002,	%d0
	move.w  %d0,		MCF_MBAR+MCFSIM_CSAR1
	move.w  #0x0040,	%d0
	move.w  %d0,		MCF_MBAR+MCFSIM_CSCR1
	move.l  #0x00000037,	%d0
	move.l  %d0,		MCF_MBAR+MCFSIM_CSMR1

	/* Setup CS3 - Ethernet */
	move.w  #0x8003,	%d0
	move.w  %d0,		MCF_MBAR+MCFSIM_CSAR3
	move.w  #0x0020,	%d0
	move.w  %d0,		MCF_MBAR+MCFSIM_CSCR3
	move.l  #0x00000037,	%d0
	move.l  %d0,		MCF_MBAR+MCFSIM_CSMR3

	/* Setup CS6 - MUX CPLD */
	move.w	#0x8001,	%d0
	move.w	%d0,		MCF_MBAR+MCFSIM_CSAR6
	move.w	#0x0040,	%d0
	move.w  %d0,		MCF_MBAR+MCFSIM_CSCR6
	move.l  #0x00000037,	%d0
	move.l  %d0,		MCF_MBAR+MCFSIM_CSMR6

	/*
	 *  Setup VBR as per eval board (really dBUG does this).
	 *  These settings must match it.
	 */
	move.l  #VBR_BASE, %a0          /* Note VBR can't be read */
	movec   %a0, %VBR
	move.l  %a0, _ramvec            /* Set up vector addr */
	move.l  %a0, _rambase           /* Set up base RAM addr */

	/*
	 *  Zero out the bss region.
	 */
	lea.l   _sbss,  %a0         /* Get start of bss */
	move.l  #_ebss, %d0
	add.l   #0x10,  %d0
	move.l  %d0,    %a1         /* Get end of bss */
	move.l  #0x00,  %d0
_clear_bss:
	move.l  %d0, (%a0)+         /* Clear each word */
	cmp.l   %a0, %a1            /* Check if at end */
	bne _clear_bss

	/*
	 * Setup Memory size inforamtion, do it after the copy loops. If I
	 * do it before the copy loops, stuff gets clobbered bad
	 */
	move.l  #MEM_SIZE, %d0
	move.l  %d0, _ramend                    /* Set end ram addr */
	move.l  #VBR_BASE, %d0                  /* Note VBR can't be read */
	move.l  %d0, _ramvec                    /* Set up vector addr */
	move.l  %d0, _rambase                   /* Set up base RAM addr */
	move.l  #_ebss, %d0
	add.l   #0x10,  %d0
	move.l  %d0,    _ramstart

	move.l  #MEM_SIZE,  %a7

	/*
	 * Configure the external interrupts. Currently I use interrupt vectors
	 * 4 and 7. Set all external interrupts to be autovectored. Set the
	 * actual interrupt pins to generate interrupt levels 4, 3, 1. Note:
	 * the parallel port is only used for parallel port, not alternate
	 * functions.
	 */
	move.b  #0xfe,  %d0
	move.b  %d0,    MCF_MBAR+MCFSIM_AVR
	move.b  #0x80,  %d0
	move.b  %d0,    MCF_MBAR+MCFSIM_IRQPAR

	/*
	 *      Enable CPU internal cache.
	 */
#if ENABLE_CACHE
	move.l  #0x01040100, %d0                /* Invalidate whole cache */
	movec   %d0,%CACR
	nop
	move.l  #ACR0_VAL, %d0
	movec   %d0, %ACR0
	move.l  #ACR1_VAL, %d0
	movec   %d0, %ACR1
	move.l  #ACR2_VAL, %d0
	movec   %d0, %ACR2
	move.l  #ACR3_VAL, %d0
	movec   %d0, %ACR3

	move.l  #CACR_VAL, %d0
	movec   %d0,%CACR
#endif

	/*
	 *	Load the current task pointer and stack.
	 */
	lea	init_task_union, %a0
	movel	%a0, _current_task
	lea	0x2000(%a0), %sp

	/*
	 *	Assember start up done, start code proper.
	 */
	jsr	start_kernel			/* Start Linux kernel */
dbug:
_exit:
	jmp	_exit				/* Should never get here */

/*****************************************************************************/
.section ".data.initvect","awx"
	.long   0	/* Reset: Initial Stack Pointer                 - 0.  */
	.long   _start	/* Reset: Initial Program Counter               - 1.  */
	.long   dbug	/* Bus Error                                    - 2.  */
	.long   dbug	/* Address Error                                - 3.  */
	.long   dbug	/* Illegal Instruction                          - 4.  */
	.long   dbug	/* Divide by zero                               - 5.  */
	.long   dbug	/* CHK, CHK2 Instructions                       - 6.  */
	.long   dbug	/* TRAPcc, TRAPV Instructions                   - 7.  */
	.long   dbug	/* Privilege Violation                          - 8.  */
	.long   dbug	/* Trace                                        - 9.  */
	.long   dbug        /* Line 1010 Emulator                           - 10. */
	.long   dbug        /* Line 1111 Emualtor                           - 11. */
	.long   dbug        /* Harware Breakpoint                           - 12. */
	.long   dbug        /* (Reserved for Coprocessor Protocol Violation)- 13. */
	.long   dbug        /* Format Error                                 - 14. */
	.long   dbug        /* Uninitialized Interrupt                      - 15. */
	.long   dbug        /* (Unassigned, Reserver)                       - 16. */
	.long   dbug        /* (Unassigned, Reserver)                       - 17. */
	.long   dbug        /* (Unassigned, Reserver)                       - 18. */
	.long   dbug        /* (Unassigned, Reserver)                       - 19. */
	.long   dbug        /* (Unassigned, Reserver)                       - 20. */
	.long   dbug        /* (Unassigned, Reserver)                       - 21. */
	.long   dbug        /* (Unassigned, Reserver)                       - 22. */
	.long   dbug        /* (Unassigned, Reserver)                       - 23. */
	.long   dbug        /* Spurious Interrupt                           - 24. */
	.long   dbug        /* Level 1 Interrupt Autovector                 - 25. */
	.long   dbug        /* Level 2 Interrupt Autovector                 - 26. */
	.long   dbug        /* Level 3 Interrupt Autovector                 - 27. */
	.long   dbug        /* Level 4 Interrupt Autovector                 - 28. */
	.long   dbug        /* Level 5 Interrupt Autovector                 - 29. */
	.long   dbug        /* Level 6 Interrupt Autovector                 - 30. */
	.long   dbug        /* Level 7 Interrupt Autovector                 - 31. */
	.long   system_call /* dbug Instruction Vectors 0                   - 32. */
	.long   dbug        /* Trap Instruction Vectors 1                   - 33. */
	.long   dbug        /* Trap Instruction Vectors 2                   - 34. */
	.long   dbug        /* Trap Instruction Vectors 3                   - 35. */
	.long   dbug        /* Trap Instruction Vectors 4                   - 36. */
	.long   dbug        /* Trap Instruction Vectors 5                   - 37. */
	.long   dbug        /* Trap Instruction Vectors 6                   - 38. */
	.long   dbug        /* Trap Instruction Vectors 7                   - 39. */
	.long   dbug        /* Trap Instruction Vectors 8                   - 40. */
	.long   dbug        /* Trap Instruction Vectors 9                   - 41. */
	.long   dbug        /* Trap Instruction Vectors 10                  - 42. */
	.long   dbug        /* Trap Instruction Vectors 11                  - 43. */
	.long   dbug        /* Trap Instruction Vectors 12                  - 44. */
	.long   dbug        /* Trap Instruction Vectors 13                  - 45. */
	.long   dbug        /* Trap Instruction Vectors 14                  - 46. */
	.long   dbug        /* Trap Instruction Vectors 15                  - 47. */
	.long   0           /* (Reserved for Coprocessor)                   - 48. */
	.long   0           /* (Reserved for Coprocessor)                   - 49. */
	.long   0           /* (Reserved for Coprocessor)                   - 50. */
	.long   0           /* (Reserved for Coprocessor)                   - 51. */
	.long   0           /* (Reserved for Coprocessor)                   - 52. */
	.long   0           /* (Reserved for Coprocessor)                   - 53. */
	.long   0           /* (Reserved for Coprocessor)                   - 54. */
	.long   0           /* (Reserved for Coprocessor)                   - 55. */
	.long   0           /* (Reserved for Coprocessor)                   - 56. */
	.long   0           /* (Reserved for Coprocessor)                   - 57. */
	.long   0           /* (Reserved for Coprocessor)                   - 58. */
	.long   0           /* (Unassigned, Reserved)                       - 59. */
	.long   0           /* (Unassigned, Reserved)                       - 60. */
	.long   0           /* (Unassigned, Reserved)                       - 61. */
	.long   0           /* (Unassigned, Reserved)                       - 62. */
	.long   0           /* (Unassigned, Reserved)                       - 63. */
	.long   0           /* (User-Defined Vectors 1) CPM Error           - 64. */
	.long   0           /* (User-Defined Vectors 2) CPM Parallel IO PC11- 65. */
	.long   0           /* (User-Defined Vectors 3) CPM Parallel IO PC10- 66. */
	.long   0           /* (User-Defined Vectors 4) CPM SMC2 / PIP      - 67. */
	.long   0           /* (User-Defined Vectors 5) CPM SMC1            - 68. */
	.long   0           /* (User-Defined Vectors 6) CPM SPI             - 69. */
	.long   0           /* (User-Defined Vectors 7) CPM Parallel IO PC9 - 70. */
	.long   0           /* (User-Defined Vectors 8) CPM Timer 4         - 71. */
	.long   0           /* (User-Defined Vectors 9) CPM Reserved        - 72. */
	.long   0           /* (User-Defined Vectors 10) CPM Parallel IO PC8- 73. */
	.long   0           /* (User-Defined Vectors 11) CPM Parallel IO PC7- 74. */
	.long   0           /* (User-Defined Vectors 12) CPM Parallel IO PC6- 75. */
	.long   0           /* (User-Defined Vectors 13) CPM Timer 3        - 76. */
	.long   0           /* (User-Defined Vectors 14) CPM Reserved       - 77. */
	.long   0           /* (User-Defined Vectors 15) CPM Parallel IO PC5- 78. */
	.long   0           /* (User-Defined Vectors 16) CPM Parallel IO PC4- 79. */
	.long   0           /* (User-Defined Vectors 17) CPM Reserved       - 80. */
	.long   0           /* (User-Defined Vectors 18) CPM RISC Timer Tbl - 81. */
	.long   0           /* (User-Defined Vectors 19) CPM Timer 2        - 82. */
	.long   0           /* (User-Defined Vectors 21) CPM Reserved       - 83. */
	.long   0           /* (User-Defined Vectors 22) CPM IDMA2          - 84. */
	.long   0           /* (User-Defined Vectors 23) CPM IDMA1          - 85. */
	.long   0           /* (User-Defined Vectors 24) CPM SDMA Bus Err   - 86. */
	.long   0           /* (User-Defined Vectors 25) CPM Parallel IO PC3- 87. */
	.long   0           /* (User-Defined Vectors 26) CPM Parallel IO PC2- 88. */
	.long   0           /* (User-Defined Vectors 27) CPM Timer 1        - 89. */
	.long   0           /* (User-Defined Vectors 28) CPM Parallel IO PC1- 90. */
	.long   0           /* (User-Defined Vectors 29) CPM SCC 4          - 91. */
	.long   0           /* (User-Defined Vectors 30) CPM SCC 3          - 92. */
	.long   0           /* (User-Defined Vectors 31) CPM SCC 2          - 93. */
	.long   0           /* (User-Defined Vectors 32) CPM SCC 1          - 94. */
	.long   0           /* (User-Defined Vectors 33) CPM Parallel IO PC0- 95. */
	/*                  I don't think anything uses the vectors after here.   */
	.long   0           /* (User-Defined Vectors 34)                    - 96. */
	.long   0,0,0,0,0               /* (User-Defined Vectors 35  -  39). */
	.long   0,0,0,0,0,0,0,0,0,0     /* (User-Defined Vectors 40  -  49). */
	.long   0,0,0,0,0,0,0,0,0,0     /* (User-Defined Vectors 50  -  59). */
	.long   0,0,0,0,0,0,0,0,0,0     /* (User-Defined Vectors 60  -  69). */
	.long   0,0,0,0,0,0,0,0,0,0     /* (User-Defined Vectors 70  -  79). */
	.long   0,0,0,0,0,0,0,0,0,0     /* (User-Defined Vectors 80  -  89). */
	.long   0,0,0,0,0,0,0,0,0,0     /* (User-Defined Vectors 90  -  99). */
	.long   0,0,0,0,0,0,0,0,0,0     /* (User-Defined Vectors 100 - 109). */
	.long   0,0,0,0,0,0,0,0,0,0     /* (User-Defined Vectors 110 - 119). */
	.long   0,0,0,0,0,0,0,0,0,0     /* (User-Defined Vectors 120 - 129). */
	.long   0,0,0,0,0,0,0,0,0,0     /* (User-Defined Vectors 130 - 139). */
	.long   0,0,0,0,0,0,0,0,0,0     /* (User-Defined Vectors 140 - 149). */
	.long   0,0,0,0,0,0,0,0,0,0     /* (User-Defined Vectors 150 - 159). */
	.long   0,0,0,0,0,0,0,0,0,0     /* (User-Defined Vectors 160 - 169). */
	.long   0,0,0,0,0,0,0,0,0,0     /* (User-Defined Vectors 170 - 179). */
	.long   0,0,0,0,0,0,0,0,0,0     /* (User-Defined Vectors 180 - 189). */
	.long   0,0,0                   /* (User-Defined Vectors 190 - 192). */
