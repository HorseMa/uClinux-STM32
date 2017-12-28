/**
 * arch/armnommu/mach-S3C3410/fast_fiq.c
 * 
 * Optimized handler for one FIQ handler, with minimized latency
 *
 * Copyright (c) 2003 by Thomas Eschenbacher <thomas.eschenbacher@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * TODO: merge changes from S3C44B0X, this version might be quite buggy !
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/arch/irq.h>
#include <asm-armnommu/ioctls.h>
#include <asm/ioctl.h>

/** Stack for the FIQ handler, 1kB should be enough */
static unsigned char _fiq_stack[S3C3410_SIMPLE_FIQ_STACK_SIZE]
	__attribute__ ((aligned(4)));
        
/** Interrupt handler table can be found in hal_platform_setup.h 
    it is located at 0x100 */

/**
 * Wrapper for fast FIQ handling. Saves/restores the user space registers
 * and executes the handler that is specified at address 0x3C.
 * @note needs only 10 words == 40 bytes stack
 * @note only one FIQ handler is allowed in this system !
 */
void __attribute__ ((naked)) _fiq_wrapper(void)
{
    /* original */
#if 0
    asm("\
	mov	r8,lr							\n\
	stmdb 	sp!,{r0-r8}     /* save unbanked R0...R7+R8 */ 		\n\
        mov	r0, #0x1C	/* get the IRQ number */		\n\
	ldr	r0, [r0]						\n\
        and	r0, r0, #0x7F						\n\
	mov	r0, r0, lsr #2						\n\
        stmdb	sp!,{r0}	/* store IRQ number */			\n\
        bl	s3c3410_clear_pb        				\n\
        mov     r3, #0x100                                              \n\
        add     r3, r3, r0, LSL #2                                      \n\
	/*mov	r3, #0x3C*/	/* get the handler's address */		\n\
	ldr	r3, [r3]						\n\
	mov	lr, pc							\n\
	mov	pc, r3		/* and execute the handler */		\n\
	ldmia	sp!,{r0}	/* restore IRQ number */		\n\
	/*mov	lr, pc*/	/* ACK the interrupt */			\n\
        ldmia	sp!,{r0-r8}     /* restore unbanked R0...R7+R8 */ 	\n\
	mov	lr,r8							\n\
        subs	pc,r14,#4	/* return from FIQ, restore SPSR */ 	\n\
    ");
    */
#else    
    /* debug */
    /* r9 -> LED address
       r10 -> LED value
       r11 -> stack check
    */
    register unsigned int addr asm("r9") = 0x7ff2c2e;
    asm("\
        mov     r11, #0xe5                                              \n\
        mov     r11, r11, lsl #8                                        \n\
        add     r11, r11, #0xcc                                         \n\
        cmp     r11, sp                                                 \n\
        beq     stack_ok                                                \n\
        mov     r0, r0                                                  \n\
        stack_ok:                                                       \n\
	mov	r8,lr							\n\
	stmdb 	sp!,{r0-r8}     /* save unbanked R0...R7+R8 */ 		\n\
        mov	r0, #0x1C	/* get the IRQ number */		\n\
	ldr	r0, [r0]						\n\
        and	r0, r0, #0x7F						\n\
	mov	r0, r0, lsr #2						\n\
        stmdb	sp!,{r0}	/* store IRQ number */			\n\
        cmp     r0, #2  /* rx int */                                    \n\
        moveq   r10, #0x80                                              \n\
        cmp     r0, #5  /* dma0 (tx) int */                             \n\
        moveq   r10, #0x40                                              \n\
        cmp     r0, #12 /* timer int */                                 \n\
        moveq   r10, #0x20                                              \n\
        strb    r10, [r9]                                               \n\
        bl	s3c3410_clear_pb         				\n\
        mov     r3, #0x100                                              \n\
        add     r3, r3, r0, LSL #2                                      \n\
	ldr	r3, [r3]						\n\
	mov	lr, pc							\n\
	mov	pc, r3		/* and execute the handler */		\n\
	ldmia	sp!,{r0}	/* restore IRQ number */		\n\
	/*mov	lr, pc*/	/* ACK the interrupt */			\n\
        ldmia	sp!,{r0-r8}     /* restore unbanked R0...R7+R8 */ 	\n\
	mov	lr,r8							\n\
	mov     r10, #0  /* reset LED */                                \n\
        strb    r10, [r9]                                               \n\
        cmp     r11, sp                                                 \n\
        beq     stack_ok2                                               \n\
        mov     r0, r0                                                  \n\
        stack_ok2:                                                      \n\
        subs	pc,r14,#4	/* return from FIQ, restore SPSR */ 	\n\
    " : : "r" (addr));
#endif
}

/****************************************************************************/
static inline void set_fiq_stack(unsigned int fiq_sp)
{
    asm("\
        mov     r3,%[fiq_sp]						\n\
	mrs     r2,cpsr                 /* switch to FIQ mode */ 	\n\
	bic     r1,r2,#0x1F /* CPSR_MODE_BITS */			\n\
	orr     r1,r1,#0x11 /* CPSR_FIQ_MODE */				\n\
	msr     cpsr,r1							\n\
	bic	sp,r3,#0x03		/* sp is passed in R0 */	\n\
	msr	cpsr,r2			/* restore previous CPU mode */ \n\
    "
    :
    : [fiq_sp] "r" (fiq_sp)
    : "r1", "r2", "r3"
    );
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
 * Installs a FIQ handler for fastest possible interrupt handling.
 *
 * @param interrupt number of the interrupt source (irq number)
 * @param handler function pointer to a C function to handle the FIQ,
 *                does not need to acknowledge the IRQ.
 * @param prio    priority of the interrupt. Attention use the lowest
 *                numbers for FIQ. Otherwise it may happen that an IRQ
 *                is executed as FIQ!!!! The priorities of IRQ must all be
 *                lower (higher value) than that of the IRQs
 */
void install_fiq_handler(unsigned int interrupt, void (*handler)(void), 
                         unsigned char new_prio)
{
    unsigned int i;
    unsigned char from, to, prev; /* other priorities to change */
    signed char diff;
    unsigned char old_prio = new_prio;

    /* set up the FIQ stack */
    set_fiq_stack(((unsigned int)&_fiq_stack) + sizeof(_fiq_stack));

    /* patch a branch instruction to the FIQ wrapper into the  */
    /* proper position in the vector table that starts at 0x80 */
    *(volatile unsigned int*)(0x80 + (interrupt << 2)) =
        _opcode_arm32_branch_relative(0x80+(interrupt << 2),
        (unsigned int)&_fiq_wrapper);

    /* enter the address of the handler into the vector at 0x3C */
    *(volatile unsigned int*)(0x100 + (interrupt << 2)) = (unsigned int)handler;
    /**(volatile unsigned int*)(0x3C) = (unsigned int)handler;*/

    /* configure interrupt source to FIQ mode */
    outl(inl(S3C3410X_INTMOD) | (1 << interrupt), S3C3410X_INTMOD);
    
    /* reconfigure priority registers; otherwise it may happen that
       IRQ and FIQ arise at the same time and the IRQ will be served as FIQ
       as the SAMSUNG interrupt controller only uses priorities to
       decide which instruction to write to 0x18 and 0x1C
    */

    /* search previous priority */
    for (i=0; i < 32; i++)
    {
        if (((inl(S3C3410X_INTPRI0+(i & ~0x3)) >> ((i%4)<<3)) & 0x1F) == 
	    interrupt)
        {
            old_prio = i;
            break;
        }
    }

    if (new_prio == old_prio)
        return; /* nothing to do */

    /* calc which priorities of other interrupts
       have to be changed (from 'from' to 'to') and
       in which direction we have to move ('diff').
    */
    if (new_prio < old_prio)
    {
        from = new_prio;
        to = old_prio;
        diff = +1;
    }
    else
    {
        from = old_prio;
        to = new_prio;
        diff = -1;
    }

    /* update priorities of other interrupts */        
    prev = interrupt;
    for (i=from; i <= to; i+=diff)
    {
        unsigned int temp = inl(S3C3410X_INTPRI0+(i&~0x3));
        unsigned char actual = (temp >> ((i%4)<<3)) & 0x1F;
        temp &= ~(0x1F << ((i%4)<<3)); /* mask */
        temp |= prev << ((i%4)<<3);
        outl(temp, S3C3410X_INTPRI0+(i&~0x3));
        prev = actual;
    }
}

/****************************************************************************/
/**
 * Returns the highest used IRQ (not FIQ) priority (the lowest value).
 *
 * @return the highest used IRQ priority (lowest value)
 */
unsigned char get_highest_irq_prio(void)
{
    unsigned int i, j, prio_reg, int_mode, count = 0;
    
    int_mode = inl(S3C3410X_INTMOD);
    for (i=0; i < 8; i++)
    {
        prio_reg = inl(S3C3410X_INTPRI0+(i<<2));
        for (j=0; j < 4; j++)
        {
             if (!(int_mode & (1 << count))) /* if IRQ (not FIQ) */
             {
                 return ((prio_reg >> (j<<3)) & 0x1F);
             }
             count++;
        }
    }
    
    return 0; /* we should never get here all ints are FIQs*/
}
/****************************************************************************/
/****************************************************************************/
