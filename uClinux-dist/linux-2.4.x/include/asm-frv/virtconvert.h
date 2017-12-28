#ifndef __ASM_VIRT_CONVERT__
#define __ASM_VIRT_CONVERT__

/*
 * Macros used for converting between virtual and physical mappings.
 */

#ifdef __KERNEL__

#include <linux/config.h>
#include <asm/setup.h>

#ifndef CONFIG_UCLINUX

#define phys_to_virt(vaddr)	((void *) ((unsigned long)(vaddr) + PAGE_OFFSET))
#define virt_to_phys(vaddr)	((unsigned long) (vaddr) - PAGE_OFFSET)

#else

#define phys_to_virt(vaddr)	((void *) (vaddr))
#define virt_to_phys(vaddr)	((unsigned long) (vaddr))

#endif

#define virt_to_bus virt_to_phys
#define bus_to_virt phys_to_virt

#define __page_address(page)	(PAGE_OFFSET + (((page) - mem_map) << PAGE_SHIFT))
#define page_to_phys(page)	virt_to_phys((void *)__page_address(page))

#endif
#endif
