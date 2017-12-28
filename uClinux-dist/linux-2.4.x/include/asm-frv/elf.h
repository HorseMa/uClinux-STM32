/* elf.h: FR-V ELF definitions
 *
 * Copyright (C) 2003 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 * - Derived from include/asm-m68knommu/elf.h
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#ifndef __ASM_ELF_H
#define __ASM_ELF_H

/*
 * ELF register definitions..
 */

#include <linux/config.h>
#include <asm/ptrace.h>
#include <asm/user.h>

typedef unsigned long elf_greg_t;

#define ELF_NGREG (sizeof(struct pt_regs) / sizeof(elf_greg_t))
typedef elf_greg_t elf_gregset_t[ELF_NGREG];

typedef struct fpmedia_struct elf_fpregset_t;

/*
 * This is used to ensure we don't load something for the wrong architecture.
 */

struct elf32_hdr;
extern int elf_check_arch(const struct elf32_hdr *hdr);
#define elf_check_fdpic(x) ((x)->e_flags & EF_FRV_FDPIC && !((x)->e_flags & EF_FRV_NON_PIC_RELOCS))
#define elf_check_const_displacement(x) ((x)->e_flags & EF_FRV_PIC)

/*
 * These are used to set parameters in the core dumps.
 */
#define ELF_CLASS	ELFCLASS32
#define ELF_DATA	ELFDATA2MSB
#define ELF_ARCH	EM_FRV

#define ELF_PLAT_INIT(_r)			\
do {						\
	__kernel_frame0_ptr->gr16	= 0;	\
	__kernel_frame0_ptr->gr17	= 0;	\
	__kernel_frame0_ptr->gr18	= 0;	\
	__kernel_frame0_ptr->gr19	= 0;	\
	__kernel_frame0_ptr->gr20	= 0;	\
	__kernel_frame0_ptr->gr21	= 0;	\
	__kernel_frame0_ptr->gr22	= 0;	\
	__kernel_frame0_ptr->gr23	= 0;	\
	__kernel_frame0_ptr->gr24	= 0;	\
	__kernel_frame0_ptr->gr25	= 0;	\
	__kernel_frame0_ptr->gr26	= 0;	\
	__kernel_frame0_ptr->gr27	= 0;	\
	__kernel_frame0_ptr->gr29	= 0;	\
} while(0)

#define ELF_FDPIC_PLAT_INIT(_regs, _exec_map_addr, _interp_map_addr, _dynamic_addr)	\
do {											\
	__kernel_frame0_ptr->gr16	= _exec_map_addr;				\
	__kernel_frame0_ptr->gr17	= _interp_map_addr;				\
	__kernel_frame0_ptr->gr18	= _dynamic_addr;				\
	__kernel_frame0_ptr->gr19	= 0;						\
	__kernel_frame0_ptr->gr20	= 0;						\
	__kernel_frame0_ptr->gr21	= 0;						\
	__kernel_frame0_ptr->gr22	= 0;						\
	__kernel_frame0_ptr->gr23	= 0;						\
	__kernel_frame0_ptr->gr24	= 0;						\
	__kernel_frame0_ptr->gr25	= 0;						\
	__kernel_frame0_ptr->gr26	= 0;						\
	__kernel_frame0_ptr->gr27	= 0;						\
	__kernel_frame0_ptr->gr29	= 0;						\
} while(0)

#define USE_ELF_CORE_DUMP
#define ELF_EXEC_PAGESIZE	16384

/* This is the location that an ET_DYN program is loaded if exec'ed.  Typical
   use of this is to invoke "./ld.so someprog" to test out a new version of
   the loader.  We need to make sure that it is out of the way of the program
   that it will "exec", and that there is sufficient room for the brk.  */

#define ELF_ET_DYN_BASE         0x08000000UL

#define ELF_CORE_COPY_REGS(pr_reg, regs)				\
	memcpy(&pr_reg[0], &regs->sp, 31 * sizeof(uint32_t));

/* This yields a mask that user programs can use to figure out what
   instruction set this cpu supports.  */

#define ELF_HWCAP	(0)

/* This yields a string that ld.so will use to load implementation
   specific libraries for optimization.  This is more specific in
   intent than poking at uname or /proc/cpuinfo.  */

#define ELF_PLATFORM  (NULL)

#ifdef __KERNEL__
#define SET_PERSONALITY(ex, ibcs2) set_personality((ibcs2)?PER_SVR4:PER_LINUX)
#endif

#endif
