/***************************************************************************/

/*
 *	linux/arch/m68knommu/platform/547x/config.c
 *
 *	Copyright (C) 1999-2004, Greg Ungerer (gerg@snapgear.com)
 */

/***************************************************************************/

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/param.h>
#include <linux/init.h>
#include <asm/irq.h>
#include <asm/dma.h>
#include <asm/traps.h>
#include <asm/machdep.h>
#include <asm/coldfire.h>
#include <asm/mcfslt.h>
#include <asm/mcfsim.h>
#include <asm/delay.h>

/***************************************************************************/

void	coldfire_profile_init(void);

/***************************************************************************/

void coldfire_tick(void)
{
	volatile struct mcfslt *sp;

	/* Reset the SLT timer */
	sp = (volatile struct mcfslt *) (MCF_MBAR + MCFSLT_TIMER0);
	sp->ssr = MCFSLT_SSR_TE;
}

/***************************************************************************/

void coldfire_timer_init(void (*handler)(int, void *, struct pt_regs *))
{
	volatile struct mcfslt *sp;
	volatile unsigned char *icrp;
	volatile unsigned int *imrp;

	request_irq(64+54, handler, SA_INTERRUPT, "ColdFire Timer", sp);
	icrp = (volatile unsigned char *) (MCF_MBAR + MCFSIM_ICR0 + 54);
	*icrp = MCFSIM_ICR_LEVEL5 | MCFSIM_ICR_PRI3;
	imrp = (volatile unsigned int *) (MCF_MBAR + MCFSIM_IMRH);
	*imrp &= ~(1 << (54 - 32));

	/* Set up SLT TIMER0 as poll clock */
	sp = (volatile struct mcfslt *) (MCF_MBAR + MCFSLT_TIMER0);
	sp->scr = 0;

	sp->stcnt = MCF_BUSCLK / HZ;
	sp->scr = MCFSLT_SCR_RUN | MCFSLT_SCR_IEN | MCFSLT_SCR_TEN;

#ifdef CONFIG_HIGHPROFILE
	coldfire_profile_init();
#endif
}

/***************************************************************************/

unsigned long coldfire_timer_offset(void)
{
	volatile struct mcfslt *sp;
	volatile unsigned int *ipr;
	unsigned long trr, tcn, offset;

	ipr = (volatile unsigned int *) (MCF_MBAR + MCFSIM_IPRH);
	sp = (volatile struct mcfslt *) (MCF_MBAR + MCFSLT_TIMER0);
	tcn = sp->scnt;
	trr = sp->stcnt;

	/*
	 * If we are still in the first half of the upcount and a
	 * timer interupt is pending, then add on a ticks worth of time.
	 */
	offset = ((tcn * (1000000 / HZ)) / trr);
	if (((offset * 2) < (1000000 / HZ)) && (*ipr & (1 << (54 - 32))))
		offset += 1000000 / HZ;
	return offset;	
}

/***************************************************************************/
#ifdef CONFIG_HIGHPROFILE
/***************************************************************************/

#define	PROFILEHZ	1013

/*
 *	Use the other timer to provide high accuracy profiling info.
 */

void coldfire_profile_tick(int irq, void *dummy, struct pt_regs *regs)
{
	volatile struct mcfslt *sp;

	/* Reset the SLT TIMER1 */
	sp = (volatile struct mcfslt *) (MCF_MBAR + MCFSLT_TIMER1);
	sp->ssr = MCFSLT_SSR_TE;

        if (!user_mode(regs)) {
                if (prof_buffer && current->pid) {
                        extern int _stext;
                        unsigned long ip = instruction_pointer(regs);
                        ip -= (unsigned long) &_stext;
                        ip >>= prof_shift;
                        if (ip < prof_len)
                                prof_buffer[ip]++;
                }
        }
}

void coldfire_profile_init(void)
{
	volatile struct mcfslt *sp;
	volatile unsigned char *icrp;
	volatile unsigned int *imrp;

	printk("PROFILE: lodging slice timer1=%d for profiling\n", PROFILEHZ);

	request_irq(64+53, coldfire_profile_tick, (SA_INTERRUPT | IRQ_FLG_FAST),
		"Profile Timer", sp);
	icrp = (volatile unsigned char *) (MCF_MBAR + MCFSIM_ICR0 + 53);
	*icrp = MCFSIM_ICR_LEVEL7 | MCFSIM_ICR_PRI3;
	imrp = (volatile unsigned int *) (MCF_MBAR + MCFSIM_IMRH);
	*imrp &= ~(1 << (53 - 32));

	/* Set up SLT TIMER1 as profiler clock */
	sp = (volatile struct mcfslt *) (MCF_MBAR + MCFSLT_TIMER1);
	sp->scr = 0;

	sp->stcnt = MCF_CLK / PROFILEHZ;
	sp->scr = MCFSLT_SCR_RUN | MCFSLT_SCR_IEN | MCFSLT_SCR_TEN;
}

/***************************************************************************/
#endif	/* CONFIG_HIGHPROFILE */
/***************************************************************************/

/*
 *	Program the vector to be an auto-vectored.
 */

void mcf_autovector(unsigned int vec)
{
	/* Everything is auto-vectored on the 547x */
}

/***************************************************************************/

extern e_vector	*_ramvec;

void set_evector(int vecnum, void (*handler)(void))
{
	if (vecnum >= 0 && vecnum <= 255)
		_ramvec[vecnum] = handler;
}

/***************************************************************************/

/* assembler routines */
asmlinkage void buserr(void);
asmlinkage void trap(void);
asmlinkage void system_call(void);
asmlinkage void inthandler(void);

void __init coldfire_trap_init(void)
{
	int i;

#ifndef ENABLE_dBUG
	*((volatile unsigned int *) (MCF_MBAR + MCFSIM_IMRL)) = 0xfffffffe;
	*((volatile unsigned int *) (MCF_MBAR + MCFSIM_IMRH)) = 0xffffffff;
#endif

	/*
	 *	There is a common trap handler and common interrupt
	 *	handler that handle almost every vector. We treat
	 *	the system call and bus error special, they get their
	 *	own first level handlers.
	 */
#ifndef ENABLE_dBUG
	for (i = 3; (i <= 23); i++)
		_ramvec[i] = trap;
	for (i = 33; (i <= 63); i++)
		_ramvec[i] = trap;
#endif

	for (i = 24; (i <= 30); i++)
		_ramvec[i] = inthandler;
#ifndef ENABLE_dBUG
	_ramvec[31] = inthandler;  // Disables the IRQ7 button
#endif

	for (i = 64; (i < 255); i++)
		_ramvec[i] = inthandler;
	_ramvec[255] = 0;

	_ramvec[2] = buserr;
	_ramvec[32] = system_call;
}

/***************************************************************************/

void coldfire_reset(void)
{
	HARD_RESET_NOW();
}

/***************************************************************************/

/*
 *	Generic dumping code. Used for panic and debug.
 */

void dump(struct pt_regs *fp)
{
	extern unsigned int sw_usp, sw_ksp;
	unsigned long	*sp;
	unsigned char	*tp;
	int		i;

	printk("\nCURRENT PROCESS:\n\n");
	printk("COMM=%s PID=%d\n", current->comm, current->pid);

	if (current->mm) {
		printk("TEXT=%08x-%08x DATA=%08x-%08x BSS=%08x-%08x\n",
			(int) current->mm->start_code,
			(int) current->mm->end_code,
			(int) current->mm->start_data,
			(int) current->mm->end_data,
			(int) current->mm->end_data,
			(int) current->mm->brk);
		printk("USER-STACK=%08x  KERNEL-STACK=%08x\n\n",
			(int) current->mm->start_stack,
			(int) (((unsigned long) current) + 2 * PAGE_SIZE));
	}

	printk("PC: %08lx\n", fp->pc);
	printk("SR: %08lx    SP: %08lx\n", (long) fp->sr, (long) fp);
	printk("d0: %08lx    d1: %08lx    d2: %08lx    d3: %08lx\n",
		fp->d0, fp->d1, fp->d2, fp->d3);
	printk("d4: %08lx    d5: %08lx    a0: %08lx    a1: %08lx\n",
		fp->d4, fp->d5, fp->a0, fp->a1);
	printk("\nUSP: %08x   KSP: %08x   TRAPFRAME: %08x\n",
		sw_usp, sw_ksp, (unsigned int) fp);

	printk("\nCODE:");
	tp = ((unsigned char *) fp->pc) - 0x20;
	for (sp = (unsigned long *) tp, i = 0; (i < 0x40);  i += 4) {
		if ((i % 0x10) == 0)
			printk("\n%08x: ", (int) (tp + i));
		printk("%08x ", (int) *sp++);
	}
	printk("\n");

	printk("\nKERNEL STACK:");
	tp = ((unsigned char *) fp) - 0x40;
	for (sp = (unsigned long *) tp, i = 0; (i < 0xc0); i += 4) {
		if ((i % 0x10) == 0)
			printk("\n%08x: ", (int) (tp + i));
		printk("%08x ", (int) *sp++);
	}
	printk("\n");

	printk("\nUSER STACK:");
	tp = (unsigned char *) (sw_usp - 0x10);
	for (sp = (unsigned long *) tp, i = 0; (i < 0x80); i += 4) {
		if ((i % 0x10) == 0)
			printk("\n%08x: ", (int) (tp + i));
		printk("%08x ", (int) *sp++);
	}
	printk("\n\n");
}

/***************************************************************************/
#ifdef TRAP_DBG_INTERRUPT

asmlinkage void dbginterrupt_c(struct frame *fp)
{
	extern void dump(struct pt_regs *fp);
	printk("%s(%d): BUS ERROR TRAP\n", __FILE__, __LINE__);
        dump((struct pt_regs *) fp);
	asm("halt");
}

#endif
/***************************************************************************/

void config_BSP(char *commandp, int size)
{
#ifdef CONFIG_BOOTPARAM
	strncpy(commandp, CONFIG_BOOTPARAM_STRING, size);
	commandp[size-1] = 0;
#else
	memset(commandp, 0, size);
#endif

	mach_sched_init = coldfire_timer_init;
	mach_tick = coldfire_tick;
	mach_trap_init = coldfire_trap_init;
	mach_reset = coldfire_reset;
	mach_gettimeoffset = coldfire_timer_offset;
}

/***************************************************************************/
