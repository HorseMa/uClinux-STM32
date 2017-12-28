/*
 * linux/include/asm-armnommu/arch-isl3893/io.h
 *
 * Copyright (C) 1997-1999 Russell King
 *
 * Modifications:
 *  06-12-1997	RMK	Created.
 *  07-04-1999	RMK	Major cleanup
 *  02-19-2001  gjm     Leveraged for armnommu/dsc21
 */
#ifndef __ASM_ARM_ARCH_IO_H
#define __ASM_ARM_ARCH_IO_H

/*
 * kernel/resource.c uses this to initialize the global ioport_resource struct
 * which is used in all calls to request_resource(), allocate_resource(), etc.
 * --gmcnutt
 */
#define IO_SPACE_LIMIT 0xffffffff

/*
 * If we define __io then asm/io.h will take care of most of the inb & friends
 * macros. It still leaves us some 16bit macros to deal with ourselves, though.
 */
#define __mem_pci(a)		((unsigned long)(a))

#define PCIO_BASE           0x10000000
#define PCIO_SIZE           0x1000
#define PCIO_MEM	       (PCIO_BASE + PCIO_SIZE)
#define PCIO_MEM_SIZE       0x1000

#define __io(a) (PCIO_BASE + (a))

#ifdef CONFIG_PCI

u8 pci_read_byte( int where );
u16 pci_read_word( int where );
u32 pci_read_dword( int where );
void pci_write_byte( int where, u8 v8 );
void pci_write_word(int where, u16 v16);
void pci_write_dword(int where, u32 v32);

/*** 08000000h Flash, 10000000 PCI; uppper limit not checked intentionally (performance)  ***/

#define __arch_getb(a)	    ( (a>=PCIO_BASE) ? pci_read_byte(a)      : (*(volatile unsigned char *)(a))  )
#define __arch_getw(a)      ( (a>=PCIO_BASE) ? pci_read_word(a)      : (*(volatile unsigned short *)(a)) )
#define __arch_getl(a)		( (a>=PCIO_BASE) ? pci_read_dword(a)     : (*(volatile unsigned long *)(a))  )

#define __arch_putb(v,a)	( (a>=PCIO_BASE) ? pci_write_byte(a, v)  : (*(volatile unsigned char *)(a) = (v))  )
#define __arch_putw(v,a)    ( (a>=PCIO_BASE) ? pci_write_word(a, v)  : (*(volatile unsigned short *)(a) = (v)) )
#define __arch_putl(v,a)	( (a>=PCIO_BASE) ? pci_write_dword(a, v) : (*(volatile unsigned long *)(a) = (v))  )

#else

#define __arch_getb(a)      (*(volatile unsigned char *)(a))
#define __arch_putb(v,a)    (*(volatile unsigned char *)(a) = (v))
#define __arch_getw(a)      (*(volatile unsigned short *)(a))
#define __arch_putw(v,a)    (*(volatile unsigned short *)(a) = (v))
#define __arch_getl(a)      (*(volatile unsigned long *)(a))
#define __arch_putl(v,a)    (*(volatile unsigned long *)(a) = (v))

#endif

/*
 * Defining these two gives us ioremap for free. See asm/io.h.
 * --gmcnutt
 */
#define iomem_valid_addr(iomem,sz) (1)
#define iomem_to_phys(iomem) (iomem)

#endif
