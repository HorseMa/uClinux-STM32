/*
 * linux/include/asm-armnommu/arch-isl3893/mpu.h
 *
 * ISL3893 (ARM946) Memory Protection Unit support.
 */
/*  $Header$
 *
 *  Copyright (C) 2002 Intersil Americas Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef __ASM_ARCH_MPU_H
#define __ASM_ARCH_MPU_H
#include <asm/proc-fns.h>

#define MAX_ACTIVE_MPU_AREAS		4	/* We use area 4-7, area 0-3 are for the MVC */
#define MAX_STATIC_USER_MPU_AREAS	2	/* Static areas for Text and Data/stack/BSS */
/* Max number of areas we allow. Increasing this value will cost performance, but gives greater
 * flexibility. This value is chosen such that the mpu_context_t struct is a multiple of
 * a cacheline (4 words)
 */
#define MAX_USER_MPU_AREAS		30

struct mpu_context_t
{
   unsigned long mpu_reg[MAX_USER_MPU_AREAS];
   unsigned long switch_idx;
   unsigned long reprogram_count;
};

static inline void program_mpu_areas( struct mpu_context_t *mpu_context )
{
	__asm__ __volatile__ ("mcr p15, 0, %0, c6, c4, 0 " : : "r" (mpu_context->mpu_reg[0]));
	__asm__ __volatile__ ("mcr p15, 0, %0, c6, c5, 0 " : : "r" (mpu_context->mpu_reg[1]));
	__asm__ __volatile__ ("mcr p15, 0, %0, c6, c6, 0 " : : "r" (mpu_context->mpu_reg[2]));
	__asm__ __volatile__ ("mcr p15, 0, %0, c6, c7, 0 " : : "r" (mpu_context->mpu_reg[3]));

	/* We have to flush the cache after an MPU switch */
	cpu_cache_clean_invalidate_all();
}

#endif /* __ASM_ARCH_MPU_H */
