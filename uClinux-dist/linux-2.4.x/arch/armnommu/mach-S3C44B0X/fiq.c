/**
 * arch/armnommu/mach-S3C44B0X/fiq.c
 *
 * Optimized handler for one FIQ handler, with minimized latency
 *
 * Copyright (c) 2004 by Thomas Eschenbacher <thomas.eschenbacher@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/config.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/arch/irq.h>
#include <asm/arch/fiq.h>
#include <asm/arch/irqs.h>
#include <asm-armnommu/ioctls.h>
#include <asm/ioctl.h>

/** start address of the RAM */
#define RAM_START 0x0C000000

/* defined in arch/armnommu/kernel/fiq.c */
extern void set_fiq_handler(void *start, unsigned int length);

/** list of FIQ handler */
static struct {
	u_int32_t mask;        /* bitmask for checking INTPND */
	void (*handler)(void); /* handler */
} _fiq_table[NR_IRQS+1];

/** Stack for the FIQ handler, 1kB should be enough */
static unsigned char _fiq_stack[S3C44B0X_SIMPLE_FIQ_STACK_SIZE]
        __attribute__ ((aligned(4)));

/****************************************************************************/
/**
 * dummy FIQ handler, should never be called
 */
static void _unknown_fiq_handler(void)
{
}

/****************************************************************************/
/**
 * Calculates the ARM32 opcode for a branch instruction
 * @param from the address where the instruction will be executed from
 * @param to the target address of the branch
 */
static inline unsigned int _opcode_arm32_branch_relative(unsigned int from,
                                                         unsigned int to)
{
	if (from < to) {
		return 0xEA000000 + ((to - (from+8)) >> 2);
	} else {
		return 0xEAFFFFFE - ((from - to) >> 2); /* <- untested !!! */
	}
}

/****************************************************************************/
/**
 * Wrapper for fast FIQ handling. Saves/restores the user space registers
 * and executes the handler that is specified in the _fiq_handler table
 * @note needs only 10 words == 40 bytes stack
 */
void __attribute__ ((naked)) _fiq_wrapper(void)
{
	asm("\
	mov	r8, lr							\n\
	stmdb	sp!, {r0-r8}	/* save unbanked R0...R7+R8 */		\n\
    3:									\n\
	ldr	r0, %[INTPND]						\n\
	ldmia	r0, {r0-r2}	/* R0=INTPND, R1=INTMOD, R2=INTMSK */	\n\
	and	r0, r0, r1	/* ignore IRQs */			\n\
	bic	r0, r0, r2	/* ignore masked interrupts */		\n\
									\n\
	cmp	r0, #0		/* sanity check: no pending FIQ ? */	\n\
	beq	2f							\n\
									\n\
	/* seek for the FIQ handler through the bitmask in R1 	*/	\n\
	/* R0 = bitmask with pending FIQ */				\n\
	/* R1 = bitmask for checking */					\n\
	/* R2 = handler */						\n\
	/* R3 = working register */					\n\
	/* R4 = pointer to handler table */				\n\
	ldr 	r4, %[_fiq_table]					\n\
    1:									\n\
	ldmia	r4!, {r1, r2}						\n\
	and	r3, r0, r1						\n\
	cmp	r3, r1							\n\
	bne	1b							\n\
									\n\
	ldr	r3, %[INTACK]	/* acknowledge the FIQ */		\n\
	str	r1, [r3]						\n\
									\n\
	mov	lr, pc							\n\
	mov	pc, r2          /* and execute the handler */		\n\
	b 	3b							\n\
    2:									\n\
	ldmia	sp!, {r0-r8}    /* restore unbanked R0...R7+R8 */	\n\
	mov	lr, r8							\n\
	subs	pc, r14, #4	/* return from FIQ, restore SPSR */	\n\
	"
	:
	: [_fiq_table] "m" (_fiq_table),
	  [INTPND] "m" (S3C44B0X_INTPND),
	  [INTACK] "m" (S3C44B0X_F_ISPC)
	: "memory"
	);
}

/****************************************************************************/
static inline void _set_fiq_stack(unsigned int fiq_sp)
{
	asm("\
	mov	r3, %[fiq_sp]					\n\
	mrs	r2, cpsr	/* switch to FIQ mode */	\n\
	bic	r1, r2, #0x1F	/* CPSR_MODE_BITS */		\n\
	orr	r1, r1, #0x11	/* CPSR_FIQ_MODE */		\n\
	msr	cpsr, r1					\n\
	bic	sp,r3,#0x03	/* sp is passed in R0 */	\n\
	msr	cpsr,r2		/* restore previous CPU mode */	\n\
	"
	:
	: [fiq_sp] "r" (fiq_sp)
	: "r1", "r2", "r3"
	);
}

/****************************************************************************/
/**
 * Installs a FIQ handler for fastest possible interrupt handling.
 *
 * @param interrupt number of the interrupt source
 *        [0...NR_IRQS-1]
 * @param priority the priority of the FIQ, from 0
 *        to NR_IRQS-1, 0 is the highest priority
 * @param handler pointer to the ISR that handles the FIQ
 */
void install_fiq_handler(unsigned int interrupt, unsigned int priority,
                         void (*handler)(void))
{
	int nr;

	/* sanity check: interrupt must be [0...25] */
	if (interrupt >= NR_IRQS) return;
	if (priority >= NR_IRQS)  return;

	/* disable global interrupts and out IRQ through INTMSK */
	outl(inl(S3C44B0X_INTMSK) |
	    ((1 << 26) | (1 << interrupt)), S3C44B0X_INTMSK);

	/* make sure that it is not already installed */
	uninstall_fiq_handler(interrupt, priority);
	    
	/* move all FIQs with lower priority one entry upwards */
	for (nr=NR_IRQS-1; nr > priority; nr--)
    	{
		_fiq_table[nr] = _fiq_table[nr-1];
	}

	/* enter the address of the handler into the list */
	_fiq_table[priority].mask    = 0;
	_fiq_table[priority].handler = handler;
	_fiq_table[priority].mask    = (1 << interrupt);

	/* disable everything through INTCON */
	outl(0x07, S3C44B0X_INTCON);

	/* clear pending IRQ and FIQ bits */
	s3c44b0x_clear_pb(interrupt);

	/* configure interrupt source to FIQ mode */
	outl(inl(S3C44B0X_INTMOD) | (1 << interrupt), S3C44B0X_INTMOD);

	/* enable everything in INTCON */
	outl(0x4, S3C44B0X_INTCON);

	/* allow global interrupts again */
	outl(inl(S3C44B0X_INTMSK) & ~(1 << 26), S3C44B0X_INTMSK);
}

/****************************************************************************/
/**
 * Uninstalls a FIQ handler.
 *
 * @param interrupt number of the interrupt source
 *        [0...NR_IRQS-1]
 * @param priority the priority of the FIQ, from 0
 *        to NR_IRQS-1, 0 is the highest priority
 */
void uninstall_fiq_handler(unsigned int interrupt, unsigned int priority)
{
	int nr, i;
	(void)priority;
	
	/* sanity check: interrupt must be [0...25] */
	if (interrupt >= NR_IRQS) return;

	/* disable global interrupts and out IRQ through INTMSK */
	outl(inl(S3C44B0X_INTMSK) |
	    ((1 << 26) | (1 << interrupt)), S3C44B0X_INTMSK);

	/* move all FIQs with lower priority one entry downwards */
	for (i=0; i < NR_IRQS; i++)
	{
		if (_fiq_table[i].mask == (1 << interrupt))
		{
			for (nr = i; nr < NR_IRQS; nr++)
			{
				_fiq_table[nr] = _fiq_table[nr+1];
			}
			break;
		}
	}

	/* disable everything through INTCON */
	outl(0x07, S3C44B0X_INTCON);

	/* clear pending IRQ and FIQ bits */
	s3c44b0x_clear_pb(interrupt);

	/* configure interrupt source to IRQ mode */
	outl(inl(S3C44B0X_INTMOD) & ~(1 << interrupt), S3C44B0X_INTMOD);

	/* enable everything in INTCON */
	outl(0x4, S3C44B0X_INTCON);

	/* allow global interrupts again */
	outl(inl(S3C44B0X_INTMSK) & ~(1 << 26), S3C44B0X_INTMSK);
}

/****************************************************************************/
void s3c44b0x_init_FIQ_handlers(void)
{
	unsigned int branch;
	unsigned int i;

	/* set up the FIQ stack */
	_set_fiq_stack(((unsigned int)&_fiq_stack) + sizeof(_fiq_stack));

	/* note: the entry at NR_IRQS is used if no irq has been found */
	for (i=0; i <= NR_IRQS; i++)
	{
		_fiq_table[i].mask    = 0xFFFFFFFF;
		_fiq_table[i].handler = (void *)(&_unknown_fiq_handler);
	}

	branch = _opcode_arm32_branch_relative(
	    0x0C00001C, (unsigned int)&_fiq_wrapper);
	*((unsigned int *)(0x0C00001C)) = branch;

	/*
	 * @todo
	 * reconfigure priority registers; otherwise it may happen that
	 * IRQ and FIQ arise at the same time and the IRQ will be served as FIQ
	 * as the SAMSUNG interrupt controller only uses priorities to
	 * decide which instruction to write to 0x18 and 0x1C
	 */
}

EXPORT_SYMBOL(install_fiq_handler);
EXPORT_SYMBOL(uninstall_fiq_handler);

/****************************************************************************/
/****************************************************************************/
