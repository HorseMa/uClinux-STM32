/*
 *	linux/mm/remap.c
 *
 *  Copyright (c) 2001 Lineo, Inc. David McCullough <davidm@lineo.com>
 *  Copyright (c) 2000-2001 D Jeff Dionne <jeff@uClinux.org> ref uClinux 2.0
 *	(C) Copyright 1996 Linus Torvalds
 */

#include <linux/slab.h>
#include <linux/smp_lock.h>
#include <linux/shm.h>
#include <linux/mman.h>
#include <linux/swap.h>

#include <asm/uaccess.h>
#include <asm/pgalloc.h>

extern unsigned long askedalloc, realalloc;

/*
 * Expand (or shrink) an existing mapping, potentially moving it at the
 * same time (controlled by the MREMAP_MAYMOVE flag and available VM space)
 *
 * MREMAP_FIXED option added 5-Dec-1999 by Benjamin LaHaise
 * This option implies MREMAP_MAYMOVE.
 *
 * on uClinux, we only permit changing a mapping's size, and only as long as it stays within the
 * hole allocated by the kmalloc() call in do_mmap_pgoff() and the block is not shareable
 */
unsigned long do_mremap(unsigned long addr,
			unsigned long old_len, unsigned long new_len,
			unsigned long flags, unsigned long new_addr)
{
	struct vm_list_struct *vml = NULL;

	/* insanity checks first */
	if (new_len == 0)
		return (unsigned long) -EINVAL;

	if (flags & MREMAP_FIXED && new_addr != addr)
		return (unsigned long) -EINVAL;

	for (vml = current->mm->vmlist; vml; vml = vml->next)
		if (vml->vma->vm_start == addr)
			goto found;

	return (unsigned long) -EINVAL;

 found:
	if (vml->vma->vm_end != vml->vma->vm_start + old_len)
		return (unsigned long) -EFAULT;

	if (vml->vma->vm_flags & VM_MAYSHARE)
		return (unsigned long) -EPERM;

	if (new_len > ksize((void *) addr))
		return (unsigned long) -ENOMEM;

	/* all checks complete - do it */
	vml->vma->vm_end = vml->vma->vm_start + new_len;

	askedalloc -= old_len;
	askedalloc += new_len;

	return vml->vma->vm_start;
}

/*
 * FIXME: Could do a tradional realloc() in some cases.
 */
asmlinkage unsigned long sys_mremap(unsigned long addr,
				    unsigned long old_len, unsigned long new_len,
				    unsigned long flags, unsigned long new_addr)
{
	return -ENOSYS;
}
