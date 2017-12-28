/*
 *  linux/include/asm-arm/ide.h
 *
 *  Copyright (C) 1994-1996  Linus Torvalds & authors
 */

/*
 *  This file contains the i386 architecture specific IDE code.
 */

#ifndef __ASMARM_IDE_H
#define __ASMARM_IDE_H

#ifdef __KERNEL__

#ifdef CONFIG_ARCH_EDB7312
#define MAX_HWIFS	1
#endif

#ifndef MAX_HWIFS
#define MAX_HWIFS	4
#endif

#define	ide__sti()	__sti()

#include <asm/arch/ide.h>

#ifdef CONFIG_ARCH_EDB7312
#include <asm/hardware/clps7111.h>
#include <asm/io.h>

#define ide_request_irq(irq,hand,flg,dev,id)   request_irq((irq),(hand),SA_SHIRQ,(dev),(id))
#define ide_free_irq(irq,dev_id)               free_irq((irq), (dev_id))
#define ide_check_region(from,extent)          (0)
#define ide_request_region(from,extent,name)
#define ide_release_region(from,extent)

#define HAVE_ARCH_IN_BYTE
#define HAVE_ARCH_OUT_BYTE

#define OUT_BYTE(b,p)  (clps_writel(clps_readl(MEMCFG2) | 0x0300, MEMCFG2), \
                       __raw_writeb(b, p), \
                       clps_writel(clps_readl(MEMCFG2) & ~0x0300, MEMCFG2))
#define OUT_WORD(w,p)  __raw_writew(w, p)
#define IN_BYTE(p)     edb7312_ide_in_byte (p)
#define IN_WORD(p)     __raw_readw(p)

static inline int edb7312_ide_in_byte(int p)
{
	int r;

	clps_writel(clps_readl(MEMCFG2) | 0x0300, MEMCFG2);
	r = __raw_readb(p);
	clps_writel(clps_readl(MEMCFG2) & ~0x0300, MEMCFG2);

	return (r);
}

/* convert old to new style ide i/o primitives */



#endif

/*
 * We always use the new IDE port registering,
 * so these are fixed here.
 */
#define ide_default_io_base(i)		((ide_ioreg_t)0)
#define ide_default_irq(b)		(0)

#include <asm-generic/ide_iops.h>

#endif /* __KERNEL__ */

#endif /* __ASMARM_IDE_H */
