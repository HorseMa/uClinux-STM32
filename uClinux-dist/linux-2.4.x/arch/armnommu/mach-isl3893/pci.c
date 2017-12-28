/*  $Header$
 *
 *  Copyright (C) 2002 Intersil Americas Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/init.h>
#include <linux/delay.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/mach/pci.h>
#include <asm/arch/pci.h>
#include <asm/arch/gpio.h>
#include <asm/arch/dma.h>

#ifdef CONFIG_PCI

#define WORD_SIZE               4   /* bytes */

#define HOST_PCI_BASE           (PCIO_BASE)
#define HOST_MEM_OFFSET         0x00001000
#define HOST_MEM_WINDOW         0x00001000
#define HOST_MEM_MASK          (HOST_MEM_WINDOW-1)

#define HOST_BASE_DMA           0xc0000000
#define HOST_TX_DMA             0xF00
#define HOST_RX_DMA             0xF20

#define HOST_PCI_MAP	        (PCIO_BASE)
#define HOST_MEM_MAP	        (HOST_PCI_MAP + HOST_MEM_OFFSET)

#define PCI_ARCH_MEM_READ       0x06
#define PCI_ARCH_MEM_WRITE      0x07
#define PCI_ARCH_CONFIG_READ	0x0a    // 0xfa
#define PCI_ARCH_CONFIG_WRITE   0x0b    // 0xfb

#define DEBUG_                  0x01
#define DEBUG_CONFIG            0x02
#define DEBUG_IO                0x04
#define DEBUG_RW                0x08

unsigned long pci_debug_mask = DEBUG_;

#define DEBUG
#ifdef DEBUG
#define DBG( mask, pr... )   if ( mask & pci_debug_mask ) { printk( pr ); }
#else
#define DBG( mask, pr... )
#endif

int pci_arch_master_wait( int size );
void pci_arch_master_setup( int addr, int size, int mode );

int pci_arch_read( int offset );
int pci_arch_write( int offset, int value );

int pci_arch_read_config( int offset );
int pci_arch_write_config( int offset, int value );

/*
u8 pci_read_byte( int where );
u16 pci_read_word( int where );
u32 pci_read_dword( int where );
void pci_arch_write_byte( int where, u8 v8 );
void pci_arch_write_word(int where, u16 v16);
void pci_arch_write_dword(int where, u32 v32);
*/

static int __init pci_arch_map_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
	return IRQ_PCIINT;
    //return IRQ_GP1IRQINT;
}

static void __init pci_arch_setup_resources(struct resource **resource);

static void pci_arch_init( void * );
static void pci_arch_reset ( void );

struct hw_pci isl38xx_pci __initdata = {
	setup_resources:	pci_arch_setup_resources,
    init:			    pci_arch_init,
	mem_offset:		    PCIO_BASE,
	swizzle:		    no_swizzle,
	map_irq:		    pci_arch_map_irq
};

static spinlock_t pci_arch_lock = SPIN_LOCK_UNLOCKED;

int pci_arch_master_wait( int size )
{
    int timeout = 0x80*size;

    while( !(uPCI->ARMIntReg & PCIARMIntRegMASTERCOMPLETE) )
    {
        if( --timeout <= 0 ) {
            return( -1 );
        }
    }
    return( 0 );
}

void pci_arch_master_setup( int addr, int size, int mode )
{
   uPCI->ARMIntAckReg = PCIARMIntAckRegMASTERCOMPLETE;
   uPCI->MasHostMemAddr = addr;
   uPCI->MasLenReg = size;
   uPCI->MasControlReg = mode;		// ignore length
}

int pci_arch_read( int offset)
{
    int value;

    uPCI->MasOptReg = PCIMasOptRegOPTIONENABLE | PCI_ARCH_MEM_READ;
    pci_arch_master_setup( offset, 1 , PCIMasControlRegENABLE
    		             | PCIMasControlRegUSEDATAREG );
    pci_arch_master_wait( 1 );
    value = uPCI->MasDataReg;
    uPCI->MasOptReg = 0;

    return( value );
}

int pci_arch_write( int offset, int value )
{
    uPCI->MasOptReg = PCIMasOptRegOPTIONENABLE | PCI_ARCH_MEM_WRITE;
    uPCI->MasDataReg = value;
    pci_arch_master_setup( offset, 1, PCIMasControlRegENABLE
            | PCIMasControlRegUSEDATAREG | PCIMasControlRegTOHOST );
    return( pci_arch_master_wait( 1 ) );
}

int pci_arch_read_config( int offset )
{
    int value;

    uPCI->MasOptReg = PCIMasOptRegOPTIONENABLE | PCI_ARCH_CONFIG_READ;
    pci_arch_master_setup( offset, 1 , PCIMasControlRegENABLE | PCIMasControlRegUSEDATAREG );
    pci_arch_master_wait( 1 );
    value = uPCI->MasDataReg;
    uPCI->MasOptReg = 0;

    return( value );
}

int pci_arch_write_config( int offset, int value )
{
    int error;

    uPCI->MasOptReg  = PCIMasOptRegOPTIONENABLE | PCI_ARCH_CONFIG_WRITE;
    uPCI->MasDataReg = value;
    pci_arch_master_setup( offset, 1, PCIMasControlRegENABLE | PCIMasControlRegUSEDATAREG
                           | PCIMasControlRegTOHOST );
    error = pci_arch_master_wait( 1 );
    uPCI->MasOptReg = 0;

    return( error );
}

/***********************************************************************/

u8 pci_read_byte( int where )
{
	unsigned int  offset;
    unsigned int  align;
	u32           v32;
    u8            v8;

    offset = where & (~0x3);
    align  = where & 0x3;

    v32 = pci_arch_read( offset );
    v8 = (u8)(v32 >> (8*align)) & 0xFF ;

    DBG( DEBUG_IO, "read_byte : %08x %02x\n", where, v8 );

    return v8;
}

u16 pci_read_word( int where )
{
	unsigned long offset;
    unsigned int  align;
	u32           v32;
    u16           v16;

    offset = where & (~0x3);
    align  = where & 0x3;

    v32 = pci_arch_read( offset );
    v16 = (u16)(v32 >> (8*align)) & 0xFFFF ;

    DBG( DEBUG_IO, "read_word : %08x %04x\n", where, v16 );

    return v16;
}

u32 pci_read_dword( int where )
{
	unsigned long offset;
    unsigned int  align;
	u32           v32;

    offset = where & (~0x3);
    align  = where & 0x3;

    v32 = pci_arch_read( offset );

    DBG( DEBUG_IO, "read_dword : %08x %08x\n", where, v32 );

    return v32;
}

void pci_write_byte( int where, u8 v8 )
{
	unsigned long flags;
	unsigned int  offset;
    unsigned int  align;
	u32           v32;

    offset = where & (~0x3);
    align  = where & 0x3;

	spin_lock_irqsave(&pci_arch_lock, flags);

    v32 = pci_arch_read( offset );

    v32 &= ~(0xFF << (8*align));
    v32 |= v8 << (8*align);

    pci_arch_write( offset, v32 );

    spin_unlock_irqrestore(&pci_arch_lock, flags);

    DBG( DEBUG_IO, "write_byte : %08x %02x\n", where, v8 );
}

void pci_write_word(int where, u16 v16)
{
	unsigned long flags;
	unsigned int  offset;
    unsigned int  align;
	u32           v32;

    offset = where & (~0x3);
    align  = where & 0x3;

	spin_lock_irqsave(&pci_arch_lock, flags);

    v32 = pci_arch_read( offset );

    v32 &= ~(0xFFFF << (8*align));
    v32 |= v16 << (8*align);

    pci_arch_write( offset, v32 );

    spin_unlock_irqrestore(&pci_arch_lock, flags);

    DBG( DEBUG_IO, "write_word : %08x %04x\n", where, v16 );
}

void pci_write_dword(int where, u32 v32)
{
	unsigned long flags;
	unsigned int  offset;
    unsigned int  align;

    offset = where & (~0x3);
    align  = where & 0x3;

	spin_lock_irqsave(&pci_arch_lock, flags);

    pci_arch_write( offset, v32 );

    spin_unlock_irqrestore(&pci_arch_lock, flags);

    DBG( DEBUG_IO, "write_dword : %08x %08x\n", where, v32 );
}

/***********************************************************************/

int pci_arch_read_config_byte(struct pci_dev *dev, int where, u8 *pv8)
{
	unsigned long flags;
	unsigned int  offset;
    unsigned int  align;
	u32           v32;

    if ( dev->devfn != 0 ) { return -1; }

    offset = where & (~0x3);
    align  = where & 0x3;

	spin_lock_irqsave(&pci_arch_lock, flags);

    v32 = pci_arch_read_config( offset );

    spin_unlock_irqrestore(&pci_arch_lock, flags);

    *pv8 = (u8)(v32 >> (8*align)) & 0xFF ;

    DBG( DEBUG_CONFIG, "read_config_byte : %08x %02x\n", where, *pv8 );

    return PCIBIOS_SUCCESSFUL;
}

int pci_arch_read_config_word(struct pci_dev *dev, int where, u16 *pv16)
{
	unsigned long offset;
	unsigned long flags;
    unsigned int  align;
	u32           v32;

    if ( dev->devfn != 0 ) { return -1; }

    offset = where & (~0x3);
    align  = where & 0x3;

	spin_lock_irqsave(&pci_arch_lock, flags);

    v32 = pci_arch_read_config( offset );

    spin_unlock_irqrestore(&pci_arch_lock, flags);

    *pv16 = (u16)(v32 >> (8*align)) & 0xFFFF ;

    DBG( DEBUG_CONFIG, "read_config_word : %08x %04x\n", where, *pv16 );

    return PCIBIOS_SUCCESSFUL;
}

int pci_arch_read_config_dword(struct pci_dev *dev, int where, u32 *pv32)
{
	unsigned long offset;
	unsigned long flags;
    unsigned int  align;

    if ( dev->devfn != 0 ) { return -1; }

    offset = where & (~0x3);
    align  = where & 0x3;

	spin_lock_irqsave(&pci_arch_lock, flags);

    *pv32 = pci_arch_read_config( offset );

    spin_unlock_irqrestore(&pci_arch_lock, flags);

    DBG( DEBUG_CONFIG, "read_config_dword : %08x %08x\n", where, *pv32 );

    return PCIBIOS_SUCCESSFUL;
}

int pci_arch_write_config_byte(struct pci_dev *dev, int where, u8 v8 )
{
	unsigned long flags;
	unsigned int  offset;
    unsigned int  align;
	u32           v32;

    if ( dev->devfn != 0 ) { return -1; }

    offset = where & (~0x3);
    align  = where & 0x3;

	spin_lock_irqsave(&pci_arch_lock, flags);

    v32 = pci_arch_read_config( offset );

    v32 &= ~(0xFF << (8*align));
    v32 |= v8 << (8*align);

    pci_arch_write_config( offset, v32 );

    spin_unlock_irqrestore(&pci_arch_lock, flags);

    DBG( DEBUG_CONFIG, "write_config_byte : %08x %02x\n", where, v8 );

    return PCIBIOS_SUCCESSFUL;
}

int pci_arch_write_config_word(struct pci_dev *dev, int where, u16 v16)
{
	unsigned long flags;
	unsigned int  offset;
    unsigned int  align;
	u32           v32;

    if ( dev->devfn != 0 ) { return -1; }

    offset = where & (~0x3);
    align  = where & 0x3;
    // assert ( align == 0x00 || align == 0x10 );

	spin_lock_irqsave(&pci_arch_lock, flags);

    v32 = pci_arch_read_config( offset );

    v32 &= ~(0xFFFF << (8*align));
    v32 |= v16 << (8*align);

    pci_arch_write_config( offset, v32 );

    spin_unlock_irqrestore(&pci_arch_lock, flags);

    DBG( DEBUG_CONFIG, "write_config_word : %08x %04x\n", where, v16 );

    return PCIBIOS_SUCCESSFUL;
}

int pci_arch_write_config_dword(struct pci_dev *dev, int where, u32 v32)
{
	unsigned long flags;
	unsigned int  offset;
    unsigned int  align;

    if ( dev->devfn != 0 ) { return -1; }

    offset = where & (~0x3);
    align  = where & 0x3;
    // assert ( align == 0x00 );

	spin_lock_irqsave(&pci_arch_lock, flags);

    pci_arch_write_config( offset, v32 );

    spin_unlock_irqrestore(&pci_arch_lock, flags);

    DBG( DEBUG_CONFIG, "write_config_dword : %08x %08x\n", where, v32 );

    return PCIBIOS_SUCCESSFUL;
}

/***********************************************************************/

#if 0

// Target DMA write broken: -during first transfer word 0 is copied in word 9
//                          -every subsequent transfer first word contains bogus, all word copies are
//                           shifted by 4, last word not copied
// Fix: - dummy first copy
//      - shift down to address one word
//      - copy one extra word

int pci_arch_block_write ( void *to, void *from, int size )
{
    int flags;
    int words = (size + (WORD_SIZE-1) + ((int) from & (WORD_SIZE-1)) ) >> 2; /* div WORD_SIZE optimized */

    DBG( DEBUG_IO, "pci_arch_dma_write(%p, %p, %d)\n", to, from, size );

    ((int *)to)--; size += WORD_SIZE;

    pci_arch_write( HOST_PCI_BASE + PCIDevStatReg, 0 );

    /*** setup target DMA ***/

    // 2 word FIFO in target PCI controller; program twice to make sure everything reaches the DMA controller

    pci_arch_write( HOST_PCI_BASE + PCIDirectMemBase, HOST_BASE_DMA );

    pci_arch_write( HOST_PCI_BASE + HOST_MEM_OFFSET + HOST_RX_DMA + DMACONTROL, DMACONTROLEnable );
    pci_arch_write( HOST_PCI_BASE + HOST_MEM_OFFSET + HOST_RX_DMA + DMAMAXCNT,  words << 2 ); /* bytes */
    pci_arch_write( HOST_PCI_BASE + HOST_MEM_OFFSET + HOST_RX_DMA + DMABASE,    (int)to );

    pci_arch_write( HOST_PCI_BASE + PCIDirectMemBase, HOST_BASE_DMA );

    pci_arch_write( HOST_PCI_BASE + HOST_MEM_OFFSET + HOST_RX_DMA + DMACONTROL, DMACONTROLEnable );
    pci_arch_write( HOST_PCI_BASE + HOST_MEM_OFFSET + HOST_RX_DMA + DMAMAXCNT,  words << 2 ); /* bytes */
    pci_arch_write( HOST_PCI_BASE + HOST_MEM_OFFSET + HOST_RX_DMA + DMABASE,    (int)to );

    pci_arch_write( HOST_PCI_BASE + PCIDevStatReg, PCIDevStatRegSetTargetDMA );

    /*** setup my DMA ***/
    uHostTxDma->CONTROL = DMACONTROLEnable;
    uHostTxDma->MAXCNT  = words << 2;   /* bytes */
    uHostTxDma->BASE    = (int) from;

    /*** Load DMA ***/
    uPCI->MasOptReg      = 0;
    uPCI->ARMIntAckReg   = PCIARMIntAckRegMASTERCOMPLETE;
    uPCI->MasHostMemAddr = HOST_PCI_MAP + HOST_MEM_OFFSET;
    uPCI->MasLenReg      = words;
    uPCI->MasControlReg  = PCIMasControlRegENABLE | PCIMasControlRegTOHOST;

    while( !(uPCI->ARMIntReg & PCIARMIntRegMASTERCOMPLETE) );
    //while ( !(uHostTxDma->STATUS & DMASTATUSStallInt) );

    pci_arch_write( HOST_PCI_BASE + PCIDevStatReg, 0 ); // Turn off PCIDevStatRegSetTargetDMA
    pci_arch_write( HOST_PCI_BASE + PCIDevStatReg, 0 ); // Since the PCI/DMA fsm writes the first
                                                        // word to the data stream a 2nd write is needed
    return( (int)from & (WORD_SIZE-1) );
}

#else

int pci_arch_block_write( void *to, void *from, int size )
{
    int offset, *word;
    int words = (size + (WORD_SIZE-1) + ((int) from & (WORD_SIZE-1)) ) >> 2; /* div WORD_SIZE optimized */

    DBG( DEBUG_IO, "pci_arch_block_write(%p, %p, %d)\n", to, from, size );

    pci_arch_write( HOST_PCI_BASE + PCIDevStatReg, 0 );     // Turn off PCIDevStatRegSetTargetDMA
                                                            // set the Direct Memory Base Register on the host
    // 2 word FIFO in target PCI controller; program twice to make sure address reaches the register
    pci_arch_write( HOST_PCI_BASE + PCIDirectMemBase, (int) to & ~HOST_MEM_MASK );
    pci_arch_write( HOST_PCI_BASE + PCIDirectMemBase, (int) to & ~HOST_MEM_MASK );

    for( word=(int *)from, offset=((int)to & HOST_MEM_MASK); words > 0; offset=(offset+WORD_SIZE) & HOST_MEM_MASK, word++, words-- )
    {
        pci_arch_write( HOST_PCI_BASE + HOST_MEM_OFFSET + offset, *word );

        if( offset == (HOST_MEM_WINDOW-WORD_SIZE) )
        {
            pci_arch_write( HOST_PCI_BASE + PCIDirectMemBase, ((int) to + HOST_MEM_WINDOW) & ~HOST_MEM_MASK );
        }
    }

    return( (int)from & (WORD_SIZE-1) );
}

#endif

/***********************************************************************/

static struct resource pci_arch_ioport = {
	name:	"PCI isl38xx ioport",
	start:	PCIO_BASE,
	end:	PCIO_BASE + PCIO_MEM_SIZE,
	flags:	IORESOURCE_IO,
};

static struct resource pci_arch_iomem = {
	name:	"PCI isl38xx iomem",
	start:	PCIO_MEM,
	end:	PCIO_MEM + PCIO_MEM_SIZE,
	flags:	IORESOURCE_MEM,
};

/***********************************************************************/

static void __init pci_arch_setup_resources(struct resource **resource)
{

	if (request_resource(&ioport_resource, &pci_arch_ioport))
		printk("PCI: unable to allocate non-prefetchable io region\n");

	if (request_resource(&iomem_resource, &pci_arch_iomem))
		printk("PCI: unable to allocate non-prefetchable memory region\n");

    resource[0] = &ioport_resource;         /* the io resource for this bus */
    resource[1] = &iomem_resource;          /* the mem resource for this bus */
	resource[2] = NULL;                     /* the prefetch mem resource for this bus */
}

static struct pci_ops isl38xx_ops = {
	read_byte:	pci_arch_read_config_byte,
	read_word:	pci_arch_read_config_word,
	read_dword:	pci_arch_read_config_dword,
	write_byte:	pci_arch_write_config_byte,
	write_word:	pci_arch_write_config_word,
	write_dword:    pci_arch_write_config_dword,
};

/***********************************************************************/

static void __init pci_arch_init(void *sysdata)     /* struct pci_sys_data sysdata  */
{
	DBG( DEBUG_, "pci_arch_init: %08x\n", (unsigned int)sysdata );

    /* Initialize local controller  */

    uGPIO3->DDR   = gpio_set( 0x80 );
    uGPIO3->DR    = gpio_set( 0x80 );    /* GP 3_7 de-assert reset */

    //uGPIO1->DDR   = gpio_clr( 0x08 );    /* GP 1_3 input */
    //uGPIO1->INTCL = gpio_set( 0x08 );    /* GP 1_3 interrupt clear */

    //uGPIO1->EENR  = gpio_set( 0x08 );    /* GP 1_3 interrupt edge triggered */
    //uGPIO1->SNR   = gpio_set( 0x08 );    /* GP 1_3 trigger on falling edge */

    //uGPIO1->EENR  = gpio_clr( 0x08 );    /* GP 1_3 interrupt level triggered */
    //uGPIO1->SNR   = gpio_set( 0x08 );    /* GP 1_3 trigger active low */

    //uGPIO1->IRQ   = gpio_set( 0x08 );    /* GP 1_3 interrupt mask */

    DBG( DEBUG_, "pci_arch_init : %08x\n", uGPIO1->IRQ );

    uPCI->ConfigLoadReg = 0x80000000;    /* enable device   */

    uPCI->ARMIntAckReg = PCIARMIntAckRegPCI_FATAL_ERROR;
    uPCI->ARMIntAckReg = PCIARMIntAckRegPCI_PARITY_ERROR;

    /* set Memory Space / Bus Master bits */
    pci_arch_write_config( PCI_COMMAND,
        pci_arch_read_config( PCI_COMMAND ) | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER );
    pci_arch_write_config( PCI_TRDY_TIMEOUT_VALUE,  0xFF );
    pci_arch_write_config( PCI_RETRY_TIMEOUT_VALUE, 0xFF );
    pci_arch_write_config( PCI_BASE_ADDRESS_0, PCIO_BASE ) ; /* set Base Address Register 0 to PCIO_BASE  */

    pci_scan_bus (0, &isl38xx_ops, sysdata);
}

static void pci_arch_reset ( void )
{
    u32 v32;

    v32 = pci_arch_read( PCIO_BASE + PCIDevStatReg );
	DBG( DEBUG_, "pci_arch_reset(%x)\n", v32 );
    pci_arch_write( PCIO_BASE + PCIDevStatReg, v32 | PCIDevStatRegSetHostReset );
    udelay(10);
    pci_arch_write( PCIO_BASE + PCIDevStatReg, v32 ) ;
    udelay(50000);
}

EXPORT_SYMBOL(pci_read_dword);
EXPORT_SYMBOL(pci_write_dword);

#endif

