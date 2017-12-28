/*
 *  linux/arch/nios2nommu/kernel/ptrace.c
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

#include <asm/uaccess.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/processor.h>
#include <asm/cacheflush.h>

/* #define DEBUG */

#ifdef DEBUG
# define PRINTK_DEBUG(str...)   printk(KERN_DEBUG str)
#else
# define PRINTK_DEBUG(str...)   do { } while (0)
#endif

/*
 * does not yet catch signals sent when the child dies.
 * in exit.c or in signal.c.
 */

/* determines which bits in the SR the user has access to. */
/* 1 = access 0 = no access */
#define SR_MASK 0x00000000

/* Find the stack offset for a register, relative to thread.ksp. */
#define PT_REG(reg)	((long)&((struct pt_regs *)0)->reg)
#define SW_REG(reg)	((long)&((struct switch_stack *)0)->reg \
			 - sizeof(struct switch_stack))

/* Mapping from PT_xxx to the stack offset at which the register is
   saved.  Notice that usp has no stack-slot and needs to be treated
   specially (see get_reg/put_reg below). */
static int regoff[] = {
	         -1, PT_REG( r1), PT_REG( r2), PT_REG( r3),
	PT_REG( r4), PT_REG( r5), PT_REG( r6), PT_REG( r7),
	PT_REG( r8), PT_REG( r9), PT_REG(r10), PT_REG(r11),
	PT_REG(r12), PT_REG(r13), PT_REG(r14), PT_REG(r15),  /* reg 15 */
	SW_REG(r16), SW_REG(r17), SW_REG(r18), SW_REG(r19),
	SW_REG(r20), SW_REG(r21), SW_REG(r22), SW_REG(r23),
	         -1,          -1, PT_REG( gp), PT_REG( sp),
	PT_REG( fp), PT_REG( ea),          -1, PT_REG( ra),  /* reg 31 */
	PT_REG( ea),          -1,          -1,          -1,  /* use ea for pc */
	         -1,          -1,          -1,          -1,
	         -1,          -1,          -1,          -1   /* reg 43 */ 
};

/*
 * Get contents of register REGNO in task TASK.
 */
static inline long get_reg(struct task_struct *task, int regno)
{
	unsigned long *addr;

	if (regoff[regno] == -1)
		return 0;
	else if (regno < sizeof(regoff)/sizeof(regoff[0]))
		addr = (unsigned long *)((char *)task->thread.kregs + regoff[regno]);
	else
		return 0;
	return *addr;
}

/*
 * Write contents of register REGNO in task TASK.
 */
static inline int put_reg(struct task_struct *task, int regno,
			  unsigned long data)
{
	unsigned long *addr;

	if (regoff[regno] == -1)
		return -1;
	else if (regno < sizeof(regoff)/sizeof(regoff[0]))
		addr = (unsigned long *)((char *)task->thread.kregs + regoff[regno]);
	else
		return -1;
	*addr = data;
	return 0;
}

/*
 * Called by kernel/ptrace.c when detaching..
 *
 * Nothing special to do here, no processor debug support.
 */
void ptrace_disable(struct task_struct *child)
{
}

long arch_ptrace(struct task_struct *child, long request, long addr, long data)
{
	int ret;

	switch (request) {
		/* when I and D space are separate, these will need to be fixed. */
		case PTRACE_PEEKTEXT: /* read word at location addr. */ 
		case PTRACE_PEEKDATA: {
			unsigned long tmp;
			int copied;

			PRINTK_DEBUG("%s PEEKTEXT: addr=0x%08x\n", __FUNCTION__,(u32)addr);
			copied = access_process_vm(child, addr, &tmp, sizeof(tmp), 0);
			ret = -EIO;
			if (copied != sizeof(tmp))
				break;
			ret = put_user(tmp,(unsigned long *) data);
			PRINTK_DEBUG("%s PEEKTEXT: rword=0x%08x\n", __FUNCTION__, (u32)tmp);
			break;
		}

		/* read the word at location addr in the USER area. */
		case PTRACE_PEEKUSR: {
			unsigned long tmp;
			
			ret = -EIO;
			if ((addr & 3) || addr < 0 ||
			    addr > sizeof(struct user) - 3)
				break;
			
			PRINTK_DEBUG("%s PEEKUSR: addr=0x%08x\n", __FUNCTION__, (u32)addr);
			tmp = 0;  /* Default return condition */
			addr = addr >> 2; /* temporary hack. */
			ret = -EIO;
			if (addr < sizeof(regoff)/sizeof(regoff[0])) {
				tmp = get_reg(child, addr);
			} else if (addr == PT_TEXT_ADDR/4) {
				tmp = child->mm->start_code;
			} else if (addr == PT_DATA_ADDR/4) {
				tmp = child->mm->start_data;
			} else if (addr == PT_TEXT_END_ADDR/4) {
				tmp = child->mm->end_code;
			} else
				break;
			ret = put_user(tmp,(unsigned long *) data);
			PRINTK_DEBUG("%s PEEKUSR: rdword=0x%08x\n", __FUNCTION__, (u32)tmp);
			break;
		}

		/* when I and D space are separate, this will have to be fixed. */
		case PTRACE_POKETEXT: /* write the word at location addr. */
		case PTRACE_POKEDATA: {
			int copied;

			PRINTK_DEBUG("%s POKETEXT: addr=0x%08x, data=0x%08x\n", __FUNCTION__, (u32)addr, (u32)data);
			ret = 0;
			copied = access_process_vm(child, addr, &data, sizeof(data), 1);
			if (request == PTRACE_POKETEXT)
				cache_push(addr, sizeof(data));
			PRINTK_DEBUG("%s POKETEXT: copied size = %d\n", __FUNCTION__, copied);
			if (copied == sizeof(data))
				break;
			ret = -EIO;
			break;
		}

		case PTRACE_POKEUSR: /* write the word at location addr in the USER area */
			PRINTK_DEBUG("%s POKEUSR: addr=0x%08x, data=0x%08x\n", __FUNCTION__, (u32)addr, (u32)data);
			ret = -EIO;
			if ((addr & 3) || addr < 0 ||
			    addr > sizeof(struct user) - 3)
				break;

			addr = addr >> 2; /* temporary hack. */
			    
			if (addr == PTR_ESTATUS) {
				data &= SR_MASK;
				data |= get_reg(child, PTR_ESTATUS) & ~(SR_MASK);
			}
			if (addr < sizeof(regoff)/sizeof(regoff[0])) {
				if (put_reg(child, addr, data))
					break;
				ret = 0;
				break;
			}
			break;

		case PTRACE_SYSCALL: /* continue and stop at next (return from) syscall */
		case PTRACE_CONT: { /* restart after signal. */

			PRINTK_DEBUG("%s CONT: addr=0x%08x, data=0x%08x\n", __FUNCTION__, (u32)addr, (u32)data);
			ret = -EIO;
			if ((unsigned long) data > _NSIG)
				break;
			if (request == PTRACE_SYSCALL)
				set_tsk_thread_flag(child, TIF_SYSCALL_TRACE);
			else
				clear_tsk_thread_flag(child, TIF_SYSCALL_TRACE);
			child->exit_code = data;
			PRINTK_DEBUG("%s CONT: About to run wake_up_process()\n", __FUNCTION__);
			wake_up_process(child);
			ret = 0;
			break;
		}

		/*
		 * make the child exit.  Best I can do is send it a sigkill. 
		 * perhaps it should be put in the status that it wants to 
		 * exit.
		 */
		case PTRACE_KILL: {

			PRINTK_DEBUG("%s KILL\n", __FUNCTION__);
			ret = 0;
			if (child->state == EXIT_ZOMBIE) /* already dead */
				break;
			child->exit_code = SIGKILL;
			wake_up_process(child);
			break;
		}

		case PTRACE_DETACH:	/* detach a process that was attached. */
			PRINTK_DEBUG("%s DETACH\n", __FUNCTION__);
			ret = ptrace_detach(child, data);
			break;

		case PTRACE_GETREGS: { /* Get all gp regs from the child. */
		  	int i;
			unsigned long tmp;

			PRINTK_DEBUG("%s GETREGS\n", __FUNCTION__);
			for (i = 0; i < sizeof(regoff)/sizeof(regoff[0]); i++) {
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

		case PTRACE_SETREGS: { /* Set all gp regs in the child. */
			int i;
			unsigned long tmp;

			PRINTK_DEBUG("%s SETREGS\n", __FUNCTION__);
			for (i = 0; i < sizeof(regoff)/sizeof(regoff[0]); i++) {
			    if (get_user(tmp, (unsigned long *) data)) {
				ret = -EFAULT;
				break;
			    }
			    if (i == PTR_ESTATUS) {
				tmp &= SR_MASK;
				tmp |= get_reg(child, PTR_ESTATUS) & ~(SR_MASK);
			    }
			    put_reg(child, i, tmp);
			    data += sizeof(long);
			}
			ret = 0;
			break;
		}

		default:
			PRINTK_DEBUG("%s Undefined\n", __FUNCTION__);
			ret = -EIO;
			break;
	}
	return ret;
}

asmlinkage void syscall_trace(void)
{
	if (!test_thread_flag(TIF_SYSCALL_TRACE))
		return;
	if (!(current->ptrace & PT_PTRACED))
		return;
	current->exit_code = SIGTRAP;
	current->state = TASK_STOPPED;
	ptrace_notify(SIGTRAP | ((current->ptrace & PT_TRACESYSGOOD)
				 ? 0x80 : 0));
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
