/*
 *	linux/mm/mmap.c
 *
 * Written by obz.
 *
 * NO_MM
 *  Copyright (c) 2004-2005 David Howells <dhowells@redhat.com>
 *  Copyright (c) 2001-2005 David McCullough <davidm@snapgear.com>
 *  Copyright (c) 2001 Lineo, Inc. David McCullough <davidm@lineo.com>
 *  Copyright (c) 2000-2001 D Jeff Dionne <jeff@uClinux.org> ref uClinux 2.0
 */
#include <linux/slab.h>
#include <linux/shm.h>
#include <linux/mman.h>
#include <linux/pagemap.h>
#include <linux/swap.h>
#include <linux/swapctl.h>
#include <linux/smp_lock.h>
#include <linux/init.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/personality.h>
#include <linux/mount.h>
#include <linux/rbtree.h>

#include <asm/uaccess.h>
#include <asm/pgalloc.h>

#ifndef NO_MM
/*
 * WARNING: the debugging will use recursive algorithms so never enable this
 * unless you know what you are doing.
 */
#undef DEBUG_MM_RB

/* description of effects of mapping type and prot in current implementation.
 * this is due to the limited x86 page protection hardware.  The expected
 * behavior is in parens:
 *
 * map_type	prot
 *		PROT_NONE	PROT_READ	PROT_WRITE	PROT_EXEC
 * MAP_SHARED	r: (no) no	r: (yes) yes	r: (no) yes	r: (no) yes
 *		w: (no) no	w: (no) no	w: (yes) yes	w: (no) no
 *		x: (no) no	x: (no) yes	x: (no) yes	x: (yes) yes
 *		
 * MAP_PRIVATE	r: (no) no	r: (yes) yes	r: (no) yes	r: (no) yes
 *		w: (no) no	w: (no) no	w: (copy) copy	w: (no) no
 *		x: (no) no	x: (no) yes	x: (no) yes	x: (yes) yes
 *
 */
pgprot_t protection_map[16] = {
	__P000, __P001, __P010, __P011, __P100, __P101, __P110, __P111,
	__S000, __S001, __S010, __S011, __S100, __S101, __S110, __S111
};

int sysctl_overcommit_memory;
int max_map_count = DEFAULT_MAX_MAP_COUNT;

/* Check that a process has enough memory to allocate a
 * new virtual mapping.
 */
int vm_enough_memory(long pages)
{
	/* Stupid algorithm to decide if we have enough memory: while
	 * simple, it hopefully works in most obvious cases.. Easy to
	 * fool it, but this should catch most mistakes.
	 */
	/* 23/11/98 NJC: Somewhat less stupid version of algorithm,
	 * which tries to do "TheRightThing".  Instead of using half of
	 * (buffers+cache), use the minimum values.  Allow an extra 2%
	 * of num_physpages for safety margin.
	 */

	unsigned long free;
	
        /* Sometimes we want to use more memory than we have. */
	if (sysctl_overcommit_memory)
	    return 1;

	/* The page cache contains buffer pages these days.. */
	free = page_cache_size;
	free += nr_free_pages();
	free += nr_swap_pages;

	/*
	 * This double-counts: the nrpages are both in the page-cache
	 * and in the swapper space. At the same time, this compensates
	 * for the swap-space over-allocation (ie "nr_swap_pages" being
	 * too small.
	 */
	free += swapper_space.nrpages;

	/*
	 * The code below doesn't account for free space in the inode
	 * and dentry slab cache, slab cache fragmentation, inodes and
	 * dentries which will become freeable under VM load, etc.
	 * Lets just hope all these (complex) factors balance out...
	 */
	free += (dentry_stat.nr_unused * sizeof(struct dentry)) >> PAGE_SHIFT;
	free += (inodes_stat.nr_unused * sizeof(struct inode)) >> PAGE_SHIFT;

	return free > pages;
}

/* Remove one vm structure from the inode's i_mapping address space. */
static inline void __remove_shared_vm_struct(struct vm_area_struct *vma)
{
	struct file * file = vma->vm_file;

	if (file) {
		struct inode *inode = file->f_dentry->d_inode;
		if (vma->vm_flags & VM_DENYWRITE)
			atomic_inc(&inode->i_writecount);
		if(vma->vm_next_share)
			vma->vm_next_share->vm_pprev_share = vma->vm_pprev_share;
		*vma->vm_pprev_share = vma->vm_next_share;
	}
}

static inline void remove_shared_vm_struct(struct vm_area_struct *vma)
{
	lock_vma_mappings(vma);
	__remove_shared_vm_struct(vma);
	unlock_vma_mappings(vma);
}

void lock_vma_mappings(struct vm_area_struct *vma)
{
	struct address_space *mapping;

	mapping = NULL;
	if (vma->vm_file)
		mapping = vma->vm_file->f_dentry->d_inode->i_mapping;
	if (mapping)
		spin_lock(&mapping->i_shared_lock);
}

void unlock_vma_mappings(struct vm_area_struct *vma)
{
	struct address_space *mapping;

	mapping = NULL;
	if (vma->vm_file)
		mapping = vma->vm_file->f_dentry->d_inode->i_mapping;
	if (mapping)
		spin_unlock(&mapping->i_shared_lock);
}

#endif /* NO_MM */

/*
 *  sys_brk() for the most part doesn't need the global kernel
 *  lock, except when an application is doing something nasty
 *  like trying to un-brk an area that has already been mapped
 *  to a regular file.  in this case, the unmapping will need
 *  to invoke file system routines that need the global lock.
 */
asmlinkage unsigned long sys_brk(unsigned long brk)
{
#ifndef NO_MM
	unsigned long rlim, retval;
	unsigned long newbrk, oldbrk;
	struct mm_struct *mm = current->mm;

	down_write(&mm->mmap_sem);

	if (brk < mm->end_code)
		goto out;
	newbrk = PAGE_ALIGN(brk);
	oldbrk = PAGE_ALIGN(mm->brk);
	if (oldbrk == newbrk)
		goto set_brk;

	/* Always allow shrinking brk. */
	if (brk <= mm->brk) {
		if (!do_munmap(mm, newbrk, oldbrk-newbrk))
			goto set_brk;
		goto out;
	}

	/* Check against rlimit.. */
	rlim = current->rlim[RLIMIT_DATA].rlim_cur;
	if (rlim < RLIM_INFINITY && brk - mm->start_data > rlim)
		goto out;

	/* Check against existing mmap mappings. */
	if (find_vma_intersection(mm, oldbrk, newbrk+PAGE_SIZE))
		goto out;

	/* Check if we have enough memory.. */
	if (!vm_enough_memory((newbrk-oldbrk) >> PAGE_SHIFT))
		goto out;

	/* Ok, looks good - let it rip. */
	if (do_brk(oldbrk, newbrk-oldbrk) != oldbrk)
		goto out;
set_brk:
	mm->brk = brk;
out:
	retval = mm->brk;
	up_write(&mm->mmap_sem);
	return retval;
#else
	struct mm_struct *mm = current->mm;

	if (brk < mm->start_brk || brk > mm->end_brk)
		return mm->brk;

	if (mm->brk == brk)
		return mm->brk;

	/*
	 * Always allow shrinking brk
	 */
	if (brk <= mm->brk) {
		mm->brk = brk;
		return brk;
	}

	/*
	 * Ok, looks good - let it rip.
	 */
	return mm->brk = brk;
#endif
}


/* Combine the mmap "prot" and "flags" argument into one "vm_flags" used
 * internally. Essentially, translate the "PROT_xxx" and "MAP_xxx" bits
 * into "VM_xxx".
 */
static inline unsigned long calc_vm_flags(unsigned long prot, unsigned long flags)
{
#define _trans(x,bit1,bit2) \
((bit1==bit2)?(x&bit1):(x&bit1)?bit2:0)

	unsigned long prot_bits, flag_bits;
	prot_bits =
		_trans(prot, PROT_READ, VM_READ) |
		_trans(prot, PROT_WRITE, VM_WRITE) |
		_trans(prot, PROT_EXEC, VM_EXEC);
	flag_bits =
		_trans(flags, MAP_GROWSDOWN, VM_GROWSDOWN) |
		_trans(flags, MAP_DENYWRITE, VM_DENYWRITE) |
		_trans(flags, MAP_EXECUTABLE, VM_EXECUTABLE);
	return prot_bits | flag_bits;
#undef _trans
}

#ifndef NO_MM

#ifdef DEBUG_MM_RB
static int browse_rb(rb_node_t * rb_node) {
	int i = 0;
	if (rb_node) {
		i++;
		i += browse_rb(rb_node->rb_left);
		i += browse_rb(rb_node->rb_right);
	}
	return i;
}

static void validate_mm(struct mm_struct * mm) {
	int bug = 0;
	int i = 0;
	struct vm_area_struct * tmp = mm->mmap;
	while (tmp) {
		tmp = tmp->vm_next;
		i++;
	}
	if (i != mm->map_count)
		printk("map_count %d vm_next %d\n", mm->map_count, i), bug = 1;
	i = browse_rb(mm->mm_rb.rb_node);
	if (i != mm->map_count)
		printk("map_count %d rb %d\n", mm->map_count, i), bug = 1;
	if (bug)
		BUG();
}
#else
#define validate_mm(mm) do { } while (0)
#endif

static struct vm_area_struct * find_vma_prepare(struct mm_struct * mm, unsigned long addr,
						struct vm_area_struct ** pprev,
						rb_node_t *** rb_link, rb_node_t ** rb_parent)
{
	struct vm_area_struct * vma;
	rb_node_t ** __rb_link, * __rb_parent, * rb_prev;

	__rb_link = &mm->mm_rb.rb_node;
	rb_prev = __rb_parent = NULL;
	vma = NULL;

	while (*__rb_link) {
		struct vm_area_struct *vma_tmp;

		__rb_parent = *__rb_link;
		vma_tmp = rb_entry(__rb_parent, struct vm_area_struct, vm_rb);

		if (vma_tmp->vm_end > addr) {
			vma = vma_tmp;
			if (vma_tmp->vm_start <= addr)
				return vma;
			__rb_link = &__rb_parent->rb_left;
		} else {
			rb_prev = __rb_parent;
			__rb_link = &__rb_parent->rb_right;
		}
	}

	*pprev = NULL;
	if (rb_prev)
		*pprev = rb_entry(rb_prev, struct vm_area_struct, vm_rb);
	*rb_link = __rb_link;
	*rb_parent = __rb_parent;
	return vma;
}

static inline void __vma_link_list(struct mm_struct * mm, struct vm_area_struct * vma, struct vm_area_struct * prev,
				   rb_node_t * rb_parent)
{
	if (prev) {
		vma->vm_next = prev->vm_next;
		prev->vm_next = vma;
	} else {
		mm->mmap = vma;
		if (rb_parent)
			vma->vm_next = rb_entry(rb_parent, struct vm_area_struct, vm_rb);
		else
			vma->vm_next = NULL;
	}
}

static inline void __vma_link_rb(struct mm_struct * mm, struct vm_area_struct * vma,
				 rb_node_t ** rb_link, rb_node_t * rb_parent)
{
	rb_link_node(&vma->vm_rb, rb_parent, rb_link);
	rb_insert_color(&vma->vm_rb, &mm->mm_rb);
}

static inline void __vma_link_file(struct vm_area_struct * vma)
{
	struct file * file;

	file = vma->vm_file;
	if (file) {
		struct inode * inode = file->f_dentry->d_inode;
		struct address_space *mapping = inode->i_mapping;
		struct vm_area_struct **head;

		if (vma->vm_flags & VM_DENYWRITE)
			atomic_dec(&inode->i_writecount);

		head = &mapping->i_mmap;
		if (vma->vm_flags & VM_SHARED)
			head = &mapping->i_mmap_shared;
      
		/* insert vma into inode's share list */
		if((vma->vm_next_share = *head) != NULL)
			(*head)->vm_pprev_share = &vma->vm_next_share;
		*head = vma;
		vma->vm_pprev_share = head;
	}
}

static void __vma_link(struct mm_struct * mm, struct vm_area_struct * vma,  struct vm_area_struct * prev,
		       rb_node_t ** rb_link, rb_node_t * rb_parent)
{
	__vma_link_list(mm, vma, prev, rb_parent);
	__vma_link_rb(mm, vma, rb_link, rb_parent);
	__vma_link_file(vma);
}

static inline void vma_link(struct mm_struct * mm, struct vm_area_struct * vma, struct vm_area_struct * prev,
			    rb_node_t ** rb_link, rb_node_t * rb_parent)
{
	lock_vma_mappings(vma);
	spin_lock(&mm->page_table_lock);
	__vma_link(mm, vma, prev, rb_link, rb_parent);
	spin_unlock(&mm->page_table_lock);
	unlock_vma_mappings(vma);

	mm->map_count++;
	validate_mm(mm);
}

static int vma_merge(struct mm_struct * mm, struct vm_area_struct * prev,
		     rb_node_t * rb_parent, unsigned long addr, unsigned long end, unsigned long vm_flags)
{
	spinlock_t * lock = &mm->page_table_lock;
	if (!prev) {
		prev = rb_entry(rb_parent, struct vm_area_struct, vm_rb);
		goto merge_next;
	}
	if (prev->vm_end == addr && can_vma_merge(prev, vm_flags)) {
		struct vm_area_struct * next;

		spin_lock(lock);
		prev->vm_end = end;
		next = prev->vm_next;
		if (next && prev->vm_end == next->vm_start && can_vma_merge(next, vm_flags)) {
			prev->vm_end = next->vm_end;
			__vma_unlink(mm, next, prev);
			spin_unlock(lock);

			mm->map_count--;
			kmem_cache_free(vm_area_cachep, next);
			return 1;
		}
		spin_unlock(lock);
		return 1;
	}

	prev = prev->vm_next;
	if (prev) {
 merge_next:
		if (!can_vma_merge(prev, vm_flags))
			return 0;
		if (end == prev->vm_start) {
			spin_lock(lock);
			prev->vm_start = addr;
			spin_unlock(lock);
			return 1;
		}
	}

	return 0;
}

unsigned long do_mmap_pgoff(struct file * file, unsigned long addr, unsigned long len,
	unsigned long prot, unsigned long flags, unsigned long pgoff)
{
	struct mm_struct * mm = current->mm;
	struct vm_area_struct * vma, * prev;
	unsigned int vm_flags;
	int correct_wcount = 0;
	int error;
	rb_node_t ** rb_link, * rb_parent;

	if (file) {
		if (!file->f_op || !file->f_op->mmap)
			return -ENODEV;

		if ((prot & PROT_EXEC) && (file->f_vfsmnt->mnt_flags & MNT_NOEXEC))
			return -EPERM;
	}

	if (!len)
		return addr;

	len = PAGE_ALIGN(len);

	if (len > TASK_SIZE || len == 0)
		return -EINVAL;

	/* offset overflow? */
	if ((pgoff + (len >> PAGE_SHIFT)) < pgoff)
		return -EINVAL;

	/* Too many mappings? */
	if (mm->map_count > max_map_count)
		return -ENOMEM;

	/* Obtain the address to map to. we verify (or select) it and ensure
	 * that it represents a valid section of the address space.
	 */
	addr = get_unmapped_area(file, addr, len, pgoff, flags);
	if (addr & ~PAGE_MASK)
		return addr;

	/* Do simple checking here so the lower-level routines won't have
	 * to. we assume access permissions have been handled by the open
	 * of the memory object, so we don't do any here.
	 */
	vm_flags = calc_vm_flags(prot,flags) | mm->def_flags | VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC;

	/* mlock MCL_FUTURE? */
	if (vm_flags & VM_LOCKED) {
		unsigned long locked = mm->locked_vm << PAGE_SHIFT;
		locked += len;
		if (locked > current->rlim[RLIMIT_MEMLOCK].rlim_cur)
			return -EAGAIN;
	}

	if (file) {
		switch (flags & MAP_TYPE) {
		case MAP_SHARED:
			if ((prot & PROT_WRITE) && !(file->f_mode & FMODE_WRITE))
				return -EACCES;

			/* Make sure we don't allow writing to an append-only file.. */
			if (IS_APPEND(file->f_dentry->d_inode) && (file->f_mode & FMODE_WRITE))
				return -EACCES;

			/* make sure there are no mandatory locks on the file. */
			if (locks_verify_locked(file->f_dentry->d_inode))
				return -EAGAIN;

			vm_flags |= VM_SHARED | VM_MAYSHARE;
			if (!(file->f_mode & FMODE_WRITE))
				vm_flags &= ~(VM_MAYWRITE | VM_SHARED);

			/* fall through */
		case MAP_PRIVATE:
			if (!(file->f_mode & FMODE_READ))
				return -EACCES;
			break;

		default:
			return -EINVAL;
		}
	} else {
		vm_flags |= VM_SHARED | VM_MAYSHARE;
		switch (flags & MAP_TYPE) {
		default:
			return -EINVAL;
		case MAP_PRIVATE:
			vm_flags &= ~(VM_SHARED | VM_MAYSHARE);
			/* fall through */
		case MAP_SHARED:
			break;
		}
	}

	/* Clear old maps */
munmap_back:
	vma = find_vma_prepare(mm, addr, &prev, &rb_link, &rb_parent);
	if (vma && vma->vm_start < addr + len) {
		if (do_munmap(mm, addr, len))
			return -ENOMEM;
		goto munmap_back;
	}

	/* Check against address space limit. */
	if ((mm->total_vm << PAGE_SHIFT) + len
	    > current->rlim[RLIMIT_AS].rlim_cur)
		return -ENOMEM;

	/* Private writable mapping? Check memory availability.. */
	if ((vm_flags & (VM_SHARED | VM_WRITE)) == VM_WRITE &&
	    !(flags & MAP_NORESERVE)				 &&
	    !vm_enough_memory(len >> PAGE_SHIFT))
		return -ENOMEM;

	/* Can we just expand an old anonymous mapping? */
	if (!file && !(vm_flags & VM_SHARED) && rb_parent)
		if (vma_merge(mm, prev, rb_parent, addr, addr + len, vm_flags))
			goto out;

	/* Determine the object being mapped and call the appropriate
	 * specific mapper. the address has already been validated, but
	 * not unmapped, but the maps are removed from the list.
	 */
	vma = kmem_cache_alloc(vm_area_cachep, SLAB_KERNEL);
	if (!vma)
		return -ENOMEM;

	vma->vm_mm = mm;
	vma->vm_start = addr;
	vma->vm_end = addr + len;
	vma->vm_flags = vm_flags;
	vma->vm_page_prot = protection_map[vm_flags & 0x0f];
	vma->vm_ops = NULL;
	vma->vm_pgoff = pgoff;
	vma->vm_file = NULL;
	vma->vm_private_data = NULL;
	vma->vm_raend = 0;

	if (file) {
		error = -EINVAL;
		if (vm_flags & (VM_GROWSDOWN|VM_GROWSUP))
			goto free_vma;
		if (vm_flags & VM_DENYWRITE) {
			error = deny_write_access(file);
			if (error)
				goto free_vma;
			correct_wcount = 1;
		}
		vma->vm_file = file;
		get_file(file);
		error = file->f_op->mmap(file, vma);
		if (error)
			goto unmap_and_free_vma;
	} else if (flags & MAP_SHARED) {
		error = shmem_zero_setup(vma);
		if (error)
			goto free_vma;
	}

	/* Can addr have changed??
	 *
	 * Answer: Yes, several device drivers can do it in their
	 *         f_op->mmap method. -DaveM
	 */
	if (addr != vma->vm_start) {
		/*
		 * It is a bit too late to pretend changing the virtual
		 * area of the mapping, we just corrupted userspace
		 * in the do_munmap, so FIXME (not in 2.4 to avoid breaking
		 * the driver API).
		 */
		struct vm_area_struct * stale_vma;
		/* Since addr changed, we rely on the mmap op to prevent 
		 * collisions with existing vmas and just use find_vma_prepare 
		 * to update the tree pointers.
		 */
		addr = vma->vm_start;
		stale_vma = find_vma_prepare(mm, addr, &prev,
						&rb_link, &rb_parent);
		/*
		 * Make sure the lowlevel driver did its job right.
		 */
		if (unlikely(stale_vma && stale_vma->vm_start < vma->vm_end)) {
			printk(KERN_ERR "buggy mmap operation: [<%p>]\n",
				file ? file->f_op->mmap : NULL);
			BUG();
		}
	}

	vma_link(mm, vma, prev, rb_link, rb_parent);
	if (correct_wcount)
		atomic_inc(&file->f_dentry->d_inode->i_writecount);

out:	
	mm->total_vm += len >> PAGE_SHIFT;
	if (vm_flags & VM_LOCKED) {
		mm->locked_vm += len >> PAGE_SHIFT;
		make_pages_present(addr, addr + len);
	}
	return addr;

unmap_and_free_vma:
	if (correct_wcount)
		atomic_inc(&file->f_dentry->d_inode->i_writecount);
	vma->vm_file = NULL;
	if(file)
	  fput(file);

	/* Undo any partial mapping done by a device driver. */
	zap_page_range(mm, vma->vm_start, vma->vm_end - vma->vm_start);
free_vma:
	kmem_cache_free(vm_area_cachep, vma);
	return error;
}

/* Get an address range which is currently unmapped.
 * For shmat() with addr=0.
 *
 * Ugly calling convention alert:
 * Return value with the low bits set means error value,
 * ie
 *	if (ret & ~PAGE_MASK)
 *		error = ret;
 *
 * This function "knows" that -ENOMEM has the bits set.
 */
#ifndef HAVE_ARCH_UNMAPPED_AREA
static inline unsigned long arch_get_unmapped_area(struct file *filp, unsigned long addr, unsigned long len, unsigned long pgoff, unsigned long flags)
{
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma;
	int found_hole = 0;

	if (len > TASK_SIZE)
		return -ENOMEM;

	if (addr) {
		addr = PAGE_ALIGN(addr);
		vma = find_vma(mm, addr);
		if (TASK_SIZE - len >= addr &&
		    (!vma || addr + len <= vma->vm_start))
			return addr;
	}
	addr = PAGE_ALIGN(TASK_UNMAPPED_BASE);

	for (vma = find_vma(mm, addr); ; vma = vma->vm_next) {
		/* At this point:  (!vma || addr < vma->vm_end). */
		if (TASK_SIZE - len < addr)
			return -ENOMEM;
		if (!vma || addr + len <= vma->vm_start)
			return addr;
		addr = vma->vm_end;
	}
}
#else
extern unsigned long arch_get_unmapped_area(struct file *, unsigned long, unsigned long, unsigned long, unsigned long);
#endif	

unsigned long get_unmapped_area(struct file *file, unsigned long addr, unsigned long len, unsigned long pgoff, unsigned long flags)
{
  	unsigned long retval;

	if (flags & MAP_FIXED) {
		if (addr > TASK_SIZE - len || addr >= TASK_SIZE)
			return -ENOMEM;
		if (addr & ~PAGE_MASK)
			return -EINVAL;
		return addr;
	}

	if (file && file->f_op && file->f_op->get_unmapped_area) {
	  	retval =  file->f_op->get_unmapped_area(file, addr, len,
						     pgoff, flags);
		/* -ENOSYS will be returned if the device-specific driver
		 * does not implement this function. e.g. framebuffer drivers
		 */
		if (retval != -ENOSYS)
		  	return retval;
	}

	return arch_get_unmapped_area(file, addr, len, pgoff, flags);
}

/* Look up the first VMA which satisfies  addr < vm_end,  NULL if none. */
struct vm_area_struct * find_vma(struct mm_struct * mm, unsigned long addr)
{
	struct vm_area_struct *vma = NULL;

	if (mm) {
		/* Check the cache first. */
		/* (Cache hit rate is typically around 35%.) */
		vma = mm->mmap_cache;
		if (!(vma && vma->vm_end > addr && vma->vm_start <= addr)) {
			rb_node_t * rb_node;

			rb_node = mm->mm_rb.rb_node;
			vma = NULL;

			while (rb_node) {
				struct vm_area_struct * vma_tmp;

				vma_tmp = rb_entry(rb_node, struct vm_area_struct, vm_rb);

				if (vma_tmp->vm_end > addr) {
					vma = vma_tmp;
					if (vma_tmp->vm_start <= addr)
						break;
					rb_node = rb_node->rb_left;
				} else
					rb_node = rb_node->rb_right;
			}
			if (vma)
				mm->mmap_cache = vma;
		}
	}
	return vma;
}

/* Same as find_vma, but also return a pointer to the previous VMA in *pprev. */
struct vm_area_struct * find_vma_prev(struct mm_struct * mm, unsigned long addr,
				      struct vm_area_struct **pprev)
{
	if (mm) {
		/* Go through the RB tree quickly. */
		struct vm_area_struct * vma;
		rb_node_t * rb_node, * rb_last_right, * rb_prev;
		
		rb_node = mm->mm_rb.rb_node;
		rb_last_right = rb_prev = NULL;
		vma = NULL;

		while (rb_node) {
			struct vm_area_struct * vma_tmp;

			vma_tmp = rb_entry(rb_node, struct vm_area_struct, vm_rb);

			if (vma_tmp->vm_end > addr) {
				vma = vma_tmp;
				rb_prev = rb_last_right;
				if (vma_tmp->vm_start <= addr)
					break;
				rb_node = rb_node->rb_left;
			} else {
				rb_last_right = rb_node;
				rb_node = rb_node->rb_right;
			}
		}
		if (vma) {
			if (vma->vm_rb.rb_left) {
				rb_prev = vma->vm_rb.rb_left;
				while (rb_prev->rb_right)
					rb_prev = rb_prev->rb_right;
			}
			*pprev = NULL;
			if (rb_prev)
				*pprev = rb_entry(rb_prev, struct vm_area_struct, vm_rb);
			if ((rb_prev ? (*pprev)->vm_next : mm->mmap) != vma)
				BUG();
			return vma;
		}
	}
	*pprev = NULL;
	return NULL;
}

struct vm_area_struct * find_extend_vma(struct mm_struct * mm, unsigned long addr)
{
	struct vm_area_struct * vma;
	unsigned long start;

	addr &= PAGE_MASK;
	vma = find_vma(mm,addr);
	if (!vma)
		return NULL;
	if (vma->vm_start <= addr)
		return vma;
	if (!(vma->vm_flags & VM_GROWSDOWN))
		return NULL;
	start = vma->vm_start;
	if (expand_stack(vma, addr))
		return NULL;
	if (vma->vm_flags & VM_LOCKED) {
		make_pages_present(addr, start);
	}
	return vma;
}

/* Normal function to fix up a mapping
 * This function is the default for when an area has no specific
 * function.  This may be used as part of a more specific routine.
 * This function works out what part of an area is affected and
 * adjusts the mapping information.  Since the actual page
 * manipulation is done in do_mmap(), none need be done here,
 * though it would probably be more appropriate.
 *
 * By the time this function is called, the area struct has been
 * removed from the process mapping list, so it needs to be
 * reinserted if necessary.
 *
 * The 4 main cases are:
 *    Unmapping the whole area
 *    Unmapping from the start of the segment to a point in it
 *    Unmapping from an intermediate point to the end
 *    Unmapping between to intermediate points, making a hole.
 *
 * Case 4 involves the creation of 2 new areas, for each side of
 * the hole.  If possible, we reuse the existing area rather than
 * allocate a new one, and the return indicates whether the old
 * area was reused.
 */
static struct vm_area_struct * unmap_fixup(struct mm_struct *mm, 
	struct vm_area_struct *area, unsigned long addr, size_t len, 
	struct vm_area_struct *extra)
{
	struct vm_area_struct *mpnt;
	unsigned long end = addr + len;

	area->vm_mm->total_vm -= len >> PAGE_SHIFT;
	if (area->vm_flags & VM_LOCKED)
		area->vm_mm->locked_vm -= len >> PAGE_SHIFT;

	/* Unmapping the whole area. */
	if (addr == area->vm_start && end == area->vm_end) {
		if (area->vm_ops && area->vm_ops->close)
			area->vm_ops->close(area);
		if (area->vm_file)
			fput(area->vm_file);
		kmem_cache_free(vm_area_cachep, area);
		return extra;
	}

	/* Work out to one of the ends. */
	if (end == area->vm_end) {
		/*
		 * here area isn't visible to the semaphore-less readers
		 * so we don't need to update it under the spinlock.
		 */
		area->vm_end = addr;
		lock_vma_mappings(area);
		spin_lock(&mm->page_table_lock);
	} else if (addr == area->vm_start) {
		area->vm_pgoff += (end - area->vm_start) >> PAGE_SHIFT;
		/* same locking considerations of the above case */
		area->vm_start = end;
		lock_vma_mappings(area);
		spin_lock(&mm->page_table_lock);
	} else {
	/* Unmapping a hole: area->vm_start < addr <= end < area->vm_end */
		/* Add end mapping -- leave beginning for below */
		mpnt = extra;
		extra = NULL;

		mpnt->vm_mm = area->vm_mm;
		mpnt->vm_start = end;
		mpnt->vm_end = area->vm_end;
		mpnt->vm_page_prot = area->vm_page_prot;
		mpnt->vm_flags = area->vm_flags;
		mpnt->vm_raend = 0;
		mpnt->vm_ops = area->vm_ops;
		mpnt->vm_pgoff = area->vm_pgoff + ((end - area->vm_start) >> PAGE_SHIFT);
		mpnt->vm_file = area->vm_file;
		mpnt->vm_private_data = area->vm_private_data;
		if (mpnt->vm_file)
			get_file(mpnt->vm_file);
		if (mpnt->vm_ops && mpnt->vm_ops->open)
			mpnt->vm_ops->open(mpnt);
		area->vm_end = addr;	/* Truncate area */

		/* Because mpnt->vm_file == area->vm_file this locks
		 * things correctly.
		 */
		lock_vma_mappings(area);
		spin_lock(&mm->page_table_lock);
		__insert_vm_struct(mm, mpnt);
	}

	__insert_vm_struct(mm, area);
	spin_unlock(&mm->page_table_lock);
	unlock_vma_mappings(area);
	return extra;
}

/*
 * Try to free as many page directory entries as we can,
 * without having to work very hard at actually scanning
 * the page tables themselves.
 *
 * Right now we try to free page tables if we have a nice
 * PGDIR-aligned area that got free'd up. We could be more
 * granular if we want to, but this is fast and simple,
 * and covers the bad cases.
 *
 * "prev", if it exists, points to a vma before the one
 * we just free'd - but there's no telling how much before.
 */
static void free_pgtables(struct mm_struct * mm, struct vm_area_struct *prev,
	unsigned long start, unsigned long end)
{
	unsigned long first = start & PGDIR_MASK;
	unsigned long last = end + PGDIR_SIZE - 1;
	unsigned long start_index, end_index;

	if (!prev) {
		prev = mm->mmap;
		if (!prev)
			goto no_mmaps;
		if (prev->vm_end > start) {
			if (last > prev->vm_start)
				last = prev->vm_start;
			goto no_mmaps;
		}
	}
	for (;;) {
		struct vm_area_struct *next = prev->vm_next;

		if (next) {
			if (next->vm_start < start) {
				prev = next;
				continue;
			}
			if (last > next->vm_start)
				last = next->vm_start;
		}
		if (prev->vm_end > first)
			first = prev->vm_end + PGDIR_SIZE - 1;
		break;
	}
no_mmaps:
	if (last < first)
		return;
	/*
	 * If the PGD bits are not consecutive in the virtual address, the
	 * old method of shifting the VA >> by PGDIR_SHIFT doesn't work.
	 */
	start_index = pgd_index(first);
	end_index = pgd_index(last);
	if (end_index > start_index) {
		clear_page_tables(mm, start_index, end_index - start_index);
		flush_tlb_pgtables(mm, first & PGDIR_MASK, last & PGDIR_MASK);
	}
}

/* Munmap is split into 2 main parts -- this part which finds
 * what needs doing, and the areas themselves, which do the
 * work.  This now handles partial unmappings.
 * Jeremy Fitzhardine <jeremy@sw.oz.au>
 */
int do_munmap(struct mm_struct *mm, unsigned long addr, size_t len)
{
	struct vm_area_struct *mpnt, *prev, **npp, *free, *extra;

	if ((addr & ~PAGE_MASK) || addr >= TASK_SIZE || len > TASK_SIZE-addr)
		return -EINVAL;

	if ((len = PAGE_ALIGN(len)) == 0)
		return -EINVAL;

	/* Check if this memory area is ok - put it on the temporary
	 * list if so..  The checks here are pretty simple --
	 * every area affected in some way (by any overlap) is put
	 * on the list.  If nothing is put on, nothing is affected.
	 */
	mpnt = find_vma_prev(mm, addr, &prev);
	if (!mpnt)
		return 0;
	/* we have  addr < mpnt->vm_end  */

	if (mpnt->vm_start >= addr+len)
		return 0;

	/* If we'll make "hole", check the vm areas limit */
	if ((mpnt->vm_start < addr && mpnt->vm_end > addr+len)
	    && mm->map_count >= max_map_count)
		return -ENOMEM;

	/*
	 * We may need one additional vma to fix up the mappings ... 
	 * and this is the last chance for an easy error exit.
	 */
	extra = kmem_cache_alloc(vm_area_cachep, SLAB_KERNEL);
	if (!extra)
		return -ENOMEM;

	npp = (prev ? &prev->vm_next : &mm->mmap);
	free = NULL;
	spin_lock(&mm->page_table_lock);
	for ( ; mpnt && mpnt->vm_start < addr+len; mpnt = *npp) {
		*npp = mpnt->vm_next;
		mpnt->vm_next = free;
		free = mpnt;
		rb_erase(&mpnt->vm_rb, &mm->mm_rb);
	}
	mm->mmap_cache = NULL;	/* Kill the cache. */
	spin_unlock(&mm->page_table_lock);

	/* Ok - we have the memory areas we should free on the 'free' list,
	 * so release them, and unmap the page range..
	 * If the one of the segments is only being partially unmapped,
	 * it will put new vm_area_struct(s) into the address space.
	 * In that case we have to be careful with VM_DENYWRITE.
	 */
	while ((mpnt = free) != NULL) {
		unsigned long st, end, size;
		struct file *file = NULL;

		free = free->vm_next;

		st = addr < mpnt->vm_start ? mpnt->vm_start : addr;
		end = addr+len;
		end = end > mpnt->vm_end ? mpnt->vm_end : end;
		size = end - st;

		if (mpnt->vm_flags & VM_DENYWRITE &&
		    (st != mpnt->vm_start || end != mpnt->vm_end) &&
		    (file = mpnt->vm_file) != NULL) {
			atomic_dec(&file->f_dentry->d_inode->i_writecount);
		}
		remove_shared_vm_struct(mpnt);
		mm->map_count--;

		zap_page_range(mm, st, size);

		/*
		 * Fix the mapping, and free the old area if it wasn't reused.
		 */
		extra = unmap_fixup(mm, mpnt, st, size, extra);
		if (file)
			atomic_inc(&file->f_dentry->d_inode->i_writecount);
	}
	validate_mm(mm);

	/* Release the extra vma struct if it wasn't used */
	if (extra)
		kmem_cache_free(vm_area_cachep, extra);

	free_pgtables(mm, prev, addr, addr+len);

	return 0;
}

asmlinkage long sys_munmap(unsigned long addr, size_t len)
{
	int ret;
	struct mm_struct *mm = current->mm;

	down_write(&mm->mmap_sem);
	ret = do_munmap(mm, addr, len);
	up_write(&mm->mmap_sem);
	return ret;
}


static inline void verify_mmap_write_lock_held(struct mm_struct *mm)
{
	if (down_read_trylock(&mm->mmap_sem)) {
		WARN_ON(1);
		up_read(&mm->mmap_sem);
	}
}

/*
 *  this is really a simplified "do_mmap".  it only handles
 *  anonymous maps.  eventually we may be able to do some
 *  brk-specific accounting here.
 */
unsigned long do_brk(unsigned long addr, unsigned long len)
{
	struct mm_struct * mm = current->mm;
	struct vm_area_struct * vma, * prev;
	unsigned long flags;
	rb_node_t ** rb_link, * rb_parent;

	len = PAGE_ALIGN(len);
	if (!len)
		return addr;

	if ((addr + len) > TASK_SIZE || (addr + len) < addr)
		return -EINVAL;

	/*
	 * mlock MCL_FUTURE?
	 */
	if (mm->def_flags & VM_LOCKED) {
		unsigned long locked = mm->locked_vm << PAGE_SHIFT;
		locked += len;
		if (locked > current->rlim[RLIMIT_MEMLOCK].rlim_cur)
			return -EAGAIN;
	}

	/*
	 * mm->mmap_sem is required to protect against another thread
	 * changing the mappings while we sleep (on kmalloc for one).
	 */
	verify_mmap_write_lock_held(mm);

	/*
	 * Clear old maps.  this also does some error checking for us
	 */
 munmap_back:
	vma = find_vma_prepare(mm, addr, &prev, &rb_link, &rb_parent);
	if (vma && vma->vm_start < addr + len) {
		if (do_munmap(mm, addr, len))
			return -ENOMEM;
		goto munmap_back;
	}

	/* Check against address space limits *after* clearing old maps... */
	if ((mm->total_vm << PAGE_SHIFT) + len
	    > current->rlim[RLIMIT_AS].rlim_cur)
		return -ENOMEM;

	if (mm->map_count > max_map_count)
		return -ENOMEM;

	if (!vm_enough_memory(len >> PAGE_SHIFT))
		return -ENOMEM;

	flags = VM_DATA_DEFAULT_FLAGS | mm->def_flags;

	/* Can we just expand an old anonymous mapping? */
	if (rb_parent && vma_merge(mm, prev, rb_parent, addr, addr + len, flags))
		goto out;

	/*
	 * create a vma struct for an anonymous mapping
	 */
	vma = kmem_cache_alloc(vm_area_cachep, SLAB_KERNEL);
	if (!vma)
		return -ENOMEM;

	vma->vm_mm = mm;
	vma->vm_start = addr;
	vma->vm_end = addr + len;
	vma->vm_flags = flags;
	vma->vm_page_prot = protection_map[flags & 0x0f];
	vma->vm_ops = NULL;
	vma->vm_pgoff = 0;
	vma->vm_file = NULL;
	vma->vm_private_data = NULL;

	vma_link(mm, vma, prev, rb_link, rb_parent);

out:
	mm->total_vm += len >> PAGE_SHIFT;
	if (flags & VM_LOCKED) {
		mm->locked_vm += len >> PAGE_SHIFT;
		make_pages_present(addr, addr + len);
	}
	return addr;
}

/* Build the RB tree corresponding to the VMA list. */
void build_mmap_rb(struct mm_struct * mm)
{
	struct vm_area_struct * vma;
	rb_node_t ** rb_link, * rb_parent;

	mm->mm_rb = RB_ROOT;
	rb_link = &mm->mm_rb.rb_node;
	rb_parent = NULL;
	for (vma = mm->mmap; vma; vma = vma->vm_next) {
		__vma_link_rb(mm, vma, rb_link, rb_parent);
		rb_parent = &vma->vm_rb;
		rb_link = &rb_parent->rb_right;
	}
}

/* Release all mmaps. */
void exit_mmap(struct mm_struct * mm)
{
	struct vm_area_struct * mpnt;

	release_segments(mm);
	spin_lock(&mm->page_table_lock);
	mpnt = mm->mmap;
	mm->mmap = mm->mmap_cache = NULL;
	mm->mm_rb = RB_ROOT;
	mm->rss = 0;
	spin_unlock(&mm->page_table_lock);
	mm->total_vm = 0;
	mm->locked_vm = 0;

	flush_cache_mm(mm);
	while (mpnt) {
		struct vm_area_struct * next = mpnt->vm_next;
		unsigned long start = mpnt->vm_start;
		unsigned long end = mpnt->vm_end;
		unsigned long size = end - start;

		if (mpnt->vm_ops) {
			if (mpnt->vm_ops->close)
				mpnt->vm_ops->close(mpnt);
		}
		mm->map_count--;
		remove_shared_vm_struct(mpnt);
		zap_page_range(mm, start, size);
		if (mpnt->vm_file)
			fput(mpnt->vm_file);
		kmem_cache_free(vm_area_cachep, mpnt);
		mpnt = next;
	}

	/* This is just debugging */
	if (mm->map_count)
		BUG();

	clear_page_tables(mm, FIRST_USER_PGD_NR, USER_PTRS_PER_PGD);

	flush_tlb_mm(mm);
}

/* Insert vm structure into process list sorted by address
 * and into the inode's i_mmap ring.  If vm_file is non-NULL
 * then the i_shared_lock must be held here.
 */
void __insert_vm_struct(struct mm_struct * mm, struct vm_area_struct * vma)
{
	struct vm_area_struct * __vma, * prev;
	rb_node_t ** rb_link, * rb_parent;

	__vma = find_vma_prepare(mm, vma->vm_start, &prev, &rb_link, &rb_parent);
	if (__vma && __vma->vm_start < vma->vm_end)
		BUG();
	__vma_link(mm, vma, prev, rb_link, rb_parent);
	mm->map_count++;
	validate_mm(mm);
}

int insert_vm_struct(struct mm_struct * mm, struct vm_area_struct * vma)
{
	struct vm_area_struct * __vma, * prev;
	rb_node_t ** rb_link, * rb_parent;

	__vma = find_vma_prepare(mm, vma->vm_start, &prev, &rb_link, &rb_parent);
	if (__vma && __vma->vm_start < vma->vm_end)
		return -ENOMEM;
	vma_link(mm, vma, prev, rb_link, rb_parent);
	validate_mm(mm);
	return 0;
}

#else /* NO_MM */

#ifdef DEBUG
static void show_process_blocks(void)
{
	struct mm_tblock_struct * tblock, *tmp;
	
	printk("Process blocks %d:", current->pid);
	
	tmp = &current->mm->tblock;
	while (tmp) {
		printk(" %p: %p", tmp, tmp->vma);
		if (tmp->vma)
			printk(" (%d @%p #%d)",
			       ksize(tmp->vma->vm_start), tmp->vma->vm_start, tmp->vma->vm_usage);
		if (tmp->next)
			printk(" ->");
		else
			printk(".");
		tmp = tmp->next;
	}
	printk("\n");
}
#endif /* DEBUG */

extern unsigned long askedalloc, realalloc;

/* list of shareable VMAs */
rb_root_t nommu_vma_tree = RB_ROOT;
DECLARE_RWSEM(nommu_vma_sem);

static inline struct vm_area_struct *find_nommu_vma(unsigned long start)
{
	struct vm_area_struct *vma;
	rb_node_t *n = nommu_vma_tree.rb_node;

	while (n) {
		vma = rb_entry(n, struct vm_area_struct, vm_rb);

		if (start < vma->vm_start)
			n = n->rb_left;
		else if (start > vma->vm_start)
			n = n->rb_right;
		else
			return vma;
	}

	return NULL;
}

static void add_nommu_vma(struct vm_area_struct *vma)
{
	struct vm_area_struct *pvma;
	rb_node_t **p = &nommu_vma_tree.rb_node;
	rb_node_t *parent = NULL;

	/* add the VMA to the master list */
	while (*p) {
		parent = *p;
		pvma = rb_entry(parent, struct vm_area_struct, vm_rb);

		if (vma->vm_start < pvma->vm_start) {
			p = &(*p)->rb_left;
		}
		else if (vma->vm_start > pvma->vm_start) {
			p = &(*p)->rb_right;
		}
		else {
			/* mappings are at the same address - this can only
			 * happen for shared-mem chardevs and shared file
			 * mappings backed by ramfs/tmpfs */
			BUG_ON(!(pvma->vm_flags & VM_SHARED));

			if (vma < pvma)
				p = &(*p)->rb_left;
			else if (vma > pvma)
				p = &(*p)->rb_right;
			else
				BUG();
		}
	}

	rb_link_node(&vma->vm_rb, parent, p);
	rb_insert_color(&vma->vm_rb, &nommu_vma_tree);
}

static void delete_nommu_vma(struct vm_area_struct *vma)
{
	/* remove from the master list */
	rb_erase(&vma->vm_rb, &nommu_vma_tree);
}

unsigned long do_mmap_pgoff(struct file *file,
			    unsigned long addr,
			    unsigned long len,
			    unsigned long prot,
			    unsigned long flags,
			    unsigned long pgoff)
{
	struct vm_list_struct *vml = NULL;
	struct vm_area_struct *vma = NULL;
	rb_node_t *rb;
	unsigned int vm_flags;
	void *result;
	int ret, membacked;


	/* do the simple checks first */
	if (flags & MAP_FIXED || addr) {
		printk(KERN_DEBUG "%d: Can't do fixed-address/overlay mmap of RAM\n",
		       current->pid);
		return -EINVAL;
	}

	if (PAGE_ALIGN(len) == 0)
		return addr;

	if (len > TASK_SIZE)
		return -EINVAL;

	/* offset overflow? */
	if ((pgoff + (len >> PAGE_SHIFT)) < pgoff)
		return -EINVAL;

	/* validate file mapping requests */
	membacked = 0;
	if (file) {
		/* files must support mmap */
		if (!file->f_op || !file->f_op->mmap)
			return -ENODEV;

		if ((prot & PROT_EXEC) &&
		    (file->f_vfsmnt->mnt_flags & MNT_NOEXEC))
			return -EPERM;

		/* work out if what we've got could possibly be shared
		 * - we support chardevs that provide their own "memory"
		 * - we support files/blockdevs that are memory backed
		 */
		if (S_ISCHR(file->f_dentry->d_inode->i_mode)) {
			membacked = 1;
		}
		else {
			struct address_space *mapping;

			mapping = file->f_dentry->d_inode->i_mapping;
			if (mapping)
				membacked = mapping->membacked;
		}

#ifdef MAGIC_ROM_PTR
		/*
		 * keep romptr alive a little longer until everything is converted
		 * to membacked
		 */
		if (file->f_op->romptr && !(prot & PROT_WRITE)) {
			struct vm_area_struct my_vma;
			memset(&my_vma, 0, sizeof(my_vma));
			my_vma.vm_file	= file;
			my_vma.vm_flags	= calc_vm_flags(prot,flags);
			my_vma.vm_start	= addr;
			my_vma.vm_end	= addr + len;
			my_vma.vm_pgoff	= pgoff;
			if (file->f_op->romptr(file, &my_vma) == 0)
				membacked = 1;
		}
#endif
		if (flags & MAP_SHARED) {
			/* do checks for writing, appending and locking */
			if ((prot & PROT_WRITE) && !(file->f_mode & FMODE_WRITE))
				return -EACCES;

			if (IS_APPEND(file->f_dentry->d_inode) &&
			    (file->f_mode & FMODE_WRITE))
				return -EACCES;

			if (locks_verify_locked(file->f_dentry->d_inode))
				return -EAGAIN;

			if (!membacked) {
				printk("MAP_SHARED not completely supported on !MMU\n");
				return -EINVAL;
			}

			/* we require greater support from the driver or
			 * filesystem - we ask it to tell us what memory to
			 * use */
			if (!file->f_op->get_unmapped_area)
				return -ENODEV;
		}
		else {
			/* we read private files into memory we allocate */
			if (!file->f_op->read)
				return -ENODEV;
		}
	}

	/* handle PROT_EXEC implication by PROT_READ */
#if 0
	if ((prot & PROT_READ) && (current->personality & READ_IMPLIES_EXEC))
		if (!(file && (file->f_vfsmnt->mnt_flags & MNT_NOEXEC)))
			prot |= PROT_EXEC;
#endif

	/* do simple checking here so the lower-level routines won't have
	 * to. we assume access permissions have been handled by the open
	 * of the memory object, so we don't do any here.
	 */
	vm_flags = calc_vm_flags(prot,flags) /* | mm->def_flags */
		| VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC;

	if (!membacked) {
		/* share any file segment that's mapped read-only */
		if (((flags & MAP_PRIVATE) && !(prot & PROT_WRITE) && file) ||
		    ((flags & MAP_SHARED) && !(prot & PROT_WRITE) && file))
			vm_flags |= VM_MAYSHARE;

		/* refuse to let anyone share files with this process if it's being traced -
		 * otherwise breakpoints set in it may interfere with another untraced process
		 */
		if (current->ptrace & PT_PTRACED)
			vm_flags &= ~(VM_SHARED | VM_MAYSHARE);
	}
	else {
		/* permit sharing of character devices and ramfs files at any time for
		 * anything other than a privately writable mapping
		 */
		if (!(flags & MAP_PRIVATE) || !(prot & PROT_WRITE)) {
			vm_flags |= VM_MAYSHARE;
			if (flags & MAP_SHARED)
				vm_flags |= VM_SHARED;
		}
	}

	/* we're going to need to record the mapping if it works */
	vml = kmalloc(sizeof(struct vm_list_struct), GFP_KERNEL);
	if (!vml)
		goto error_getting_vml;
	memset(vml, 0, sizeof(*vml));

	down_write(&nommu_vma_sem);

	/* if we want to share, we need to search for VMAs created by another
	 * mmap() call that overlap with our proposed mapping
	 * - we can only share with an exact match on most regular files
	 * - shared mappings on character devices and memory backed files are
	 *   permitted to overlap inexactly as far as we are concerned for in
	 *   these cases, sharing is handled in the driver or filesystem rather
	 *   than here
	 */
	if (vm_flags & VM_MAYSHARE) {
		unsigned long pglen = (len + PAGE_SIZE - 1) >> PAGE_SHIFT;
		unsigned long vmpglen;

		for (rb = rb_first(&nommu_vma_tree); rb; rb = rb_next(rb)) {
			vma = rb_entry(rb, struct vm_area_struct, vm_rb);

			if (!(vma->vm_flags & VM_MAYSHARE))
				continue;

			/* search for overlapping mappings on the same file */
			if (vma->vm_file->f_dentry->d_inode != file->f_dentry->d_inode)
				continue;

			if (vma->vm_pgoff >= pgoff + pglen)
				continue;

			vmpglen = (vma->vm_end - vma->vm_start + PAGE_SIZE - 1) >> PAGE_SHIFT;
			if (pgoff >= vma->vm_pgoff + vmpglen)
				continue;

			/* handle inexact matches between mappings */
			if (vmpglen != pglen || vma->vm_pgoff != pgoff) {
				if (!membacked)
					goto sharing_violation;
				continue;
			}

			/* we've found a VMA we can share */
			atomic_inc(&vma->vm_usage);

			vml->vma = vma;
			result = (void *) vma->vm_start;
			goto shared;
		}
	}

	vma = NULL;

	/* obtain the address to map to. we verify (or select) it and ensure
	 * that it represents a valid section of the address space
	 * - this is the hook for quasi-memory character devices
	 */
	if (file && file->f_op->get_unmapped_area) {
		addr = file->f_op->get_unmapped_area(file, addr, len, pgoff, flags);

		if (IS_ERR((void *) addr)) {
			ret = addr;
			if (ret == (unsigned long) -ENOSYS)
				ret = (unsigned long) -ENODEV;
			goto error;
		}
	}

	/* we're going to need a VMA struct as well */
	vma = kmalloc(sizeof(struct vm_area_struct), GFP_KERNEL);
	if (!vma)
		goto error_getting_vma;

	memset(vma, 0, sizeof(*vma));
	atomic_set(&vma->vm_usage, 1);
	if (file)
		get_file(file);
	vma->vm_file	= file;
	vma->vm_flags	= vm_flags;
	vma->vm_start	= addr;
	vma->vm_end	= addr + len;
	vma->vm_pgoff	= pgoff;

	vml->vma = vma;

	/* determine the object being mapped and call the appropriate specific
	 * mapper.
	 */
	if (file) {
#ifdef MAGIC_ROM_PTR
		/* First, try simpler routine designed to give us a ROM pointer. */
		if (file->f_op->romptr && !(prot & PROT_WRITE)) {
			ret = file->f_op->romptr(file, vma);
#ifdef DEBUG
			printk("romptr mmap returned %d (st=%lx)\n",
			       ret, vma->vm_start);
#endif
			result = (void *) vma->vm_start;
			if (!ret)
				goto done;
			else if (ret != -ENOSYS)
				goto error;
		} else
#endif /* MAGIC_ROM_PTR */
		/* Then try full mmap routine, which might return a RAM
		 * pointer, or do something truly complicated
		 */
		if (file->f_op->mmap) {
			ret = file->f_op->mmap(file, vma);

#ifdef DEBUG
			printk("f_op->mmap() returned %d (st=%lx)\n",
			       ret, vma->vm_start);
#endif
			result = (void *) vma->vm_start;
			if (!ret)
				goto done;
			else if (ret != -ENOSYS)
				goto error;
		} else {
			ret = -ENODEV; /* No mapping operations defined */
			goto error;
		}

		/* An ENOSYS error indicates that mmap isn't possible (as
		 * opposed to tried but failed) so we'll fall through to the
		 * copy. */
	}

	/* allocate some memory to hold the mapping
	 * - note that this may not return a page-aligned address if the object
	 *   we're allocating is smaller than a page
	 */
	ret = -ENOMEM;
	result = kmalloc(len, GFP_KERNEL);
	if (!result) {
		printk("Allocation of length %lu from process %d failed\n",
		       len, current->pid);
		show_free_areas();
		goto error;
	}

	vma->vm_start = (unsigned long) result;
	vma->vm_end = vma->vm_start + len;

#ifdef WARN_ON_SLACK
	if (len + WARN_ON_SLACK <= ksize(result))
		printk("Allocation of %lu bytes from process %d has %lu bytes of slack\n",
		       len, current->pid, ksize(result) - len);
#endif

	if (file) {
		mm_segment_t old_fs = get_fs();
		loff_t fpos;

		fpos = pgoff;
		fpos <<= PAGE_SHIFT;

		set_fs(KERNEL_DS);
		ret = file->f_op->read(file, (char *) result, len, &fpos);
		set_fs(old_fs);

		if (ret < 0)
			goto error2;
		if (ret < len)
			memset(result + ret, 0, len - ret);
	} else {
		memset(result, 0, len);
	}

	if (prot & PROT_EXEC)
		flush_icache_range((unsigned long) result, (unsigned long) result + len);

 done:
	if (!(vma->vm_flags & VM_SHARED)) {
		realalloc += ksize(result);
		askedalloc += len;
	}

	realalloc += ksize(vma);
	askedalloc += sizeof(*vma);

	current->mm->total_vm += len >> PAGE_SHIFT;

	add_nommu_vma(vma);
 shared:
	realalloc += ksize(vml);
	askedalloc += sizeof(*vml);

	vml->next = current->mm->vmlist;
	current->mm->vmlist = vml;

	up_write(&nommu_vma_sem);

#ifdef DEBUG
	printk("do_mmap:\n");
	show_process_blocks();
#endif

	return (unsigned long) result;

 error2:
	kfree(result);
 error:
	up_write(&nommu_vma_sem);
	kfree(vml);
	if (vma) {
		fput(vma->vm_file);
		kfree(vma);
	}
	return ret;

 sharing_violation:
	up_write(&nommu_vma_sem);
	printk("Attempt to share mismatched mappings\n");
	kfree(vml);
	return -EINVAL;

 error_getting_vma:
	up_write(&nommu_vma_sem);
	kfree(vml);
	printk("Allocation of vml for %lu byte allocation from process %d failed\n",
	       len, current->pid);
	show_free_areas();
	return -ENOMEM;

 error_getting_vml:
	printk("Allocation of vml for %lu byte allocation from process %d failed\n",
	       len, current->pid);
	show_free_areas();
	return -ENOMEM;
}

/*
 * handle mapping disposal for uClinux
 */
static void put_vma(struct vm_area_struct *vma)
{
	if (vma) {
		down_write(&nommu_vma_sem);

		if (atomic_dec_and_test(&vma->vm_usage)) {
			delete_nommu_vma(vma);

			if (vma->vm_ops && vma->vm_ops->close)
				vma->vm_ops->close(vma);

			/* IO memory and memory shared directly out of the pagecache from
			 * ramfs/tmpfs mustn't be released here */
			if (!(vma->vm_flags & (VM_IO | VM_SHARED)) && vma->vm_start) {
				realalloc -= ksize((void *) vma->vm_start);
				askedalloc -= vma->vm_end - vma->vm_start;
#ifdef MAGIC_ROM_PTR
				if (!is_in_rom(vma->vm_start))
					kfree((void *) vma->vm_start);
#else
				kfree((void *) vma->vm_start);
#endif
			}

			realalloc -= ksize(vma);
			askedalloc -= sizeof(*vma);

			if (vma->vm_file)
				fput(vma->vm_file);
			kfree(vma);
		}

		up_write(&nommu_vma_sem);
	}
}

int do_munmap(struct mm_struct *mm, unsigned long addr, size_t len)
{
	struct vm_list_struct *vml, **parent;
	unsigned long end = addr + len;

#if 0 /* def MAGIC_ROM_PTR - we want do want to free it with the new mmap */
	/* For efficiency's sake, if the pointer is obviously in ROM,
	   don't bother walking the lists to free it */
	if (is_in_rom(addr))
		return 0;
#endif

#ifdef DEBUG
	printk("do_munmap:\n");
#endif

	for (parent = &mm->vmlist; *parent; parent = &(*parent)->next)
		if ((*parent)->vma->vm_start == addr &&
		    (len == 0 || (*parent)->vma->vm_end == end))
			goto found;

	printk("munmap of non-mmaped memory by process %d (%s): %p\n",
	       current->pid, current->comm, (void *) addr);
	return -EINVAL;

 found:
	vml = *parent;

	put_vma(vml->vma);

	*parent = vml->next;
	realalloc -= ksize(vml);
	askedalloc -= sizeof(*vml);
	kfree(vml);
	mm->total_vm -= len >> PAGE_SHIFT;

#ifdef DEBUG
	show_process_blocks();
#endif	  

	return 0;
}

/* Release all mmaps. */
void exit_mmap(struct mm_struct * mm)
{
	struct vm_list_struct *tmp;

	if (!mm)
		return;

#ifdef DEBUG
	printk("Exit_mmap:\n");
#endif

	while ((tmp = mm->vmlist)) {
		mm->vmlist = tmp->next;
		put_vma(tmp->vma);

		realalloc -= ksize(tmp);
		askedalloc -= sizeof(*tmp);
		kfree(tmp);
	}

#ifdef DEBUG
	show_process_blocks();
#endif	  
}

asmlinkage long sys_munmap(unsigned long addr, size_t len)
{
	int ret;
	struct mm_struct *mm = current->mm;

	down_write(&mm->mmap_sem);
	ret = do_munmap(mm, addr, len);
	up_write(&mm->mmap_sem);
	return ret;
}

#endif /* NO_MM */