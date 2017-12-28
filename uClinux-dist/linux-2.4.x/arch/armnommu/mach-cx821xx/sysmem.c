/****************************************************************************
* $Header:   S:/Embedded/archives/Linux/BSP_CX821xx/uClinux2.4.6/linux/arch/armnommu/mach-cx821xx/sysmem.c-arc   1.0   Mar 19 2003 13:22:06   davidsdj  $
*
*  Copyright(c)2002  Conexant Systems
*
*****************************************************************************/

// SYSMEM: Unformatted system (nonCache) memory (SRAM or SDRAM) manager
// This module provide the services to allocate and free a block of system memory.
//  The services are slow, the idea is client allocate a block of memory and 
// manage it by itself.
// Depend on the implementation it can be OS_DEPENDENT or OS_INDEPENDENT

// Consideration:
//  DMA buffer must be at 8-byte aligned, we make it at 16-byte aligned now
// Requirement:
//      Begin and Length of SRAM 
//      Begin and Length of SDRAM 
// 

#define SYSMEM_MODULE
#include <asm/arch/bsptypes.h>
#include <asm/arch/bspcfg.h>
#include <linux/config.h>
#include <asm/arch/cnxtbsp.h>

#include <asm/arch/sysmem.h>
#include <linux/string.h>
#include <asm/memory.h>
#include <linux/kernel.h>

#define BYTE16_ALIGN_PTR(x)                      \
{                                               \
    ( unsigned long) x = ((( unsigned long) x + 0xF ) & 0xFFFFFFF0 );   \
}
#define	DRAM_8M_SIZE		0x800000


//SYSMEM APIs

static UINT32   SRAMMemTopPtr = SRAMSTART;
static UINT32   nonCachedSDRAMMemTopPtr = END_MEM;

/*******************************************************************************

FUNCTION NAME:
		SYSMEM_GeneralInit
		
ABSTRACT:
		
DETAILS:

*******************************************************************************/

void SYSMEM_GeneralInit(void)
{
	// Stub
}


/*******************************************************************************

FUNCTION NAME:
		SYSMEM_GeneralClose
		
ABSTRACT:
		
DETAILS:

*******************************************************************************/

void SYSMEM_GeneralClose( void)
{
	// Stub
}


/*******************************************************************************

FUNCTION NAME:
		SYSMEM_Allocate_BlockMemory
		
ABSTRACT:
		
DETAILS:

	!!!!!!!!!!!! WARNING WARNING WARNING WARNING !!!!!!!!!!!!!!

	THIS MEMORY ALLOCATION FUNCTION IS NOT VERY SOPHISTICATED.
	IT IS SIMPLY A POINTER TO MEMORY THAT GETS ADVANCED BY THE
	REQUESTED BLOCK SIZE FOR EACH MEMORY REQUEST.  THEREFOR, ANY
	RELEASE OF THE BLOCKS OF MEMORY ALLOCATED BY THIS FUNCTION
	MUST COME IN THE REVERSE ORDER OF WHAT THEY WERE ALLOCATED.

	!!!!!!!!!!!! WARNING WARNING WARNING WARNING !!!!!!!!!!!!!!

*******************************************************************************/

unsigned char * SYSMEM_Allocate_BlockMemory( int ifSram, int sysMemSize, int alignment )
{
    void	*pMem;
    UINT32	totalSize = 0;

	// Make the request increments of 16 bytes
	if( sysMemSize & 0xF )
	{
		totalSize = 16;
	}

	// Which kind of ram is being requested?
	switch( ifSram )
    {
		case 0:
			// SDRAM
			// Is the SDRAM memory request greater than what is remaining?
			if( sysMemSize > ( (DRAM_BASE + DRAM_8M_SIZE) - nonCachedSDRAMMemTopPtr ) )
			{
		        pMem = NULL;
			}
			else
			{
				totalSize += ( sysMemSize & 0xFFFF0 );
				pMem = ( void * )( nonCachedSDRAMMemTopPtr );
				if ((( char * ) pMem + totalSize ) < ( char * )( DRAM_BASE + DRAM_8M_SIZE ))
			    {
			        nonCachedSDRAMMemTopPtr += totalSize;
			        memset (( char * ) pMem, 0, totalSize );
			    }
			    else
			    {
			        pMem = NULL;
			    }
			}
			printk( KERN_DEBUG "SDRAM start %lx end %lx\n", pMem, nonCachedSDRAMMemTopPtr );
			break;

		case 1:
			// SRAM
			// Is the SRAM memory request greater than what is remaining?
			if( sysMemSize > ( SRAMEND - SRAMMemTopPtr ) )
			{
		        pMem = NULL;
			}
			else
			{
				totalSize += ( sysMemSize & 0x7FF0 );
				pMem = ( void * )( SRAMMemTopPtr );
				if ((( char * ) pMem + totalSize ) < ( char * )( SRAMEND ))
			    {
			        SRAMMemTopPtr += totalSize;
			        memset (( char * ) pMem, 0, totalSize );
			    }
			    else
			    {
			        pMem = NULL;
			    }
			}
			printk( KERN_DEBUG "SRAM start %lx end %lx\n", pMem, SRAMMemTopPtr );
			break;

		default:
	        pMem = NULL;
			break;
	}

	return ( pMem  );
}


/*******************************************************************************

FUNCTION NAME:
		SYSMEM_Release_BlockMemory
		
ABSTRACT:
		
DETAILS:

	!!!!!!!!!!!! WARNING WARNING WARNING WARNING !!!!!!!!!!!!!!

	THIS MEMORY RELEASE FUNCTION IS NOT VERY SOPHISTICATED.
	IT IS SIMPLY A POINTER TO MEMORY THAT GETS SET TO THE
	POINTER VALUE THAT IS BEING PASSED TO IT.  THEREFOR, ANY
	RELEASE OF THE BLOCKS OF MEMORY ALLOCATED BY THE ALLOC FUNCTION
	MUST COME IN THE REVERSE ORDER OF WHAT THEY WERE ALLOCATED.

	!!!!!!!!!!!! WARNING WARNING WARNING WARNING !!!!!!!!!!!!!!

*******************************************************************************/

int SYSMEM_Release_BlockMemory( int ifSram, unsigned char *sysMemPtr )
{
    UINT32	totalSize = 0;

	switch( ifSram )
    {
		case 0:
			// SDRAM
			nonCachedSDRAMMemTopPtr = sysMemPtr;
			printk( KERN_DEBUG "ReleaseMem SDRAM ");
			printk( KERN_DEBUG "%lx\n", nonCachedSDRAMMemTopPtr );
			break;
		case 1:
			// SRAM
			SRAMMemTopPtr = sysMemPtr;
			printk( KERN_DEBUG "ReleaseMem SRAM ");
			printk( KERN_DEBUG "%lx\n", SRAMMemTopPtr );
			break;

		default:
			break;
	}
	return 0;
}

/*******************************************************************************

FUNCTION NAME:
		SYSMEM_QueryMemory
		
ABSTRACT:
		
DETAILS:

*******************************************************************************/

unsigned long SYSMEM_QueryMemory( int ifSram )
{
	UINT32	remaining_mem;
	 
	switch( ifSram )
    {
		case 0:
			// SDRAM
			remaining_mem = ((DRAM_BASE + DRAM_8M_SIZE) - nonCachedSDRAMMemTopPtr);
			printk( KERN_DEBUG "QueryMem SDRAM ");
			break;
		case 1:
			// SRAM
			remaining_mem = ( SRAMEND - SRAMMemTopPtr);
			printk( KERN_DEBUG "QueryMem SRAM ");
			break;

		default:
			remaining_mem = 0;
			break;
	}
	printk( KERN_DEBUG "%lx\n", remaining_mem );
	return remaining_mem;
}


