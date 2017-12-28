/*
 *	arch/microblaze/kernel/exceptions.c - generic HW exception handling
 *
 * Copyright (C) 2005 John Williams <jwilliams@itee.uq.edu.au>
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License.  See the file COPYING in the main directory of this
 * archive for more details.
 *
 * Currently, Microblaze supports a total of 6 HW generated exceptions, all
 * of which may be disabled in the hardware via configuration parameters.
 * Of these, the unaligned access exception is probably the most interesting.
 * It is exception type 1, and is handled directly in a lovely bit of optimised
 * code in hw_exception_handler.S.  All other exceptions are dispatched through
 * the MB_ExceptionVectorTable that is defined and initialised below.
 * Currently, only the unaligned exception is supported - all others trap
 * to null handlers, returning immediately.
 *
 */

#include <asm/exceptions.h>

/* Forward declararation of null exception handler function */
void null_exception_handler(unsigned);

/* Define a null exception vector */
struct mb_exception_vector null_exception_vector={null_exception_handler,0};

/* The actual exception vector table (for all except unaligned exceptions */
struct mb_exception_vector MB_ExceptionVectorTable[NUM_OTHER_EXCEPTIONS];

/* Initialize_exception_handlers() - called from setup.c/trap_init() */
void initialize_exception_handlers(void)
{
	/* For now, just make all the other exception handlers NULL */
	int i;
	for(i=0;i<NUM_OTHER_EXCEPTIONS;i++)
		MB_ExceptionVectorTable[i] = null_exception_vector;

}

/* Default null handler */
void null_exception_handler(unsigned int param)
{
	return;
}

