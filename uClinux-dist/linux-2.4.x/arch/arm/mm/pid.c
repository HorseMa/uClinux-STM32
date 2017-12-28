/*
* Filename:    arch/arm/mm/pid.c
* Description: ARM Process ID (PID) functions for Fast Address Space Switching
*              (FASS) in ARM Linux.
* Created:     14/10/2001
* Changes:     19/02/2002 - arm_pid_allocate() modifed to create dummy vmas.
*              29/02/2002 - Fixed arm_pid_allocate() to update the CPU PID
*              register only when the current task is the one receiving the
*              PID.
* Copyright:   (C) 2001, 2002 Adam Wiggins <awiggins@cse.unsw.edu.au>
********************************************************************************
* Notes:
*   - Even with processors that have 128 pids we might want to limit the
*     relocation to the first 64 so we have seperate area's for the text,data,
*     bss, stack, etc relocation and arch_get_unmapped_area() area's for
*     non-MAP_FIXED mmap()'s
*   - This is evily hardcoded anyway, really some arch specific mm code should
*     do the initialision.
********************************************************************************
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public Licence version 2 as
* published by the Free Software Foundation.
*******************************************************************************/

#include <linux/mm.h>
#include <asm/pgalloc.h>

#include <asm/proc/pid.h>

extern void dump_vma(struct mm_struct *mm_p);


#if PID_SIMPLE
/* The next PID to be allocated. Hack for now */
static int next_pid_num = ARMPID_START;
#endif

/* Array of pid_structs for StrongARM's 64 pids */
/* (Initialise at run time, depending on StrongARM or XSCALE) */

static struct pid_struct pids[ARMPID_MAX] = {
	{.number = 0, .mm_count = 0},
	{.number = 1, .mm_count = 0},
	{.number = 2, .mm_count = 0},
	{.number = 3, .mm_count = 0},
	{.number = 4, .mm_count = 0},
	{.number = 5, .mm_count = 0},
	{.number = 6, .mm_count = 0},
	{.number = 7, .mm_count = 0},
	{.number = 8, .mm_count = 0},
	{.number = 9, .mm_count = 0},
	{.number = 10, .mm_count = 0},
	{.number = 11, .mm_count = 0},
	{.number = 12, .mm_count = 0},
	{.number = 13, .mm_count = 0},
	{.number = 14, .mm_count = 0},
	{.number = 15, .mm_count = 0},
	{.number = 16, .mm_count = 0},
	{.number = 17, .mm_count = 0},
	{.number = 18, .mm_count = 0},
	{.number = 19, .mm_count = 0},
	{.number = 20, .mm_count = 0},
	{.number = 21, .mm_count = 0},
	{.number = 22, .mm_count = 0},
	{.number = 23, .mm_count = 0},
	{.number = 24, .mm_count = 0},
	{.number = 25, .mm_count = 0},
	{.number = 26, .mm_count = 0},
	{.number = 27, .mm_count = 0},
	{.number = 28, .mm_count = 0},
	{.number = 29, .mm_count = 0},
	{.number = 30, .mm_count = 0},
	{.number = 31, .mm_count = 0},
	{.number = 32, .mm_count = 0},
	{.number = 33, .mm_count = 0},
	{.number = 34, .mm_count = 0},
	{.number = 35, .mm_count = 0},
	{.number = 36, .mm_count = 0},
	{.number = 37, .mm_count = 0},
	{.number = 38, .mm_count = 0},
	{.number = 39, .mm_count = 0},
	{.number = 40, .mm_count = 0},
	{.number = 41, .mm_count = 0},
	{.number = 42, .mm_count = 0},
	{.number = 43, .mm_count = 0},
	{.number = 44, .mm_count = 0},
	{.number = 45, .mm_count = 0},
	{.number = 46, .mm_count = 0},
	{.number = 47, .mm_count = 0},
	{.number = 48, .mm_count = 0},
	{.number = 49, .mm_count = 0},
	{.number = 50, .mm_count = 0},
	{.number = 51, .mm_count = 0},
	{.number = 52, .mm_count = 0},
	{.number = 53, .mm_count = 0},
	{.number = 54, .mm_count = 0},
	{.number = 55, .mm_count = 0},
	{.number = 56, .mm_count = 0},
	{.number = 57, .mm_count = 0},
	{.number = 58, .mm_count = 0},
	{.number = 59, .mm_count = 0},
	{.number = 60, .mm_count = 0},
	{.number = 61, .mm_count = 0},
	{.number = 62, .mm_count = 0},
	{.number = 63, .mm_count = 0},
};


/* arm_pid_allocate: Allocates an ARM PID to 'mm_p' address-space iff
 *   the address-space is marked as relocatable. The location of the
 *   stack dictates this relocatability. If the stack is within the
 *   32MB relocation area the address-space will be allocated a ARM
 *   PID, otherwise the zero PID.  If a pid is selected, two dummy,
 *   that is unaccessable, vm_areas are added. The first covers the
 *   32MB relocation synonym to stop that area from being
 *   mmap()'able. The second tries to protection from the stack
 *   overrunning into the bss segment or visa versa.  These dummy
 *   va_areas are not copied by fork(). On fork(), a new PID will be
 *   allocated as well as the new dummy vm_areas to go with it.
 *
 * Notes:

 *   - This function is only called by
 *     arch/arm/mmu_context.c:init_new_context() which is in turn called
 *     by fs/exec.c:exec_mmap() and kernel/fork.c:copy_mm()

 *   - fs/exec.c:exec_mmap() seems to have no way of indicating to
 *     init_new_context()/allocate_pid() any information to decide if
 *     a newly exec'ed process should have an ARM PID allocated or
 *     not. We need some way to indicate to the STACK_TOP macro
 *     whether a task should be relocated or not.
 *
 *   - Simply allocates ARM PIDs from 1 through to 63. A loop selects
 *     a PID with the least count, that is least number of processes
 *     allocated to it. This should be reexamined to see if a more
 *     appropriate method is avaliable.  ie, we might want to make sure
 *     on fork() that the new childs ARM PID is different to the parents
 *     one.
 */
void
arm_pid_allocate(struct task_struct *task_p, struct mm_struct *mm_p)
{
	int i;
	int pid_num = 0;
	unsigned long vm_flags;
	struct vm_area_struct *vma_p;

#if DEBUG_FASS
	printk("** arm_pid_allocate: mm_p 0x%p vmas %d **\n", mm_p, mm_p->map_count);
	dump_vma(mm_p);
#endif
#if PID_ENABLE
	if (mm_p->start_stack > ARMPID_TASK_SIZE) { 
		/* No pid allocated to parent */
		goto allocate; /* so PID remains zero */
	}

#else
	goto allocate;
#endif

	/* Select an ARM PID for allocation */
#if PID_SIMPLE
	/* Just round robin between the PIDs */
	pid_num = next_pid_num;
	next_pid_num = (next_pid_num >= (ARMPID_MAX - 1) ?
			  ARMPID_START : next_pid_num + 1);
#else
	/* Select the PID with fewest users */
	for (i = pid_num = 1; i < ARMPID_MAX; i++) {
		if (pids[i].mm_count == 0) {
			pid_num = i;
			break;
		}
		if (pids[pid_num].mm_count > pids[i].mm_count) {
			pid_num = i;
		}
	}
#endif

#ifdef DEBUG_FASS
	printk("** arm_pid_allocate: about to mmap dummy regions **\n");
#endif

	/* Map 4KB dummy vma into area between stack and bss. */ 
	vm_flags = VM_DONTEXPAND | VM_DONTCOPY; /* Stops fork copying */
	vma_p = kmem_cache_alloc(vm_area_cachep, SLAB_KERNEL);

	fassert(vma_p);

	down_write(&mm_p->mmap_sem);
	vma_p->vm_mm = mm_p;
	vma_p->vm_start = ARMPID_BRK_LIMIT;
	vma_p->vm_end = ARMPID_BRK_LIMIT + PAGE_SIZE;
	vma_p->vm_flags = vm_flags;
	vma_p->vm_page_prot = protection_map[vm_flags & 0x0f];
	vma_p->vm_ops = NULL;
	vma_p->vm_pgoff = 0;
	vma_p->vm_file = NULL;
	vma_p->vm_private_data = NULL;
	vma_p->vm_sharing_data = NULL;
	vma_p->vm_raend = 0;

	insert_vm_struct(mm_p, vma_p);
	up_write(&mm_p->mmap_sem);

	mm_p->total_vm++; /* Add a page */


#ifdef DEBUG_FASS
	printk("** arm_pid_allocate: Managed first mmap at 0x%lx **\n",
		 ARMPID_TASK_SIZE - MEGABYTE(1));
#endif
	/* Map 32MB dummy vma into relocation point of addres-space */
	vm_flags = VM_DONTEXPAND | VM_DONTCOPY; /* Stops fork copying */ 

	vma_p = kmem_cache_alloc(vm_area_cachep, SLAB_KERNEL);

	fassert(vma_p);

	down_write(&mm_p->mmap_sem);
	vma_p->vm_mm = mm_p;
	vma_p->vm_start = PIDNUM_TO_PID(pid_num);
	vma_p->vm_end = vma_p->vm_start + ARMPID_TASK_SIZE;
	vma_p->vm_flags = vm_flags;
	vma_p->vm_page_prot = protection_map[vm_flags & 0x0f];
	vma_p->vm_ops = NULL;
	vma_p->vm_pgoff = 0;
	vma_p->vm_file = NULL;
	vma_p->vm_private_data = NULL;
	vma_p->vm_sharing_data = NULL;
	vma_p->vm_raend = 0;
  
	insert_vm_struct(mm_p, vma_p);
	up_write(&mm_p->mmap_sem);

	mm_p->total_vm += ARMPID_TASK_SIZE >> PAGE_SHIFT;

#ifdef DEBUG_FASS
	printk("** arm_pid_allocate: Managed second mmap at 0x%x**\n",
		 PIDNUM_TO_PID(pid_num));
#endif

	/* Allocate ARM PID to address-space */
allocate:

	mm_p->context.pid_p = &pids[pid_num];
	mm_p->context.pid = PIDNUM_TO_PID(pid_num);
	pids[pid_num].mm_count++;
  
	if (current == task_p) { /* If this task is getting the new mm set pid */
		set_armpid(mm_p->context.pid);  
	}

#ifdef DEBUG_FASS
	printk("** arm_pid_allocate: allocated ARM PID %d to mm_p 0x%p**\n",
	      PID_TO_PIDNUM(get_armpid()), mm_p);
	dump_vma(mm_p);
#endif
}

/* arm_pid_unallocate: Simply deallocates an ARM PID from an address space.
 */
void
arm_pid_unallocate(struct pid_struct* pid_p)
{
	fassert(pid_p->mm_count > 0);
	//printk("** arm_unallocate_pid: unallocating ARM PID %d **\n",
	//	 PID_TO_PIDNUM(get_armpid()));

	pid_p->mm_count--;

	set_armpid(0x0);

}
