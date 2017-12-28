/*
 * include/asm-microblaze/string.h -- Architecture specific string routines
 *
 *  Copyright (C) 2005,2003  John Williams <jwilliams@itee.uq.edu.au>
 *  Copyright (C) 2001,2002  NEC Corporation
 *  Copyright (C) 2001,2002  Miles Bader <miles@gnu.org>
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License.  See the file COPYING in the main directory of this
 * archive for more details.
 *
 * Written by Miles Bader <miles@gnu.org>
 * Microblaze port by John Williams
 */

#include <linux/types.h>

#ifndef __MICROBLAZE_STRING_H__
#define __MICROBLAZE_STRING_H__

/* Only use optimised memcpy/memmove if we have a HW barrel shift.
 * Otherwise, it's more efficient to use the naive default implementations.
 * These funcs are implemented in arch/microblaze/lib
 */
#if CONFIG_XILINX_MICROBLAZE0_USE_BARREL
#define __HAVE_ARCH_MEMCPY
#define __HAVE_ARCH_MEMMOVE

extern void *memcpy (void *, const void *, __kernel_size_t);
extern void *memmove (void *, const void *, __kernel_size_t);
#endif

/* Always use our optimised memset, it will rarely be slower than 
   a naive byte set loop */
#if 1
#define __HAVE_ARCH_MEMSET
extern void *memset (void *, int, __kernel_size_t);
#endif

/* 
BCOPY still done with naive kernel implementation.  bcopy is just memcpy
with reversed args, however it's not used in the kernel anyway.  Let's 
just ignore it for now.
*/

#if 0
#define __HAVE_ARCH_BCOPY
extern void bcopy (const char *, char *, int);
#endif

#endif /* __MICROBLAZE_STRING_H__ */
