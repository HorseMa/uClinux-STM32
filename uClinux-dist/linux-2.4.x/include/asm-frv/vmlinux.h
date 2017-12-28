/* vmlinux.h: linkage layout variables
 *
 * Copyright (C) 2003 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef _ASM_VMLINUX_H
#define _ASM_VMLINUX_H

#ifndef __ASSEMBLY__

#include <linux/types.h>

#ifdef __KERNEL__

/*
 * we don't want to put variables in the GP-REL section if they're not used very much - that would
 * be waste since GP-REL addressing is limited to GP16+/-2048
 */
#define __nongpreldata	__attribute__((section(".data")))
#define __nongprelbss	__attribute__((section(".bss")))

/*
 * linker symbols
 */
extern const char _stext[], _etext[], _sdata[], _edata[], _sbss[], _ebss[], _end[];
extern const char __init_begin[], __init_end[];
extern const void __kernel_image_start, __kernel_image_end, __page_offset;

extern unsigned long __nongprelbss memory_start;
extern unsigned long __nongprelbss memory_end;
extern unsigned long __nongprelbss rom_length;

/* determine if we're running from ROM */
static inline int is_in_rom(unsigned long addr)
{
	return 0; /* default case: not in ROM */
}

#endif
#endif
#endif /* _ASM_VMLINUX_H */
