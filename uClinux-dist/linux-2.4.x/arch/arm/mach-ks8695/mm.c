/*
 *  linux/arch/arm/mach-ks8695/mm.c
 *
 *  Extra MM routines for the ARM Integrator board
 *
 *  Copyright (C) 1999,2000 Arm Limited
 *  Copyright (C) 2000 Deep Blue Solutions Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/interrupt.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/page.h>
 
#include <asm/mach/map.h>

/*
 * Logical      Physical
 * f0000000	03FF0000	IO registers
 */
#ifndef SZ_768K
#define	SZ_768K		(SZ_512K+SZ_256K)
#endif
 
static struct map_desc ks8695_io_desc[] __initdata = {
#ifdef CONFIG_PCI_KS8695P
  { IO_ADDRESS(KS8695_IO_BASE),        KS8695_IO_BASE,         SZ_64K,  DOMAIN_IO, 0, 1},	 
#else  
  { IO_ADDRESS(KS8695_PCMCIA_IO_BASE), KS8695_PCMCIA_IO_BASE,  SZ_768K, DOMAIN_IO, 0, 1},
  { IO_ADDRESS(KS8695_IO_BASE),        KS8695_IO_BASE,         SZ_64K,  DOMAIN_IO, 0, 1},
#endif	
   LAST_DESC
};

void __init ks8695_map_io(void)
{
	iotable_init(ks8695_io_desc);
}

/*
 * KS8695's internal ethernet driver doesn't use PCI bus at all. As a resumt
 * call these set of functions instead
 */
void *consistent_alloc_ex(int gfp, size_t size, dma_addr_t *dma_handle)
{
	struct page *page, *end, *free;
	unsigned long order;
	void *ret, *virt;

	if (in_interrupt())
		BUG();

	size = PAGE_ALIGN(size);
	order = get_order(size);

	page = alloc_pages(gfp, order);
	if (!page)
		goto no_page;

	/*
	 * We could do with a page_to_phys here
	 */
	virt = page_address(page);
	*dma_handle = virt_to_phys(virt);
	ret = __ioremap(virt_to_phys(virt), size, 0);
	if (!ret)
		goto no_remap;

	/*
	 * free wasted pages.  We skip the first page since we know
	 * that it will have count = 1 and won't require freeing.
	 * We also mark the pages in use as reserved so that
	 * remap_page_range works.
	 */
	page = virt_to_page(virt);
	free = page + (size >> PAGE_SHIFT);
	end  = page + (1 << order);

	for (; page < end; page++) {
		set_page_count(page, 1);
		if (page >= free)
			__free_page(page);
		else
			SetPageReserved(page);
	}
	return ret;

no_remap:
	__free_pages(page, order);
no_page:
	return NULL;
}

/*
 * free a page as defined by the above mapping.  We expressly forbid
 * calling this from interrupt context.
 */
void consistent_free_ex(void *vaddr, size_t size, dma_addr_t handle)
{
	struct page *page, *end;
	void *virt;

	if (in_interrupt())
		BUG();

	virt = phys_to_virt(handle);

	/*
	 * More messing around with the MM internals.  This is
	 * sick, but then so is remap_page_range().
	 */
	size = PAGE_ALIGN(size);
	page = virt_to_page(virt);
	end = page + (size >> PAGE_SHIFT);

	for (; page < end; page++)
		ClearPageReserved(page);

	__iounmap(vaddr);
}
