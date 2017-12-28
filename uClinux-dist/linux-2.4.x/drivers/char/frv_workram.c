/* frv_workram.c: Fujitsu FR-V CPU WorkRAM driver
 *
 * Copyright (C) 2004 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * See Documentation/fujitsu/frv/features.txt for more information
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/devfs_fs_kernel.h>
#include <asm/pgalloc.h>
#include <asm/uaccess.h>

#define WORKRAM_MAJOR 240 /* local development reserved chardev major */

MODULE_AUTHOR("Red Hat, Inc.");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Fujitsu FR-V WorkRAM device driver");
MODULE_SUPPORTED_DEVICE("frv/workram");

/*****************************************************************************/
/*
 * read from the workram memory
 */
static ssize_t workram_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	unsigned long p, v;
	unsigned int pagenum = MINOR(file->f_dentry->d_inode->i_rdev);

	p = *ppos;
	if (p >= PAGE_SIZE)
		return 0;

	if (count > PAGE_SIZE - p)
		count = PAGE_SIZE - p;

	v = 0xfe800000 + (pagenum << PAGE_SHIFT) + p;

	if (copy_to_user(buf, (void *) v, count))
		return -EFAULT;

	*ppos += count;
	return count;
} /* end workram_read() */

/*****************************************************************************/
/*
 * write to the workram
 */
static ssize_t workram_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	unsigned long p, v;
	unsigned int pagenum = MINOR(file->f_dentry->d_inode->i_rdev);

	p = *ppos;
	if (p >= PAGE_SIZE)
		return -ENOSPC;

	if (count > PAGE_SIZE - p)
		count = PAGE_SIZE - p;

	v = 0xfe800000 + (pagenum << PAGE_SHIFT) + p;

	if (copy_from_user((void *) v, buf, count))
		return -EFAULT;

	*ppos += count;
	return count;
} /* end workram_write() */

/*****************************************************************************/
/*
 * permit seeking within the WorkRAM
 */
static loff_t workram_llseek(struct file *file, loff_t offset, int origin)
{
	switch (origin) {
	case 2:
		offset += PAGE_SIZE;
		break;
	case 1:
		offset += file->f_pos;
		break;
	}

	if (offset < 0)
		return -EINVAL;

	return file->f_pos = offset;

} /* end workram_llseek() */

/*****************************************************************************/
/*
 * make a mapping by which userspace can access the page tables
 * - on MMU linux, modify the appropriate PTEs to hold the mapping
 * - on uClinux, we change the I/O DAMPR to make it userspace accessible
 */
static int workram_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long vaddr, paddr;
	unsigned int pagenum = MINOR(file->f_dentry->d_inode->i_rdev);

	/* execution from WorkRAM is not permitted, and nor are private mappings */
	if (vma->vm_flags & VM_EXEC)
		return -EINVAL;
	if (!(vma->vm_flags & VM_SHARED))
		return -EINVAL;

	/* can only mmap a page from the beginning, and must map at the correct address */
	paddr = 0xfe800000 + (pagenum << PAGE_SHIFT);
#ifndef NO_MM
	vaddr = 0x7e800000 + (pagenum << PAGE_SHIFT);
#else
	vaddr = paddr;
#endif

	if (vma->vm_pgoff != 0 ||
	    vma->vm_start != vaddr ||
	    vma->vm_end - vma->vm_start != PAGE_SIZE)
		return -EINVAL;

	/* the WorkRAM is not swappable and it's not represented by a struct page */
	vma->vm_flags |= VM_RESERVED | VM_IO;

#ifndef NO_MM
	/* the WorkRAM shouldn't be cached */
	vma->vm_page_prot = __pgprot(pgprot_val(vma->vm_page_prot) | xAMPRx_C);

	/* modify the page tables on MMU linux (uClinux doesn't need to do anything) */
	if (remap_page_range(vaddr, paddr, PAGE_SIZE, vma->vm_page_prot))
		return -EAGAIN;

#else
	/* clear the supervisor-only bit in the I/O DAMPR */
	__set_DAMPR(11, __get_DAMPR(11) & ~xAMPRx_S);

#endif

	return 0;
} /* end workram_mmap() */

/*****************************************************************************/
/*
 * specify exactly the address at which the mapping must be made
 */
static unsigned long workram_get_unmapped_area(struct file *file,
					       unsigned long addr, unsigned long len,
					       unsigned long pgoff, unsigned long flags)
{
	unsigned int pagenum = MINOR(file->f_dentry->d_inode->i_rdev);

#ifndef NO_MM
	return 0x7e800000 + (pagenum << PAGE_SHIFT);
#else
	return 0xfe800000 + (pagenum << PAGE_SHIFT);
#endif
} /* end workram_get_unmapped_area() */

/*****************************************************************************/
/*
 * check that the workram has sufficient pages to honour the WorkRAM page specified by the
 * minor device number
 */
static int workram_open(struct inode *inode, struct file *file)
{
	unsigned int workram_npages = 0;
	unsigned long psr = __get_PSR();

	switch (PSR_IMPLE(psr)) {
	case PSR_IMPLE_FR451:
		switch (PSR_VERSION(psr)) {
		case PSR_VERSION_FR451_MB93451:
			workram_npages = 1;
			break;
		default:
			break;
		}
	default:
		break;
	}

	if (MINOR(inode->i_rdev) >= workram_npages)
		return -ENXIO;

	return 0;
} /* end workram_open() */

static struct file_operations workram_fops = {
	.llseek			= workram_llseek,
	.read			= workram_read,
	.write			= workram_write,
	.mmap			= workram_mmap,
	.get_unmapped_area	= workram_get_unmapped_area,
	.open			= workram_open,
};

static int __init workram_init(void)
{
	if (devfs_register_chrdev(WORKRAM_MAJOR, "workram", &workram_fops)) {
		printk(KERN_WARNING "unable to get major %d for workram devs\n", WORKRAM_MAJOR);
		return -EAGAIN;
	}

	printk(KERN_INFO "FR-V WorkRAM driver initialised\n");
	return 0;
}

static void __exit workram_exit(void)
{
	if (devfs_unregister_chrdev(WORKRAM_MAJOR, "workram"))
		printk(KERN_CRIT "failed to remove WorkRAM device on module unload\n");
}


module_init(workram_init);
module_exit(workram_exit);
