/*
 *  arch/microblaze/mm/init.c
 *
 *  Copyright (C) 2004 John Williams
 *  Copyright (C) 2004 Atmark Techno, Inc.
 *
 *  John Williams <jwilliams@itee.uq.edu.au>
 *  Yasushi SHOJI <yashi@atmark-techno.com>
 *
 *  Based on
 *        arch/microblaze/setup.c
 *   and
 *        arch/arm/mm/init.c
 *        arch/sparc/mm/init.c
 *        arch/ia64/mm/init.c
 *        arch/m68knommu/kernel/setup.c
 *        arch/h8300/kernel/setup.c
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License.  See the file COPYING in the main directory of this
 * archive for more details.
 *
 */

#include <linux/mm.h>
#include <linux/bootmem.h>
#include <linux/config.h>

#include <asm/pgalloc.h>
#include <asm/io.h>
#include <asm/machdep.h>
#include <asm/setup.h>
#include <asm/map.h>

/* Locally used struct to describe the memory nodes */
struct node_info {
	unsigned int start;
	unsigned int end;
	int	bootmap_pages;
};

/* these function prototypes are taken form arch/microblaze/kernel/mach.h */
extern void mach_reserve_bootmem (void) __attribute__ ((__weak__));
extern void mach_get_physical_ram (unsigned long *ram_start, unsigned long *ram_len);

extern char _kram_start __attribute__ ((__weak__));
extern char _kram_end __attribute__ ((__weak__));

/* erase following when we port to 2.6, include <asm/sections.h> instead */
extern char _text[], _stext[], _etext[];
extern char _data[], _sdata[], _edata[];
extern char _end[];

extern char __bss_start[], __bss_stop[];
extern char __init_begin[], __init_end[];
extern char _sinittext[], _einittext[];

extern unsigned long _ramstart;

/* Physical System RAM.  */
static unsigned long ram_start = 0, ram_len = 0;

#define ADDR_TO_PAGE_UP(x)   ((((unsigned long)x) + PAGE_SIZE-1) >> PAGE_SHIFT)
#define ADDR_TO_PAGE(x)	     (((unsigned long)x) >> PAGE_SHIFT)
#define PAGE_TO_ADDR(x)	     (((unsigned long)x) << PAGE_SHIFT)

#define O_PFN_DOWN(x)	((x) >> PAGE_SHIFT)
#define V_PFN_DOWN(x)	O_PFN_DOWN(__pa(x))

#define O_PFN_UP(x)	(PAGE_ALIGN(x) >> PAGE_SHIFT)
#define V_PFN_UP(x)	O_PFN_UP(__pa(x))

#define PFN_SIZE(x)	((x) >> PAGE_SHIFT)
#define PFN_RANGE(s,e)	PFN_SIZE(PAGE_ALIGN((unsigned long)(e)) - \
				(((unsigned long)(s)) & PAGE_MASK))


#ifndef CONFIG_DISCONTIGMEM
#define NR_NODES	1
#else
#define NR_NODES	4
#endif

/* Somewhere to store "local" copy of meminfo structure. */
static struct meminfo meminfo __initdata = { 0, };

struct page *empty_zero_page;

/* Memory not used by the kernel.  */

void show_mem(void)
{
	int free = 0, total = 0, reserved = 0;
	int shared = 0, cached = 0, node;

	printk("Mem-info:\n");
	show_free_areas();
	printk("Free swap:       %6dkB\n",nr_swap_pages<<(PAGE_SHIFT-10));

	for (node = 0; node < numnodes; node++) {
		struct page *page, *end;

		page = NODE_MEM_MAP(node);
		end  = page + NODE_DATA(node)->node_size;

		do {
/* This is currently broken
 * PG_skip is used on sparc/sparc64 architectures to "skip" certain
 * parts of the address space.
 *
 * #define PG_skip	10
 * #define PageSkip(page) (machine_is_riscpc() && test_bit(PG_skip, &(page)->flags))
 *			if (PageSkip(page)) {
 *				page = page->next_hash;
 *				if (page == NULL)
 *					break;
 *			}
 */
			total++;
			if (PageReserved(page))
				reserved++;
			else if (PageSwapCache(page))
				cached++;
			else if (!page_count(page))
				free++;
			else
				shared += atomic_read(&page->count) - 1;
			page++;
		} while (page < end);
	}

	printk("%d pages of RAM\n", total);
	printk("%d free pages\n", free);
	printk("%d reserved pages\n", reserved);
	printk("%d pages shared\n", shared);
	printk("%d pages swap cached\n", cached);
/* #ifndef CONFIG_NO_PGT_CACHE */
#if 0
	printk("%ld page tables cached\n", pgtable_cache_size);
#endif
	show_buffers();
}

/* for 2.4, declare totalram_pages here.  for 2.6, totalram_pages is
 * declared in mm/page_alloc.c. remove from here and include
 * linux/swap.h, instead.  - yashi Apr 10, 2004 */
static unsigned long totalram_pages;

static inline void free_memmap(int node, unsigned long start, unsigned long end)
{
	unsigned long pg, pgend;

	start = __phys_to_virt(start);
	end   = __phys_to_virt(end);

	pg    = PAGE_ALIGN((unsigned long)(virt_to_page(start)));
	pgend = ((unsigned long)(virt_to_page(end))) & PAGE_MASK;

	start = __virt_to_phys(pg);
	end   = __virt_to_phys(pgend);

	free_bootmem_node(NODE_DATA(node), start, end - start);
}

static inline void free_unused_memmap_node(int node, struct meminfo *mi)
{
	unsigned long bank_start, prev_bank_end = 0;
	unsigned int i;

	/*
	 * [FIXME] This relies on each bank being in address order.  This
	 * may not be the case, especially if the user has provided the
	 * information on the command line.
	 */
	for (i = 0; i < mi->nr_banks; i++) {
		if (mi->bank[i].size == 0 || mi->bank[i].node != node)
			continue;

		bank_start = mi->bank[i].start & PAGE_MASK;

		/*
		 * If we had a previous bank, and there is a space
		 * between the current bank and the previous, free it.
		 */
		if (prev_bank_end && prev_bank_end != bank_start)
			free_memmap(node, prev_bank_end, bank_start);

		prev_bank_end = PAGE_ALIGN(mi->bank[i].start +
					   mi->bank[i].size);
	}
}

/*
 * The mem_map array can get very big.  Free
 * the unused area of the memory map.
 */
void __init create_memmap_holes(struct meminfo *mi)
{
	int node;

	for (node = 0; node < numnodes; node++)
		free_unused_memmap_node(node, mi);
}

/*
 * mem_init() marks the free areas in the mem_map and tells us how much
 * memory is free.  This is done after various parts of the system have
 * claimed their memory after the kernel image.
 */
void __init mem_init(void)
{
	unsigned int codepages, datapages, initpages;
	int i, node;

	codepages = (unsigned long)&_etext - (unsigned long)&_stext;
#ifdef CONFIG_MODEL_RAM
	datapages = _ramstart - ((unsigned int)&_etext);
#endif
#ifdef CONFIG_MODEL_ROM
	datapages = _ramstart - ((unsigned int)&_kram_start);
#endif
	initpages = (unsigned long)&__init_end - (unsigned long)&__init_begin;

	high_memory = (void *)__va(meminfo.end);
	max_mapnr   = virt_to_page(high_memory) - mem_map;

	/*
	 * We may have non-contiguous memory.
	 */
	if (meminfo.nr_banks != 1)
		create_memmap_holes(&meminfo);

	/* this will put all unused low memory onto the freelists */
	for (node = 0; node < numnodes; node++)
		totalram_pages += free_all_bootmem_node(NODE_DATA(node));

	//	SetPageReserved(virt_to_page(PAGE_OFFSET));
	/*
	 * Since our memory may not be contiguous, calculate the
	 * real number of pages we have in this system
	 */
	printk(KERN_INFO "Memory:");

	num_physpages = 0;
	for (i = 0; i < meminfo.nr_banks; i++) {
		num_physpages += meminfo.bank[i].size >> PAGE_SHIFT;
		printk(" %ldMB", meminfo.bank[i].size >> 20);
	}

	printk(" = %luMB total\n", num_physpages >> (20 - PAGE_SHIFT));
	printk(KERN_NOTICE "Memory: %luKB available (%dK code, "
		"%dK data, %dK init)\n",
		(unsigned long) nr_free_pages() << (PAGE_SHIFT-10),
		codepages >> 10, datapages >> 10, initpages >> 10);

	#if CONFIG_XILINX_UNCACHED_SHADOW
	printk(KERN_INFO "Memory: Uncached shadow enabled (0x%08X-0x%08X)\n",
		CONFIG_XILINX_ERAM_START | UNCACHED_SHADOW_MASK,
		(CONFIG_XILINX_ERAM_START | UNCACHED_SHADOW_MASK) + 
			CONFIG_XILINX_ERAM_SIZE - 1);
	#endif

	if (PAGE_SIZE >= 16384 && num_physpages <= 128) {
		extern int sysctl_overcommit_memory;
		/*
		 * On a machine this small we won't get
		 * anywhere without overcommit, so turn
		 * it on by default.
		 */
		sysctl_overcommit_memory = 1;
	}
}

/*
 * Free memory area area bounded by the range (addr, end)
 */
static inline void free_area(unsigned long addr, unsigned long end, char *s)
{
	unsigned int size = (end - addr) >> 10;

	for (; addr < end; addr += PAGE_SIZE) {
		struct page *page = virt_to_page(addr);
		ClearPageReserved(page);
		set_page_count(page, 1);
		free_page(addr);
		totalram_pages++;
	}

	if (size)
		printk("Freeing %s memory: %dK\n", s, size);
}

void free_initmem(void)
{
	free_area((unsigned long)(&__init_begin),
		  (unsigned long)(&__init_end),
		  "init");
}

/*
 * Scan the memory info structure and pull out:
 *  - the end of memory
 *  - the number of nodes
 *  - the pfn range of each node
 *  - the number of bootmem bitmap pages
 */
static unsigned int __init
find_memend_and_nodes(struct meminfo *mi, struct node_info *np)
{
	unsigned int i, bootmem_pages = 0, memend_pfn = 0;

	for (i = 0; i < NR_NODES; i++) {
		np[i].start = -1U;
		np[i].end = 0;
		np[i].bootmap_pages = 0;
	}

	for (i = 0; i < mi->nr_banks; i++) {
		unsigned long start, end;
		int node;

		if (mi->bank[i].size == 0) {
			/*
			 * Mark this bank with an invalid node number
			 */
			mi->bank[i].node = -1;
			continue;
		}

		node = mi->bank[i].node;

		if (node >= numnodes) {
			numnodes = node + 1;

			/*
			 * Make sure we haven't exceeded the maximum number
			 * of nodes that we have in this configuration.  If
			 * we have, we're in trouble.  (maybe we ought to
			 * limit, instead of bugging?)
			 */
			if (numnodes > NR_NODES)
				BUG();
		}

		/*
		 * Get the start and end pfns for this bank
		 */
		start = O_PFN_UP(mi->bank[i].start);
		end   = O_PFN_DOWN(mi->bank[i].start + mi->bank[i].size);

		if (np[node].start > start)
			np[node].start = start;

		if (np[node].end < end)
			np[node].end = end;

		if (memend_pfn < end)
			memend_pfn = end;
	}

	/*
	 * Calculate the number of pages we require to
	 * store the bootmem bitmaps.
	 */
	for (i = 0; i < numnodes; i++) {
		if (np[i].end == 0)
			continue;

		np[i].bootmap_pages = bootmem_bootmap_pages(np[i].end -
							    np[i].start);
		bootmem_pages += np[i].bootmap_pages;
	}

	/*
	 * This doesn't seem to be used by the Linux memory
	 * manager any more.  If we can get rid of it, we
	 * also get rid of some of the stuff above as well.
	 */
	/* HACK! */
	#define PHYS_OFFSET (PAGE_OFFSET)
	max_low_pfn = memend_pfn - O_PFN_DOWN(PHYS_OFFSET);
//	max_pfn = memend_pfn - O_PFN_DOWN(PHYS_OFFSET);
	mi->end = memend_pfn << PAGE_SHIFT;

	return bootmem_pages;
}

/*
 * Reserve the various regions of node 0
 */
static __init void reserve_node_zero(unsigned int bootmap_pfn, unsigned int bootmap_pages)
{
	/* Handle to Node 0*/
	pg_data_t *pgdat = NODE_DATA(0);

	unsigned long kram_start = (unsigned long )&_kram_start;
	unsigned long kram_end  = (unsigned long )&_kram_end;

	/* Machine specific bootmem reserve function */
	void (*volatile mrb) (void) = mach_reserve_bootmem;

	/*
	 * Register the kernel text and data with bootmem.
	 * Note that this can only be in node 0.
	 */
	if(kram_end > kram_start)
		reserve_bootmem_node(pgdat, __pa(kram_start), 
						(kram_end-kram_start));

        /* Reserve space for the bss segment.  Also save space for
         * rootfs as well. because bss and romfs is contiguous, we
         * reserve from start of bss to end of romfs which is
         * _ramstart */
        reserve_bootmem_node(pgdat, __pa(__bss_start), 
			_ramstart - (unsigned long)__bss_start);

	/*
	 * And don't forget to reserve the allocator bitmap,
	 * which will be freed later.
	 */
	reserve_bootmem_node(pgdat, bootmap_pfn << PAGE_SHIFT,
			     bootmap_pages << PAGE_SHIFT);

	/* Let the platform-dependent code reserve some too.  */
	if (mrb)
		(*mrb) ();
}

/*
 * FIXME: We really want to avoid allocating the bootmap bitmap
 * over the top of the initrd.  Hopefully, this is located towards
 * the start of a bank, so if we allocate the bootmap bitmap at
 * the end, we won't clash.
 */
static unsigned int __init
find_bootmap_pfn(int node, struct meminfo *mi, unsigned int bootmap_pages)
{
	unsigned int start_pfn, bank, bootmap_pfn;

	start_pfn   = V_PFN_UP(_ramstart);
	bootmap_pfn = 0;

	for (bank = 0; bank < mi->nr_banks; bank ++) {
		unsigned int start, end;

		if (mi->bank[bank].node != node)
			continue;

		start = O_PFN_UP(mi->bank[bank].start);
		end   = O_PFN_DOWN(mi->bank[bank].size +
				   mi->bank[bank].start);

		if (end < start_pfn)
			continue;

		if (start < start_pfn)
			start = start_pfn;

		if (end <= start)
			continue;

		if (end - start >= bootmap_pages) {
			bootmap_pfn = start;
			break;
		}
	}

	if (bootmap_pfn == 0)
		BUG();

	return bootmap_pfn;
}

/*
 * Register all available RAM in this node with the bootmem allocator.
 */
static inline void free_bootmem_node_bank(int node, struct meminfo *mi)
{
	pg_data_t *pgdat = NODE_DATA(node);
	int bank;

	for (bank = 0; bank < mi->nr_banks; bank++)
		if (mi->bank[bank].node == node)
			free_bootmem_node(pgdat, mi->bank[bank].start,
				mi->bank[bank].size);
}


/* Initialize the `bootmem allocator'.  RAM_START and RAM_LEN identify
   what RAM may be used.  */
void __init bootmem_init (struct meminfo *mi)
{
	struct node_info node_info[NR_NODES], *np = node_info;
	unsigned int bootmap_pages, bootmap_pfn, map_pg;
	int node, initrd_node;

	bootmap_pages	= find_memend_and_nodes(mi,np);
	bootmap_pfn 	= find_bootmap_pfn(0, mi, bootmap_pages);
	initrd_node	= 0; /*check_initrd(mi); */

	map_pg = bootmap_pfn;

	np += numnodes - 1;
	for (node = numnodes - 1; node >= 0; node--, np--)
	{
		if(np->end == 0)
		{
			if(node == 0)
				BUG();
			continue;
		}

		init_bootmem_node(NODE_DATA(node), map_pg, np->start, np->end);
		free_bootmem_node_bank(node, mi);
		map_pg += np->bootmap_pages;

		if(node == 0)
			reserve_node_zero(bootmap_pfn, bootmap_pages);
	}

	if(map_pg != bootmap_pfn + bootmap_pages)
		BUG();

}

/* Setup memory zone. We don't have no page table to warry about.
 * Initialize memory zone and call free_area_init_node().
 *
 * Note: One we get discon mem support, we need to chage the function
 */
void __init paging_init (struct meminfo *mi)
{
	unsigned i;
	unsigned long zones_size[MAX_NR_ZONES];
	void *zero_page;
	int node;

	/* Copy the meminfo struct locally */
	memcpy(&meminfo, mi, sizeof(meminfo));
	
	zero_page = alloc_bootmem_low_pages(PAGE_SIZE);
	
	for (node = 0; node < numnodes; node++)
	{
		unsigned long zone_size[MAX_NR_ZONES];
		unsigned long zhole_size[MAX_NR_ZONES];
		struct bootmem_data *bdata;
		pg_data_t *pgdat;
		int i;

		for(i=0; i < MAX_NR_ZONES; i++)
		{
			zone_size[i] = 0;
			zhole_size[i] = 0;
		}

		pgdat = NODE_DATA(node);
		bdata = pgdat->bdata;

		zone_size[0] = bdata->node_low_pfn - 
			(bdata->node_boot_start >> PAGE_SHIFT);
	
		zhole_size[0] = zone_size[0];
		for (i=0; i < mi->nr_banks; i++)
		{
			if (mi->bank[i].node != node)
				continue;

			zhole_size[0] -= mi->bank[i].size >> PAGE_SHIFT;
		}

		free_area_init_node(node, pgdat, 0, zone_size,
			bdata->node_boot_start, zhole_size);
	}

	memset(zero_page, 0, PAGE_SIZE);
	empty_zero_page = virt_to_page(zero_page);
	flush_dcache_page(empty_zero_page);
}

/* For 2.6, remove this function. mm/page_alloc.c has generic one. */
void si_meminfo (struct sysinfo *info)
{
	info->totalram = totalram_pages;
	info->sharedram = 0;
	info->freeram = nr_free_pages ();
	info->bufferram = atomic_read (&buffermem_pages);
	info->totalhigh = 0;
	info->freehigh = 0;
	info->mem_unit = PAGE_SIZE;
}
