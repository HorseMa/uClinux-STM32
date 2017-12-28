/*
 *  linux/arch/microblaze/mm/memory.c
 *
 */

#include <linux/config.h>
#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/slab.h>

/* Map some physical address range into the kernel address space. The
 * code is copied and adapted from map_chunk().
 */

unsigned long kernel_map(unsigned long paddr, unsigned long size,
			 int nocacheflag, unsigned long *memavailp )
{
	return paddr;
}


/* Fake the is_in_rom test by saying anything *NOT* in main RAM space
   must be ROM */
int is_in_rom (unsigned long addr)
{
	return (addr < (unsigned long)CONFIG_XILINX_ERAM_START) || 
		(addr >= (unsigned long)(CONFIG_XILINX_ERAM_START + CONFIG_XILINX_ERAM_SIZE));
}

