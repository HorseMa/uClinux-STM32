/****************************************************************************
* $Header:   S:/Embedded/archives/Linux/BSP_CX821xx/uClinux2.4.6/linux/include/asm-armnommu/arch-cx821xx/sysmem.h-arc   1.0   Mar 19 2003 13:49:10   davidsdj  $
*
*  Copyright(c)2002  Conexant Systems
*
*****************************************************************************/
#ifndef SYSMEM_H_
#define SYSMEM_H_

// SYSMEM: Unformatted system (nonCache) memory (SRAM or SDRAM) manager
// This module provide the services to allocate and free a block of system memory.
//  The services are slow, the idea is client allocate a block of memory and 
// manage it by itself.
// Depend on the implementation it can be OS_DEPENDENT or OS_INDEPENDENT
// 


// Set up memory pool
void SYSMEM_GeneralInit(void) ;

// Free all allocated memory back to system 
void SYSMEM_GeneralClose(void) ;

// the allocate memory will be at Hardware required alignment
// If the memory is in SRAM, set ifSram = 1.
// Otherwise set ifSram = 0.
// if alignment =0, set alignment = 16
unsigned char * SYSMEM_Allocate_BlockMemory( int ifSram, int sysMemSize, int alignment ) ;

int SYSMEM_Release_BlockMemory( int ifSram, unsigned char *sysMemPtr ) ;

// report the biggest continuous memory ( in SRAM or SDRAM)
unsigned long SYSMEM_QueryMemory( int ifSRAM ) ;

#endif
