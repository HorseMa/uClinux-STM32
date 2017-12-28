/*
 *  linux/arch/arm/mm/ioremap.c
 *
 * Re-map IO memory to kernel address space so that we can access it.
 *
 * (C) Copyright 1995 1996 Linus Torvalds
 *
 * Hacked for ARM by Phil Blundell <philb@gnu.org>
 * Hacked to allow all architectures to build, and various cleanups
 * by Russell King
 * Hacked for uClinux by Gordon Mcnutt <gmcnutt@ridgerun.com>
 */
#include <linux/types.h>
#include <linux/mm.h>

#include <asm/page.h>
#include <asm/pgalloc.h>

/*
 * No MMU? No problem.
 * --gmcnutt
 */
void * __ioremap(unsigned long phys_addr, size_t size, unsigned long flags)
{
        flush_cache_all();

#ifdef CONFIG_ARCH_WINBOND
        // return non-cacheable address
        return (void *)((unsigned long)phys_addr | 0x80000000);
#else

        return (void *)phys_addr;
#endif
}

void __iounmap(void *addr)
{}
