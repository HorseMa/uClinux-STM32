/*
 *  linux/arch/m68k/kernel/ptrace.c
 *
 *  Copyright (C) 1994 by Hamish Macdonald
 *  Taken from linux/kernel/ptrace.c and modified for M680x0.
 *  linux/kernel/ptrace.c is by Ross Biro 1/23/92, edited by Linus Torvalds
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License.  See the file COPYING in the main directory of
 * this archive for more details.
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/errno.h>
#include <linux/ptrace.h>
#include <linux/user.h>
#include <linux/config.h>

#include <asm/uaccess.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/processor.h>
#include <asm/unistd.h>

/*
 * does not yet catch signals sent when the child dies.
 * in exit.c or in signal.c.
 */

/* determines which bits in the SR the user has access to. */
/* 1 = access 0 = no access */
#define SR_MASK 0x001f

/* sets the trace bits. */
#define TRACE_BITS 0x8000

/*
 * Get contents of register REGNO in task TASK.
 */
static inline long get_reg(struct task_struct *task, int regno)
{
	struct user_context *user = task->thread.user;

	if (regno < 0 || regno >= PT__END)
		return 0;

	return ((unsigned long *) user)[regno];
}

/*
 * Write contents of register REGNO in task TASK.
 */
static inline int put_reg(struct task_struct *task, int regno,
			  unsigned long data)
{
	struct user_context *user = task->thread.user;

	if (regno < 0 || regno >= PT__END)
		return -EIO;

	switch (regno) {
	case PT_GR(0):
		return 0;
	case PT_PSR:
	case PT__STATUS:
		return -EIO;
	default:
		((unsigned long *) user)[regno] = data;
		return 0;
	}
}

/*
 * check that an address falls within the bounds of the target process's memory mappings
 */
static inline int is_user_addr_valid(struct task_struct *child,
				     unsigned long start, unsigned long len)
{
#ifndef CONFIG_UCLINUX
	if (start >= PAGE_OFFSET || len > PAGE_OFFSET - start)
		return -EIO;
	return 0;
#else
	struct vm_list_struct *vml;

	for (vml = child->mm->vmlist; vml; vml = vml->next)
		if (start >= vml->vma->vm_start && start + len <= vml->vma->vm_end)
			return 0;

	return -EIO;
#endif
}

/*
 * Called by kernel/ptrace.c when detaching..
 *
 * Control h/w single stepping
 */
void ptrace_disable(struct task_struct *child)
{
	child->thread.frame0->__status &= ~REG__STATUS_STEP;
}

void ptrace_enable(struct task_struct *child)
{
	child->thread.frame0->__status |= REG__STATUS_STEP;
}

asmlinkage int sys_ptrace(long request, long pid, long addr, long data)
{
	struct task_struct *child;
	unsigned long tmp;
	int ret;

	lock_kernel();
	ret = -EPERM;
	if (request == PTRACE_TRACEME) {
		/* are we already being traced? */
		if (current->ptrace & PT_PTRACED)
			goto out;
		/* set the ptrace bit in the process flags. */
		current->ptrace |= PT_PTRACED;
		ret = 0;
		goto out;
	}
	ret = -ESRCH;
	read_lock(&tasklist_lock);
	child = find_task_by_pid(pid);
	if (child)
		get_task_struct(child);
	read_unlock(&tasklist_lock);
	if (!child)
		goto out;

	ret = -EPERM;
	if (pid == 1)		/* you may not mess with init */
		goto out_tsk;

	if (request == PTRACE_ATTACH) {
		ret = ptrace_attach(child);
		goto out_tsk;
	}

	ret = ptrace_check_attach(child, request == PTRACE_KILL);
	if (ret < 0)
		goto out_tsk;

	switch (request) {
		/* when I and D space are separate, these will need to be fixed. */
	case PTRACE_PEEKTEXT: /* read word at location addr. */
	case PTRACE_PEEKDATA: {
		int copied;

		ret = -EIO;
		if (is_user_addr_valid(child, addr, sizeof(tmp)) < 0)
			break;

		copied = access_process_vm(child, addr, &tmp, sizeof(tmp), 0);
		if (copied != sizeof(tmp))
			break;

		ret = put_user(tmp,(unsigned long *) data);
		break;
	}

		/* read the word at location addr in the USER area. */
	case PTRACE_PEEKUSR: {
		tmp = 0;
		ret = -EIO;
		if ((addr & 3) || addr < 0)
			break;

		ret = 0;
		switch (addr >> 2) {
		case 0 ... PT__END - 1:
			tmp = get_reg(child, addr >> 2);
			break;

		case PT__END + 0:
			tmp = child->mm->end_code - child->mm->start_code;
			break;

		case PT__END + 1:
			tmp = child->mm->end_data - child->mm->start_data;
			break;

		case PT__END + 2:
			tmp = child->mm->start_stack - child->mm->start_brk;
			break;

		case PT__END + 3:
			tmp = child->mm->start_code;
			break;

		case PT__END + 4:
			tmp = child->mm->start_stack;
			break;

		default:
			ret = -EIO;
			break;
		}

		if (ret == 0)
			ret = put_user(tmp, (unsigned long *) data);
		break;
	}

		/* when I and D space are separate, this will have to be fixed. */
	case PTRACE_POKETEXT: /* write the word at location addr. */
	case PTRACE_POKEDATA:
		ret = -EIO;
		if (is_user_addr_valid(child, addr, sizeof(tmp)) < 0)
			break;
		if (access_process_vm(child, addr, &data, sizeof(data), 1) != sizeof(data))
			break;
		ret = 0;
		break;

	case PTRACE_POKEUSR: /* write the word at location addr in the USER area */
		ret = -EIO;
		if ((addr & 3) || addr < 0)
			break;

		ret = 0;
		switch (addr >> 2) {
		case 0 ... PT__END-1:
			ret = put_reg(child, addr >> 2, data);
			break;

		default:
			ret = -EIO;
			break;
		}
		break;

	case PTRACE_SYSCALL: /* continue and stop at next (return from) syscall */
	case PTRACE_CONT: /* restart after signal. */
		ret = -EIO;
		if ((unsigned long) data > _NSIG)
			break;
		if (request == PTRACE_SYSCALL)
			child->ptrace |= PT_TRACESYS;
		else
			child->ptrace &= ~PT_TRACESYS;
		child->exit_code = data;
		ptrace_disable(child);
		wake_up_process(child);
		ret = 0;
		break;

		/* make the child exit.  Best I can do is send it a sigkill.
		 * perhaps it should be put in the status that it wants to
		 * exit.
		 */
	case PTRACE_KILL:
		ret = 0;
		if (child->state == TASK_ZOMBIE) /* already dead */
			break;
		child->exit_code = SIGKILL;
		ptrace_disable(child);
		wake_up_process(child);
		break;

	case PTRACE_SINGLESTEP:  /* set the trap flag. */
		ret = -EIO;
		if ((unsigned long) data > _NSIG)
			break;
		child->ptrace &= ~PT_TRACESYS;
		ptrace_enable(child);
		child->exit_code = data;
		wake_up_process(child);
		ret = 0;
		break;

	case PTRACE_DETACH:	/* detach a process that was attached. */
		ret = ptrace_detach(child, data);
		break;

	case PTRACE_GETREGS: { /* Get all integer regs from the child. */
		int i;
		for (i = 0; i < PT__GPEND; i++) {
			tmp = get_reg(child, i);
			if (put_user(tmp, (unsigned long *) data)) {
				ret = -EFAULT;
				break;
			}
			data += sizeof(long);
		}
		ret = 0;
		break;
	}

	case PTRACE_SETREGS: { /* Set all integer regs in the child. */
		int i;
		for (i = 0; i < PT__GPEND; i++) {
			if (get_user(tmp, (unsigned long *) data)) {
				ret = -EFAULT;
				break;
			}
			put_reg(child, i, tmp);
			data += sizeof(long);
		}
		ret = 0;
		break;
	}

	case PTRACE_GETFPREGS: { /* Get the child FP/Media state. */
		ret = 0;
		if (copy_to_user((void *) data,
				 &child->thread.user->f,
				 sizeof(child->thread.user->f)))
			ret = -EFAULT;
		break;
	}

	case PTRACE_SETFPREGS: { /* Set the child FP/Media state. */
		ret = 0;
		if (copy_from_user(&child->thread.user->f,
				   (void *) data,
				   sizeof(child->thread.user->f)))
			ret = -EFAULT;
		break;
	}

	case PTRACE_GETFDPIC:
		tmp = 0;
		switch (addr) {
		case PTRACE_GETFDPIC_EXEC:
			tmp = child->mm->exec_fdpic_loadmap;
			break;
		case PTRACE_GETFDPIC_INTERP:
			tmp = child->mm->interp_fdpic_loadmap;
			break;
		default:
			break;
		}

		ret = 0;
		if (put_user(tmp, (unsigned long *) data)) {
			ret = -EFAULT;
			break;
		}
		break;

	default:
		ret = -EIO;
		break;
	}
out_tsk:
	free_task_struct(child);
out:
	unlock_kernel();
	return ret;
}

asmlinkage void syscall_trace(int leaving)
{
#if 0
	if (!current->mm)
		return;

	if (__frame->gr7 == __NR_close)
		return;

#if 0
	if (__frame->gr7 != __NR_mmap2 &&
	    __frame->gr7 != __NR_vfork &&
	    __frame->gr7 != __NR_execve &&
	    __frame->gr7 != __NR_exit)
		return;
#endif

	if (!leaving) {
		printk(KERN_CRIT "[%d] syscall_%lu(%lx,%lx,%lx,%lx,%lx,%lx)\n",
		       current->pid,
		       __frame->gr7,
		       __frame->gr8,
		       __frame->gr9,
		       __frame->gr10,
		       __frame->gr11,
		       __frame->gr12,
		       __frame->gr13);
	}
	else {
		if ((int)__frame->gr8 > -4096 && (int)__frame->gr8 < 4096)
			printk(KERN_CRIT "[%d] sys() = %ld\n", current->pid, __frame->gr8);
		else
			printk(KERN_CRIT "[%d] sys() = %lx\n", current->pid, __frame->gr8);
	}
	return;
#endif

	/* we need to indicate entry or exit to strace */
	if (leaving)
		__frame->__status |= REG__STATUS_SYSC_EXIT;
	else
		__frame->__status |= REG__STATUS_SYSC_ENTRY;

	if ((current->ptrace & (PT_PTRACED|PT_TRACESYS))
			!= (PT_PTRACED|PT_TRACESYS))
		return;
	current->exit_code = SIGTRAP;
	current->state = TASK_STOPPED;
	notify_parent(current, SIGCHLD);
	schedule();
	/*
	 * this isn't the same as continuing with a signal, but it will do
	 * for normal use.  strace only continues with a signal if the
	 * stopping signal is not SIGTRAP.  -brl
	 */
	if (current->exit_code) {
		send_sig(current->exit_code, current, 1);
		current->exit_code = 0;
	}
}
