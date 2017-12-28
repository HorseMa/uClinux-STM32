/*
 *	Cirrus MaverickCrunch support
 *
 *	Copyright (c) 2003 Petko Manolov <petkan@nucleusys.com>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	version 2 as published by the Free Software Foundation;
 */

#include <linux/sched.h>
#include <linux/init.h>
#include <asm/io.h>
#include <asm/arch/crunch.h>

unsigned int read_dspsc_low(void)
{
	int res;

	asm volatile (
	"cdp        p4, 1, c15, c0, c0, 7\n\t"	// "cfmv32sc      mvdx15, dspsc\n\t"
	"mrc        p5, 0, %0, c15, c0, 0"	// "cfmvr64l  %0, mvdx15"
	:"=r" (res)
	:
	:"memory");

	return res;
}

unsigned int read_dspsc_high(void)
{
	int res;

	asm volatile (
	"cdp        p4, 1, c15, c0, c0, 7\n\t"	// "cfmv32sc   mvdx15, dspsc\n\t"
	"mrc        p5, 0, %0, c15, c0, 1"	// "cfmvr64h   %0, mvdx15"
	:"=r" (res)
	:
	:"memory");

	return res;
}

void write_dspsc(unsigned int dspsc)
{
	asm volatile (
	"mcr        p5, 0, %0, c15, c0, 0\n\t"	// "cfmv64lr     mvdx15, %0\n\t"
	"cdp        p4, 2, c15, c0, c0, 7"	// "cfmvsc32     dspsc, mvdx15"
	:
	:"r" (dspsc), "rN"(-1)
	:"memory");
}

void crunch_init(void)
{
	write_dspsc(CRUNCH_INIT);
	current->flags |= PF_USEDCRUNCH;
}

/*
 * had to do it this way since "clf(); cli();" takes longer
 */
static inline void clear_fiq_irq(void)
{
	int cpsr;

	asm volatile (
	"mrs	%0, CPSR\n\t"
	"orr	%0, %0, #192\n\t"
	"msr	CPSR_c, %0"
	:"=r" (cpsr)
	:
	:"memory");
}

/*
 * had to do it this way since "stf(); sti();" takes (twice) longer
 */

static inline void restore_fiq_irq(void)
{
	int cpsr;

	asm volatile (
	"mrs	%0, CPSR\n\t"
	"bic	%0, %0, #192\n\t"
	"msr	CPSR_c, %0"
	:"=r" (cpsr)
	:
	:"memory");
}

static inline void save_accumulators(struct task_struct *tsk)
{
	int tmp;
	struct fp_crunch_struct *fp = &tsk->thread.fpstate.crunch;

	/*
	 * clear the IRQ & FIQ to avoid some of the bugs in the errata
	 */
	clear_fiq_irq();

	asm volatile (
	"cdp        p4, 1, c0, c0, c0, 2\n\t"	// "cfmv32al      mvfx0, mvax0\n\t"
	"stc        p5, c0, [%0], #4\n\t"	// "cfstr32      mvfx0, [%0], 4\n\t"
	"cdp        p4, 1, c1, c0, c0, 3\n\t"	// "cfmv32am      mvfx1, mvax0\n\t"
	"stc        p5, c1, [%0], #4\n\t"	// "cfstr32      mvfx1, [%0], 4\n\t"
	"cdp        p4, 1, c2, c0, c0, 4\n\t"	// "cfmv32ah      mvfx2, mvax0\n\t"
	"stc        p5, c2, [%0], #4\n\t"	// "cfstr32      mvfx2, [%0], 4\n\t"
	"cdp        p4, 1, c3, c1, c0, 2\n\t"	// "cfmv32al      mvfx3, mvax1\n\t"
	"stc        p5, c3, [%0], #4\n\t"	// "cfstr32      mvfx3, [%0], 4\n\t"
	"cdp        p4, 1, c4, c1, c0, 3\n\t"	// "cfmv32am      mvfx4, mvax1\n\t"
	"stc        p5, c4, [%0], #4\n\t"	// "cfstr32      mvfx4, [%0], 4\n\t"
	"cdp        p4, 1, c5, c1, c0, 4\n\t"	// "cfmv32ah      mvfx5, mvax1\n\t"
	"stc        p5, c5, [%0], #4\n\t"	// "cfstr32      mvfx5, [%0], 4\n\t"
	"cdp        p4, 1, c6, c2, c0, 2\n\t"	// "cfmv32al      mvfx6, mvax2\n\t"
	"stc        p5, c6, [%0], #4\n\t"	// "cfstr32      mvfx6, [%0], 4\n\t"
	"cdp        p4, 1, c7, c2, c0, 3\n\t"	// "cfmv32am      mvfx7, mvax2\n\t"
	"stc        p5, c7, [%0], #4\n\t"	// "cfstr32      mvfx7, [%0], 4\n\t"
	"cdp        p4, 1, c8, c2, c0, 4\n\t"	// "cfmv32ah      mvfx8, mvax2\n\t"
	"stc        p5, c8, [%0], #4\n\t"	// "cfstr32      mvfx8, [%0], 4\n\t"
	"cdp        p4, 1, c9, c3, c0, 2\n\t"	// "cfmv32al      mvfx9, mvax3\n\t"
	"stc        p5, c9, [%0], #4\n\t"	// "cfstr32      mvfx9, [%0], 4\n\t"
	"cdp        p4, 1, c10, c3, c0, 3\n\t"	// "cfmv32am     mvfx10, mvax3\n\t"
	"stc        p5,c10, [%0], #4\n\t"	// "cfstr32      mvfx10, [%0], 4\n\t"
	"cdp        p4, 1, c11, c3, c0, 4\n\t"	// "cfmv32ah     mvfx11, mvax3\n\t"
	"stc        p5, c11, [%0, #0]"		// "cfstr32      mvfx11, [%0, #0]"
	:"=&r" (tmp)
	:"0" (&fp->acc0[0])
	:"memory");

	restore_fiq_irq();
}

static inline void restore_accumulators(struct task_struct *tsk)
{
	int tmp;
	struct fp_crunch_struct *fp = &tsk->thread.fpstate.crunch;

	/*
	 * clear the IRQ & FIQ to avoid some of the bugs in the errata
	 */
	clear_fiq_irq();

	asm volatile (
	"ldc        p5, c0, [%0],#4\n\t"	// "cfldr32        mvfx0, [%0], 4\n\t"
	"cdp        p4, 2, c0, c0, c0, 2\n\t"	// "cfmval32       mvax0, mvfx0\n\t"
	"ldc        p5, c1, [%0],#4\n\t"	// "cfldr32        mvfx1, [%0], 4\n\t"
	"cdp        p4, 2, c0, c1, c0, 3\n\t"	// "cfmvam32       mvax0, mvfx1\n\t"
	"ldc        p5, c2, [%0],#4\n\t"	// "cfldr32        mvfx2, [%0], 4\n\t"
	"cdp        p4, 2, c0, c2, c0, 4\n\t"	// "cfmvah32       mvax0, mvfx2\n\t"
	"ldc        p5, c3, [%0],#4\n\t"	// "cfldr32        mvfx3, [%0], 4\n\t"
	"cdp        p4, 2, c1, c3, c0, 2\n\t"	// "cfmval32       mvax1, mvfx3\n\t"
	"ldc        p5, c4, [%0],#4\n\t"	// "cfldr32        mvfx4, [%0], 4\n\t"
	"cdp        p4, 2, c1, c4, c0, 3\n\t"	// "cfmvam32       mvax1, mvfx4\n\t"
	"ldc        p5, c5, [%0],#4\n\t"	// "cfldr32        mvfx5, [%0], 4\n\t"
	"cdp        p4, 2, c1, c5, c0, 4\n\t"	// "cfmvah32       mvax1, mvfx5\n\t"
	"ldc        p5, c6, [%0],#4\n\t"	// "cfldr32        mvfx6, [%0], 4\n\t"
	"cdp        p4, 2, c2, c6, c0, 2\n\t"	// "cfmval32       mvax2, mvfx6\n\t"
	"ldc        p5, c7, [%0],#4\n\t"	// "cfldr32        mvfx7, [%0], 4\n\t"
	"cdp        p4, 2, c2, c7, c0, 3\n\t"	// "cfmvam32       mvax2, mvfx7\n\t"
	"ldc        p5, c8, [%0],#4\n\t"	// "cfldr32        mvfx8, [%0], 4\n\t"
	"cdp        p4, 2, c2, c8, c0, 4\n\t"	// "cfmvah32       mvax2, mvfx8\n\t"
	"ldc        p5, c9, [%0],#4\n\t"	// "cfldr32        mvfx9, [%0], 4\n\t"
	"cdp        p4, 2, c3, c9, c0, 2\n\t"	// "cfmval32       mvax3, mvfx9\n\t"
	"ldc        p5, c10, [%0],#4\n\t"	// "cfldr32        mvfx10, [%0], 4\n\t"
	"cdp        p4, 2, c3, c10, c0, 3\n\t"	// "cfmvam32       mvax3, mvfx10\n\t"
	"ldc        p5, c11, [%0, #0]\n\t"	// "cfldr32        mvfx11, [%0, #0]\n\t"
	"cdp        p4, 2, c3, c11, c0, 4"	// "cfmvah32       mvax3, mvfx11"
	:"=&r" (tmp)
	:"0" (&fp->acc0[0])
	:"memory");

	restore_fiq_irq();
}

void save_crunch(struct task_struct *tsk)
{
	int tmp;
	struct fp_crunch_struct *fp = &tsk->thread.fpstate.crunch;

	asm volatile (
	"stcl       p5, c0, [%0],#8\n\t"	// "cfstr64        mvdx0, [%0], 8\n\t"
	"stcl       p5, c1, [%0],#8\n\t"	// "cfstr64        mvdx1, [%0], 8\n\t"
	"stcl       p5, c2, [%0],#8\n\t"	// "cfstr64        mvdx2, [%0], 8\n\t"
	"stcl       p5, c3, [%0],#8\n\t"	// "cfstr64        mvdx3, [%0], 8\n\t"
	"stcl       p5, c4, [%0],#8\n\t"	// "cfstr64        mvdx4, [%0], 8\n\t"
	"stcl       p5, c5, [%0],#8\n\t"	// "cfstr64        mvdx5, [%0], 8\n\t"
	"stcl       p5, c6, [%0],#8\n\t"	// "cfstr64        mvdx6, [%0], 8\n\t"
	"stcl       p5, c7, [%0],#8\n\t"	// "cfstr64        mvdx7, [%0], 8\n\t"
	"stcl       p5, c8, [%0],#8\n\t"	// "cfstr64        mvdx8, [%0], 8\n\t"
	"stcl       p5, c9, [%0],#8\n\t"	// "cfstr64        mvdx9, [%0], 8\n\t"
	"stcl       p5, c10, [%0],#8\n\t"	// "cfstr64        mvdx10, [%0], 8\n\t"
	"stcl       p5, c11, [%0],#8\n\t"	// "cfstr64        mvdx11, [%0], 8\n\t"
	"stcl       p5, c12, [%0],#8\n\t"	// "cfstr64        mvdx12, [%0], 8\n\t"
	"stcl       p5, c13, [%0],#8\n\t"	// "cfstr64        mvdx13, [%0], 8\n\t"
	"stcl       p5, c14, [%0],#8\n\t"	// "cfstr64        mvdx14, [%0], 8\n\t"
	"stcl       p5, c15, [%0, #0]\n\t"	// "cfstr64        mvdx15, [%0, #0]\n\t"
	"cdp        p4, 1, c15, c0, c0, 7\n\t"	// "cfmv32sc       mvdx15, dspsc\n\t"
	"stc        p5, c15, [%2, #0]"		// "cfstr32        mvfx15, [%2, #0]"
	:"=&r" (tmp)
	:"0" (&fp->regs[0]), "r" (&fp->dspsc)
	:"memory");
#ifdef CONFIG_EP93XX_CRUNCH_ACC
	/*
	 * this call should be made exactly here since it's corrupting
	 * the contents of most crunch registers ;-)
	 */
	save_accumulators(tsk);
#endif
}

void restore_crunch(struct task_struct *tsk)
{
	int tmp;
	struct fp_crunch_struct *fp = &tsk->thread.fpstate.crunch;
#ifdef CONFIG_EP93XX_CRUNCH_ACC
	/*
	 * same as above, but reversed.  if you put the call below the 'asm'
	 * code then you'll corrupt the 
	 */
	restore_accumulators(tsk);
#endif
	asm volatile (
	"ldc        p5, c15, [%2, #0]\n\t"	// "cfldr32   mvfx15, [%2, #0]\n\t"
	"cdp        p4, 2, c15, c0, c0, 7\n\t"	// "cfmvsc32  dspsc, mvdx15\n\t"
	"ldcl       p5, c0, [%0],#8\n\t"	// "cfldr64   mvdx0, [%0], 8\n\t"
	"ldcl       p5, c1, [%0],#8\n\t"	// "cfldr64   mvdx1, [%0], 8\n\t"
	"ldcl       p5, c2, [%0],#8\n\t"	// "cfldr64   mvdx2, [%0], 8\n\t"
	"ldcl       p5, c3, [%0],#8\n\t"	// "cfldr64   mvdx3, [%0], 8\n\t"
	"ldcl       p5, c4, [%0],#8\n\t"	// "cfldr64   mvdx4, [%0], 8\n\t"
	"ldcl       p5, c5, [%0],#8\n\t"	// "cfldr64   mvdx5, [%0], 8\n\t"
	"ldcl       p5, c6, [%0],#8\n\t"	// "cfldr64   mvdx6, [%0], 8\n\t"
	"ldcl       p5, c7, [%0],#8\n\t"	// "cfldr64   mvdx7, [%0], 8\n\t"
	"ldcl       p5, c8, [%0],#8\n\t"	// "cfldr64   mvdx8, [%0], 8\n\t"
	"ldcl       p5, c9, [%0],#8\n\t"	// "cfldr64   mvdx9, [%0], 8\n\t"
	"ldcl       p5, c10, [%0],#8\n\t"	// "cfldr64   mvdx10, [%0], 8\n\t"
	"ldcl       p5, c11, [%0],#8\n\t"	// "cfldr64   mvdx11, [%0], 8\n\t"
	"ldcl       p5, c12, [%0],#8\n\t"	// "cfldr64   mvdx12, [%0], 8\n\t"
	"ldcl       p5, c13, [%0],#8\n\t"	// "cfldr64   mvdx13, [%0], 8\n\t"
	"ldcl       p5, c14, [%0],#8\n\t"	// "cfldr64   mvdx14, [%0], 8\n\t"
	"ldcl       p5, c15, [%0, #0]"		// "cfldr64   mvdx15, [%0, #0]"
	:"=&r" (tmp)
	:"0" (&fp->regs[0]), "r" (&fp->dspsc)
	:"memory");
}

void crunch_exception(int irq, void *dev_id, struct pt_regs *regs)
{
	int sc, opc;

	send_sig(SIGFPE, current, 1);
	opc = read_dspsc_high();
	sc = read_dspsc_low();
	printk("%s: DSPSC_high=%08x, DSPSC_low=%08x\n", __FUNCTION__, opc, sc);
	sc &= ~(1 << 21);	/* we recure without this */
	write_dspsc(sc);
}

/*
 * only register ep9312 default FPU handler...
 */
__init int setup_crunch(void)
{
	int res;

	res = request_irq(CRUNCH_IRQ, crunch_exception, SA_INTERRUPT, "FPU", NULL);
	if (res) {
		printk("Crunch IRQ (%d) allocation failure\n", CRUNCH_IRQ);
		return res;
	}

	return res;
}

static inline int insn_is_crunch(long insn)
{
	long tmp;

	tmp = (insn >> 24) & 0x0e;
	/* cdp, mcr, mrc */
	if (tmp == 0x0e || tmp == 0x0c) {
		tmp = (insn >> 8) & 0x0f;
		if (tmp == 4)
			return 1;
		if (tmp == 5)
			return 1;
		if (tmp == 6)
			return 1;
	}

	return 0;
}

int crunch_opcode(struct pt_regs *regs)
{
	int *insn;
	struct task_struct *tsk = current;

	insn = (int *) (instruction_pointer(regs) - 4);
	if (!insn_is_crunch(*insn)) {
		/*
		 * not a crunch instruction, but might be another
		 * (FPA/VFP) floating point one
		 */
		return 0;
	}

	crunch_enable();
	regs->ARM_pc -= 4;	/* restart the Crunch instruction */
	if (tsk->flags & PF_USEDCRUNCH) {
		restore_crunch(tsk);
	} else {
		crunch_init();
	}
	tsk->flags |= PF_USEDFPU;

	return 1;
}

__initcall(setup_crunch);
