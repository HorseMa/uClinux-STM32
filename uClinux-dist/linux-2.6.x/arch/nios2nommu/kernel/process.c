/*--------------------------------------------------------------------
 *
 * arch/nios2nommu/kernel/process.c
 *
 * Derived from M68knommu
 *
 *  Copyright (C) 1995  Hamish Macdonald
 *  Copyright (C) 2000-2002, David McCullough <davidm@snapgear.com>
 * Copyright (C) 2004   Microtronix Datacom Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
 *  68060 fixes by Jesper Skov
 * Jan/20/2004		dgt	    NiosII
 *                            rdusp() === (pt_regs *) regs->sp
 *                            Monday:
 *                             asm-nios2nommu\processor.h now bears
 *                              inline thread_saved_pc
 *                                      (struct thread_struct *t)
 *                             Friday: it's back here now
 *
 ---------------------------------------------------------------------*/


/*
 * This file handles the architecture-dependent parts of process handling..
 */

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/user.h>
#include <linux/a.out.h>
#include <linux/interrupt.h>
#include <linux/reboot.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/err.h>

#include <asm/system.h>
#include <asm/traps.h>
#include <asm/setup.h>
#include <asm/pgtable.h>
#include <asm/cacheflush.h>

asmlinkage void ret_from_fork(void);

/*
 * The following aren't currently used.
 */
void (*pm_idle)(void) = NULL;
EXPORT_SYMBOL(pm_idle);

void (*pm_power_off)(void) = NULL;
EXPORT_SYMBOL(pm_power_off);

void default_idle(void)
{
	local_irq_disable();
	if (!need_resched()) {
		local_irq_enable();
		__asm__("nop");   // was asm sleep
	} else
		local_irq_enable();
}

void (*idle)(void) = default_idle;

/*
 * The idle thread. There's no useful work to be
 * done, so just try to conserve power and have a
 * low exit latency (ie sit in a loop waiting for
 * somebody to say that they'd like to reschedule)
 */
void cpu_idle(void)
{
	while (1) {
		while (!need_resched())
			idle();
		preempt_enable_no_resched();
		schedule();
		preempt_disable();
	}
}

/*
 * The development boards have no way to pull a board
 * reset. Just jump to the cpu reset address and let
 * the code in head.S take care of disabling peripherals.
 */

void machine_restart(char * __unused)
{
	local_irq_disable();
	__asm__ __volatile__ (
	"jmp	%0\n\t"
	: 
	: "r" (CPU_RESET_ADDRESS)
	: "r4");
}

EXPORT_SYMBOL(machine_restart);

void machine_halt(void)
{
	local_irq_disable();
	for (;;);
}

EXPORT_SYMBOL(machine_halt);

void exit_thread(void)
{
}

void release_thread(struct task_struct *dead_task)
{
	/* nothing to do ... */
}

/*
 * There is no way to power off the development
 * boards. So just spin lock for now. If you have
 * your own board with power down circuits add you
 * specific code here.
 */

void machine_power_off(void)
{
	local_irq_disable();
	for (;;);
}

EXPORT_SYMBOL(machine_power_off);

void show_regs(struct pt_regs * regs)
{
	printk(KERN_NOTICE "\n");

	printk(KERN_NOTICE "r1:  %08lx r2:  %08lx r3:  %08lx r4:  %08lx\n",
	       regs->r1,  regs->r2,  regs->r3,  regs->r4);

	printk(KERN_NOTICE "r5:  %08lx r6:  %08lx r7:  %08lx r8:  %08lx\n",
	       regs->r5,  regs->r6,  regs->r7,  regs->r8);

	printk(KERN_NOTICE "r9:  %08lx r10: %08lx r11: %08lx r12: %08lx\n",
	       regs->r9,  regs->r10, regs->r11, regs->r12);

	printk(KERN_NOTICE "r13: %08lx r14: %08lx r15: %08lx\n",
	       regs->r13, regs->r14, regs->r15);

	printk(KERN_NOTICE "ra:  %08lx fp:  %08lx sp:  %08lx gp:  %08lx\n",
	       regs->ra,  regs->fp,  regs->sp,  regs->gp);

	printk(KERN_NOTICE "ea:  %08lx estatus:  %08lx statusx:  %08lx\n",
	       regs->ea,  regs->estatus,  regs->status_extension);
}

/*
 * Create a kernel thread
 */
int kernel_thread(int (*fn)(void *), void * arg, unsigned long flags)
{
	long retval;
	long clone_arg = flags | CLONE_VM;
	mm_segment_t fs;

	fs = get_fs();
	set_fs(KERNEL_DS);

    __asm__ __volatile(

        "    movi    r2,    %6\n\t"     /* TRAP_ID_SYSCALL          */
        "    movi    r3,    %1\n\t"     /* __NR_clone               */
        "    mov     r4,    %5\n\t"     /* (clone_arg               */
                                        /*   (flags | CLONE_VM))    */
        "    movia   r5,    -1\n\t"     /* usp: -1                  */
        "    trap\n\t"                  /* sys_clone                */
        "\n\t"
        "    cmpeq   r4,    r3, zero\n\t"/*2nd return valu in r3    */
        "    bne     r4,    zero, 1f\n\t"/* 0: parent, just return.  */
                                         /* See copy_thread, called */
                                         /*  by do_fork, called by  */
                                         /*  nios2_clone, called by */
                                         /*  sys_clone, called by   */
                                         /*  syscall trap handler.  */

        "    mov     r4,    %4\n\t"     /* fn's parameter (arg)     */
        "\n\t"
        "\n\t"
        "    callr   %3\n\t"            /* Call function (fn)       */
        "\n\t"
        "    mov     r4,    r2\n\t"     /* fn's rtn code//;dgt2;tmp;*/
        "    movi    r2,    %6\n\t"     /* TRAP_ID_SYSCALL          */
        "    movi    r3,    %2\n\t"     /* __NR_exit                */
        "    trap\n\t"                  /* sys_exit()               */

           /* Not reached by child.                                 */
        "1:\n\t"
        "    mov     %0,    r2\n\t"     /* error rtn code (retval)  */

        :   "=r" (retval)               /* %0                       */

        :   "i" (__NR_clone)            /* %1                       */
          , "i" (__NR_exit)             /* %2                       */
          , "r" (fn)                    /* %3                       */
          , "r" (arg)                   /* %4                       */
          , "r" (clone_arg)             /* %5  (flags | CLONE_VM)   */
          , "i" (TRAP_ID_SYSCALL)       /* %6                       */

        :   "r2"                        /* Clobbered                */
          , "r3"                        /* Clobbered                */
          , "r4"                        /* Clobbered                */
          , "r5"                        /* Clobbered                */
          , "ra"                        /* Clobbered        //;mex1 */
        );

	set_fs(fs);
	return retval;
}

void flush_thread(void)
{
	/* Now, this task is no longer a kernel thread. */
	current->thread.flags &= ~NIOS2_FLAG_KTHREAD;

	set_fs(USER_DS);
}

/*
 * "nios2_fork()".. By the time we get here, the
 * non-volatile registers have also been saved on the
 * stack. We do some ugly pointer stuff here.. (see
 * also copy_thread)
 */

asmlinkage int nios2_fork(struct pt_regs *regs)
{
	/* fork almost works, enough to trick you into looking elsewhere :-( */
	return(-EINVAL);
}

/*
 * nios2_execve() executes a new program.
 */
asmlinkage int nios2_execve(struct pt_regs *regs)
{
	int error;
	char * filename;

	lock_kernel();
	filename = getname((char *) regs->r4);
	error = PTR_ERR(filename);
	if (IS_ERR(filename))
		goto out;
	error = do_execve(filename,
			  (char **) regs->r5,
			  (char **) regs->r6,
			  regs);
	putname(filename);
out:
	unlock_kernel();
	return error;
}

asmlinkage int nios2_vfork(struct pt_regs *regs)
{
	return do_fork(CLONE_VFORK | CLONE_VM | SIGCHLD, regs->sp, regs, 0, NULL, NULL);
}

asmlinkage int nios2_clone(struct pt_regs *regs)
{
    /* r4: clone_flags, r5: child_stack (usp)               */

	unsigned long clone_flags;
	unsigned long newsp;

	clone_flags = regs->r4;
	newsp = regs->r5;
	if (!newsp)
		newsp = regs->sp;
        return do_fork(clone_flags, newsp, regs, 0, NULL, NULL);
}

int copy_thread(int nr, unsigned long clone_flags,
		unsigned long usp, unsigned long topstk,
		struct task_struct * p, struct pt_regs * regs)
{
	struct pt_regs * childregs;
	struct switch_stack * childstack, *stack;
	unsigned long stack_offset, *retp;

	stack_offset = THREAD_SIZE - sizeof(struct pt_regs);
	childregs = (struct pt_regs *) ((unsigned long) p->stack + stack_offset);
	p->thread.kregs = childregs;

	*childregs = *regs;
	childregs->r2 = 0;      //;dgt2;...redundant?...see "rtnvals" below

	retp = ((unsigned long *) regs);
	stack = ((struct switch_stack *) retp) - 1;

	childstack = ((struct switch_stack *) childregs) - 1;
	*childstack = *stack;
	childstack->ra = (unsigned long)ret_from_fork;

	if (usp == -1)
		p->thread.kregs->sp = (unsigned long) childstack;
	else
		p->thread.kregs->sp = usp;

	p->thread.ksp = (unsigned long)childstack;

	/* Set the return value for the child. */
	childregs->r2 = 0;  //;dgt2;...redundant?...see childregs->r2 above
	childregs->r3 = 1;  //;dgt2;...eg: kernel_thread parent test

	/* Set the return value for the parent. */
	regs->r2 = p->pid;  // Return child pid to parent
	regs->r3 = 0;       //;dgt2;...eg: kernel_thread parent test

	return 0;
}

/*
 *	Generic dumping code. Used for panic and debug.
 */
void dump(struct pt_regs *fp)
{
	unsigned long	*sp;
	unsigned char	*tp;
	int		i;

	printk(KERN_EMERG "\nCURRENT PROCESS:\n\n");
	printk(KERN_EMERG "COMM=%s PID=%d\n", current->comm, current->pid);

	if (current->mm) {
		printk(KERN_EMERG "TEXT=%08x-%08x DATA=%08x-%08x BSS=%08x-%08x\n",
			(int) current->mm->start_code,
			(int) current->mm->end_code,
			(int) current->mm->start_data,
			(int) current->mm->end_data,
			(int) current->mm->end_data,
			(int) current->mm->brk);
		printk(KERN_EMERG "USER-STACK=%08x  KERNEL-STACK=%08x\n\n",
			(int) current->mm->start_stack,
			(int)(((unsigned long) current) + THREAD_SIZE));
	}

	printk(KERN_EMERG "PC: %08lx\n", fp->ea);
	printk(KERN_EMERG "SR: %08lx    SP: %08lx\n", (long) fp->estatus, (long) fp);
	printk(KERN_EMERG "r4: %08lx    r5: %08lx    r6: %08lx    r7: %08lx\n",
		fp->r4, fp->r5, fp->r6, fp->r7);
	printk(KERN_EMERG "r8: %08lx    r9: %08lx    r10: %08lx    r11: %08lx\n",
		fp->r8, fp->r9, fp->r10, fp->r11);
	printk(KERN_EMERG "\nUSP: %08x   TRAPFRAME: %08x\n", (unsigned int) fp->sp,
		(unsigned int) fp);

	printk(KERN_EMERG "\nCODE:");
	tp = ((unsigned char *) fp->ea) - 0x20;
	for (sp = (unsigned long *) tp, i = 0; (i < 0x40);  i += 4) {
		if ((i % 0x10) == 0)
			printk(KERN_EMERG "\n%08x: ", (int) (tp + i));
		printk(KERN_EMERG "%08x ", (int) *sp++);
	}
	printk(KERN_EMERG "\n");

	printk(KERN_EMERG "\nKERNEL STACK:");
	tp = ((unsigned char *) fp) - 0x40;
	for (sp = (unsigned long *) tp, i = 0; (i < 0xc0); i += 4) {
		if ((i % 0x10) == 0)
			printk(KERN_EMERG "\n%08x: ", (int) (tp + i));
		printk(KERN_EMERG "%08x ", (int) *sp++);
	}
	printk(KERN_EMERG "\n");
	printk(KERN_EMERG "\n");

	printk(KERN_EMERG "\nUSER STACK:");
	tp = (unsigned char *) (fp->sp - 0x10);
	for (sp = (unsigned long *) tp, i = 0; (i < 0x80); i += 4) {
		if ((i % 0x10) == 0)
			printk(KERN_EMERG "\n%08x: ", (int) (tp + i));
		printk(KERN_EMERG "%08x ", (int) *sp++);
	}
	printk(KERN_EMERG "\n\n");
}

/*
 * These bracket the sleeping functions..
 */
extern void scheduling_functions_start_here(void);
extern void scheduling_functions_end_here(void);
#define first_sched	((unsigned long) scheduling_functions_start_here)
#define last_sched	((unsigned long) scheduling_functions_end_here)

unsigned long get_wchan(struct task_struct *p)
{
	unsigned long fp, pc;
	unsigned long stack_page;
	int count = 0;
	if (!p || p == current || p->state == TASK_RUNNING)
		return 0;

	stack_page = (unsigned long)p;
	fp = ((struct switch_stack *)p->thread.ksp)->fp;        //;dgt2
	do {
		if (fp < stack_page+sizeof(struct task_struct) ||
		    fp >= 8184+stack_page)                          //;dgt2;tmp
			return 0;
		pc = ((unsigned long *)fp)[1];
		if (!in_sched_functions(pc))
			return pc;
		fp = *(unsigned long *) fp;
	} while (count++ < 16);                                 //;dgt2;tmp
	return 0;
}

/* Return saved PC of a blocked thread. */
unsigned long thread_saved_pc(struct task_struct *t)
{
	return (t->thread.kregs->ea);
}

/*
 * Do necessary setup to start up a newly executed thread.
 * Will statup in user mode (status_extension = 0).
 */
void start_thread(struct pt_regs * regs, unsigned long pc, unsigned long sp)
{
	memset((void *) regs, 0, sizeof(struct pt_regs));
	regs->estatus = NIOS2_STATUS_PIE_MSK;   // No user mode setting, at least not for now
	regs->ea = pc;
	regs->sp = sp;

	/* check if debug flag is set */
	if (current->thread.flags & NIOS2_FLAG_DEBUG ) {
		if ( *(u32*)pc == NIOS2_OP_NOP ) {
			*(u32*)pc = NIOS2_OP_BREAK;
			flush_icache_range(pc, pc+4);
		}
	}
}
