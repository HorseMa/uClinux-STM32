/*
 * include/asm-microblaze/io.h -- Misc I/O operations
 *
 *  Copyright (C) 2003     John Williams <jwilliams@itee.uq.edu.au>
 *  Copyright (C) 2001,02  NEC Corporation
 *  Copyright (C) 2001,02  Miles Bader <miles@gnu.org>
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License.  See the file COPYING in the main directory of this
 * archive for more details.
 *
 * Written by Miles Bader <miles@gnu.org>
 * Microblaze port by John Williams
 */

#ifndef __MICROBLAZE_IO_H__
#define __MICROBLAZE_IO_H__

#include <linux/config.h>
#include <linux/types.h>
#include <asm/virtconvert.h>
#include <asm/page.h>
#include <linux/mm.h>		/* Get struct page {...} */

#define IO_SPACE_LIMIT 0xFFFFFFFF

#if CONFIG_XILINX_UNCACHED_SHADOW
#define UNCACHED_SHADOW_MASK (1 << CONFIG_XILINX_UNCACHED_BIT)
#else
#define UNCACHED_SHADOW_MASK (0x0)
#endif


/*
 * Defines for accessing PCI memory. PCI is inherently little-endian,
 * so on Microblaze we byte swap.  For a discussion on this, see
 * http://lkml.org/lkml/2004/11/18/390
 */
#define readb(addr) \
    ({ unsigned char __v = (*(volatile unsigned char *) (addr)); __v; })
#define readw(addr) \
    ({ unsigned short __v = le16_to_cpu(*(volatile unsigned short *) (addr)); __v; })
#define readl(addr) \
    ({ unsigned long __v = le32_to_cpu(*(volatile unsigned long *) (addr)); __v; })
#define writeb(b, addr) \
    (void)((*(volatile unsigned char *) (addr)) = (b))
#define writew(b, addr) \
    (void)((*(volatile unsigned short *) (addr)) = cpu_to_le16(b))
#define writel(b, addr) \
    (void)((*(volatile unsigned int *) (addr)) = cpu_to_le32(b))

/*
 * Defines for raw access to PCI memory - no byte swap.
 */
#define __raw_readb(addr) \
    ({ unsigned char __v = (*(volatile unsigned char *) (addr)); __v; })
#define __raw_readw(addr) \
    ({ unsigned short __v = (*(volatile unsigned short *) (addr)); __v; })
#define __raw_readl(addr) \
    ({ unsigned long __v = (*(volatile unsigned long *) (addr)); __v; })
#define __raw_writeb(b, addr) \
    (void)((*(volatile unsigned char *) (addr)) = (b))
#define __raw_writew(b, addr) \
    (void)((*(volatile unsigned short *) (addr)) = (b))
#define __raw_writel(b, addr) \
    (void)((*(volatile unsigned int *) (addr)) = (b))


#define memset_io(a,b,c)        memset((void *)(a),(b),(c))
#define memcpy_fromio(a,b,c)    memcpy((a),(void *)(b),(c))
#define memcpy_toio(a,b,c)      memcpy((void *)(a),(b),(c))

#define inb(addr)	__raw_readb (addr)
#define inw(addr)	__raw_readw (addr)
#define inl(addr)	__raw_readl (addr)
#define outb(x, addr)	((void) __raw_writeb (x, addr))
#define outw(x, addr)	((void) __raw_writew (x, addr))
#define outl(x, addr)	((void) __raw_writel (x, addr))

/* Some #definitions to keep strange Xilinx code happy */
#define in_8(addr)	inb (addr)
#define in_be16(addr)	inw (addr)
#define in_be32(addr)	inl (addr)
#define in_le16(addr)	le16_to_cpu(inw (addr))
#define in_le32(addr)	le32_to_cpu(inl (addr))

#define out_8(addr,x )	outb (x,addr)
#define out_be16(addr,x )	outw (x,addr)
#define out_be32(addr,x )	outl (x,addr)
#define out_le16(addr,x )	outw (cpu_to_le16(x),addr)
#define out_le32(addr,x )	outl (cpu_to_le32(x),addr)



#define inb_p(port)		inb((port))
#define outb_p(val, port)	outb((val), (port))
#define inw_p(port)		inw((port))
#define outw_p(val, port)	outw((val), (port))
#define inl_p(port)		inl((port))
#define outl_p(val, port)	outl((val), (port))


static inline void io_insb (unsigned long port, void *dst, unsigned long count)
{
	unsigned char *p = dst;
	while (count--)
		*p++ = inb (port);
}
static inline void io_insw (unsigned long port, void *dst, unsigned long count)
{
	unsigned short *p = dst;
	while (count--)
		*p++ = inw (port);
}
static inline void io_insl (unsigned long port, void *dst, unsigned long count)
{
	unsigned long *p = dst;
	while (count--)
		*p++ = inl (port);
}

static inline void
io_outsb (unsigned long port, const void *src, unsigned long count)
{
	const unsigned char *p = src;
	while (count--)
		outb (*p++, port);
}
static inline void
io_outsw (unsigned long port, const void *src, unsigned long count)
{
	const unsigned short *p = src;
	while (count--)
		outw (*p++, port);
}
static inline void
io_outsl (unsigned long port, const void *src, unsigned long count)
{
	const unsigned long *p = src;
	while (count--)
		outl (*p++, port);
}

#define outsb(a,b,l) io_outsb(a,b,l)
#define outsw(a,b,l) io_outsw(a,b,l)
#define outsl(a,b,l) io_outsl(a,b,l)

#define insb(a,b,l) io_insb(a,b,l)
#define insw(a,b,l) io_insw(a,b,l)
#define insl(a,b,l) io_insl(a,b,l)

#define iounmap(addr)				((void)0)
#define ioremap(physaddr, size)			(physaddr)
#define ioremap_writethrough(physaddr, size)	(physaddr)
#define ioremap_nocache(physaddr, size)		(physaddr)
#define ioremap_fullcache(physaddr, size)	(physaddr)


/*
 * DMA mapping functions.  Microblaze has no cache coherency, so we must
 * handle the cache flushing stuff properly.  
 */
#define dma_cache_inv(_start,_size) \
        invalidate_dcache_range(_start, (_start + _size))
#define dma_cache_wback(_start,_size) \
        clean_dcache_range(_start, (_start + _size))
#define dma_cache_wback_inv(_start,_size) \
        flush_dcache_range(_start, (_start + _size))

/*              
 * DMA-consistent mapping functions, since Microblaze doesn't support
 * cache snooping.  These allocate/free a region of uncached mapped
 * memory space for use with DMA devices.  On Microblaze, the config variable
 * CONFIG_XILINX_UNCACHED_SHADOW determines if this is implemented physically,
 * using a shadow memory region outside the processor cacheable range, or
 * if driver/kernel code must simulate it by explicitly flushing the cache
 */     
extern void *consistent_alloc(int gfp, size_t size, dma_addr_t *handle);
extern void consistent_free(void *vaddr);
extern void consistent_sync(void *vaddr, size_t size, int rw);
extern void consistent_sync_page(struct page *page, unsigned long offset,
                                 size_t size, int rw);

#endif /* __MICROBLAZE_IO_H__ */
