#ifndef _ASM_IO_H
#define _ASM_IO_H

#ifdef __KERNEL__

#include <linux/config.h>
#include <asm/virtconvert.h>
#include <asm/mb-regs.h>
#include <linux/delay.h>

/*
 * These are for ISA/PCI shared memory _only_ and should never be used
 * on any other type of memory, including Zorro memory. They are meant to
 * access the bus in the bus byte order which is little-endian!.
 *
 * readX/writeX() are used to access memory mapped devices. On some
 * architectures the memory mapped IO stuff needs to be accessed
 * differently. On the m68k architecture, we just read/write the
 * memory location directly.
 */
/* ++roman: The assignments to temp. vars avoid that gcc sometimes generates
 * two accesses to memory, which may be undesireable for some devices.
 */

/*
 * swap functions are sometimes needed to interface little-endian hardware
 */

/*
 * CHANGES
 * 
 * 020325   Added some #define's for the COBRA5272 board
 *          (hede)
 */
static inline unsigned short _swapw(unsigned short v)
{
    return ((v << 8) | (v >> 8));
}

static inline unsigned long _swapl(unsigned long v)
{
    return ((v << 24) | ((v & 0xff00) << 8) | ((v & 0xff0000) >> 8) | (v >> 24));
}

//#define __iormb() asm volatile("membar")
//#define __iowmb() asm volatile("membar")

#define __raw_readb(addr) __builtin_read8((volatile void *) (addr))
#define __raw_readw(addr) __builtin_read16((volatile void *) (addr))
#define __raw_readl(addr) __builtin_read32((volatile void *) (addr))

#define __raw_writeb(datum, addr) __builtin_write8((volatile void *) (addr), datum)
#define __raw_writew(datum, addr) __builtin_write16((volatile void *) (addr), datum)
#define __raw_writel(datum, addr) __builtin_write32((volatile void *) (addr), datum)

static inline void io_outsb(unsigned int addr, const void *buf, int len)
{
	unsigned long __ioaddr = (unsigned long) addr;
	const uint8_t *bp = buf;

	while (len--)
		__builtin_write8((volatile void *) __ioaddr, *bp++);
}

static inline void io_outsw(unsigned int addr, const void *buf, int len)
{
	unsigned long __ioaddr = (unsigned long) addr;
	const uint16_t *bp = buf;

	while (len--)
		__builtin_write16((volatile void *) __ioaddr, (*bp++));
}

extern void __outsl_ns(unsigned int addr, const void *buf, int len);
extern void __outsl_sw(unsigned int addr, const void *buf, int len);
static inline void __outsl(unsigned int addr, const void *buf, int len, int swap)
{
	unsigned long __ioaddr = (unsigned long) addr;

	if (!swap)
		__outsl_ns(__ioaddr, buf, len);
	else
		__outsl_sw(__ioaddr, buf, len);
}

static inline void io_insb(unsigned int addr, void *buf, int len)
{
	unsigned long __ioaddr = (unsigned long) addr;
	uint8_t *bp = buf;

	while (len--)
		*bp++ = __builtin_read8((volatile void *) __ioaddr);
}

static inline void io_insw(unsigned int addr, void *buf, int len)
{
	unsigned long __ioaddr = (unsigned long) addr;
	uint16_t *bp = buf;

	while (len--)
		*bp++ = __builtin_read16((volatile void *) __ioaddr);
}

extern void __insl_ns(unsigned int addr, void *buf, int len);
extern void __insl_sw(unsigned int addr, void *buf, int len);
static inline void __insl(unsigned int addr, void *buf, int len, int swap)
{
	unsigned long __ioaddr = (unsigned long) addr;

	if (!swap)
		__insl_ns(__ioaddr, buf, len);
	else
		__insl_sw(__ioaddr, buf, len);
}

/*
 *	make the short names macros so specific devices
 *	can override them as required
 */

#define memset_io(a,b,c)	memset((void *)(a),(b),(c))
#define memcpy_fromio(a,b,c)	memcpy((a),(void *)(b),(c))
#define memcpy_toio(a,b,c)	memcpy((void *)(a),(b),(c))

static inline uint8_t inb(unsigned long addr)
{
	return __builtin_read8((volatile void *)addr);
}

static inline uint16_t inw(unsigned long addr)
{
	uint16_t ret = __builtin_read16((volatile void *)addr);

	if (__is_PCI_IO(addr))
		ret = _swapw(ret);

	return ret;
}

static inline uint32_t inl(unsigned long addr)
{
	uint32_t ret = __builtin_read32((volatile void *)addr);

	if (__is_PCI_IO(addr))
		ret = _swapl(ret);

	return ret;
}

static inline void outb(uint8_t datum, unsigned long addr)
{
	__builtin_write8((volatile void *)addr, datum);
}

static inline void outw(uint16_t datum, unsigned long addr)
{
	if (__is_PCI_IO(addr))
		datum = _swapw(datum);
	__builtin_write16((volatile void *)addr, datum);
}

static inline void outl(uint32_t datum, unsigned long addr)
{
	if (__is_PCI_IO(addr))
		datum = _swapl(datum);
	__builtin_write32((volatile void *)addr, datum);
}

#define inb_p(addr)	inb(addr)
#define inw_p(addr)	inw(addr)
#define inl_p(addr)	inl(addr)
#define outb_p(x,addr)	outb(x,addr)
#define outw_p(x,addr)	outw(x,addr)
#define outl_p(x,addr)	outl(x,addr)

#define outsb(a,b,l)	io_outsb(a,b,l)
#define outsw(a,b,l)	io_outsw(a,b,l)
#define outsl(a,b,l)	__outsl(a,b,l,0)

#define insb(a,b,l)	io_insb(a,b,l)
#define insw(a,b,l)	io_insw(a,b,l)
#define insl(a,b,l)	__insl(a,b,l,0)

#define IO_SPACE_LIMIT	0xffffffff

static inline uint8_t readb(volatile void *addr)
{
	return __builtin_read8(addr);
}

static inline uint16_t readw(volatile void *addr)
{
	uint16_t ret =	__builtin_read16(addr);

	if (__is_PCI_MEM(addr))
		ret = _swapw(ret);
	return ret;
}

static inline uint32_t readl(volatile void *addr)
{
	uint32_t ret =	__builtin_read32(addr);

	if (__is_PCI_MEM(addr))
		ret = _swapl(ret);

	return ret;
}

static inline void writeb(uint8_t datum, volatile void *addr)
{
	__builtin_write8(addr, datum);
	if (__is_PCI_MEM(addr))
		__flush_PCI_writes();
}

static inline void writew(uint16_t datum, volatile void *addr)
{
	if (__is_PCI_MEM(addr))
		datum = _swapw(datum);

	__builtin_write16(addr, datum);
	if (__is_PCI_MEM(addr))
		__flush_PCI_writes();
}

static inline void writel(uint32_t datum, volatile void *addr)
{
	if (__is_PCI_MEM(addr))
		datum = _swapl(datum);

	__builtin_write32(addr, datum);
	if (__is_PCI_MEM(addr))
		__flush_PCI_writes();
}


/* Values for nocacheflag and cmode */
#define IOMAP_FULL_CACHING		0
#define IOMAP_NOCACHE_SER		1
#define IOMAP_NOCACHE_NONSER		2
#define IOMAP_WRITETHROUGH		3

extern void *__ioremap(unsigned long physaddr, unsigned long size, int cacheflag);
extern void __iounmap(void *addr, unsigned long size);

extern inline void *ioremap(unsigned long physaddr, unsigned long size)
{
	return __ioremap(physaddr, size, IOMAP_NOCACHE_SER);
}
extern inline void *ioremap_nocache(unsigned long physaddr, unsigned long size)
{
	return __ioremap(physaddr, size, IOMAP_NOCACHE_SER);
}
extern inline void *ioremap_writethrough(unsigned long physaddr, unsigned long size)
{
	return __ioremap(physaddr, size, IOMAP_WRITETHROUGH);
}
extern inline void *ioremap_fullcache(unsigned long physaddr, unsigned long size)
{
	return __ioremap(physaddr, size, IOMAP_FULL_CACHING);
}

extern void iounmap(void *addr);

#endif /* __KERNEL__ */

#endif /* _ASM_IO_H */
