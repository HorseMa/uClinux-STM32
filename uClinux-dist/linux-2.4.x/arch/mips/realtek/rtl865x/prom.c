/*
 *  arch/mips/philips/nino/prom.c
 *
 *  Copyright (C) 2001 Steven J. Hill (sjhill@realitydiluted.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *  
 *  Early initialization code for the Philips Nino
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <asm/bootinfo.h>
#include <asm/addrspace.h>
#include <asm/page.h>
#include "rtl_types.h"
#include "asicRegs.h"

#include <linux/mtd/mtd.h>

char arcs_cmdline[CL_SIZE];

#ifdef CONFIG_FB_TX3912
extern unsigned long tx3912fb_paddr;
extern unsigned long tx3912fb_vaddr;
extern unsigned long tx3912fb_size;
#endif

const char *get_system_type(void)
{
	return "Philips Nino";
}

/* Do basic initialization */
void __init prom_init(int argc, char **argv, unsigned long magic, int *prom_vec)
{
	unsigned long mem_size;
#ifdef CONFIG_MTD
#ifdef FLASHMTD_FOR_7MTDP
	strcpy(arcs_cmdline, "root=/dev/mtdblock5");
#elif !defined(CONFIG_MTD_NETteluC)
	strcpy(arcs_cmdline, "root=/dev/mtdblock4");
#endif
#endif
#ifdef CONFIG_CMDLINE_BOOL
	strcpy(arcs_cmdline, CONFIG_CMDLINE);
#endif
	mips_machgroup = MACH_GROUP_PHILIPS;
	mips_machtype = MACH_PHILIPS_NINO;

#if 1 /* read MCR to get SDRAM size*/
	/* read SDRAM size */
	{
		unsigned int MCRsdram;

#ifdef CONFIG_RTL865XC
		switch ( MCRsdram = ( REG32( MCR ) & 0x1C100010 ) )
		{
			/* SDRAM 16-bit mode */
			case 0x00000000: mem_size =  2<<20; break;
			case 0x04000000: mem_size =  4<<20; break;
			case 0x08000000: mem_size =  8<<20; break;
			case 0x0C000000: mem_size = 16<<20; break;
			case 0x10000000: mem_size = 32<<20; break;
			case 0x14000000: mem_size = 64<<20; break;

			/* SDRAM 16-bit mode - 2 chip select */
			case 0x00000010: mem_size =  4<<20; break;
			case 0x04000010: mem_size =  8<<20; break;
			case 0x08000010: mem_size = 16<<20; break;
			case 0x0C000010: mem_size = 32<<20; break;
			case 0x10000010: mem_size = 64<<20; break;
			case 0x14000010: mem_size = 128<<20; break;

			/* SDRAM 32-bit mode */
			case 0x00100000: mem_size =  4<<20; break;
			case 0x04100000: mem_size =  8<<20; break;
			case 0x08100000: mem_size = 16<<20; break;
			case 0x0C100000: mem_size = 32<<20; break;
			case 0x10100000: mem_size = 64<<20; break;
			case 0x14100000: mem_size =128<<20; break;

			/* SDRAM 32-bit mode - 2 chip select */
			case 0x00100010: mem_size =  8<<20; break;
			case 0x04100010: mem_size = 16<<20; break;
			case 0x08100010: mem_size = 32<<20; break;
			case 0x0C100010: mem_size = 64<<20; break;
			case 0x10100010: mem_size =128<<20; break;
			/*
			case 0x14100010: mem_size =256<<20; break;
			*/

			default:
				printk( "SDRAM unknown(0x%08X)", MCRsdram ); 
				mem_size = 0;
				break;
		}
#else
		switch ( MCRsdram = ( REG32( MCR ) & 0x30100800 ) )
		{
			/* SDRAM 16-bit mode */
			case 0x00000000: mem_size =  2<<20; break;
			case 0x10000000: mem_size =  8<<20; break;
			case 0x20000000: mem_size = 16<<20; break;
			case 0x30000000: mem_size = 32<<20; break;
			case 0x30000800: mem_size = 64<<20; break;

			/* SDRAM 32-bit mode */
			case 0x00100000: mem_size =  4<<20; break;
			case 0x10100000: mem_size = 16<<20; break;
			case 0x20100000: mem_size = 32<<20; break;
			case 0x30100000: mem_size = 64<<20; break;
			case 0x30100800: mem_size =128<<20; break;

			case 0x00000800: /* fall thru */
			case 0x10000800: /* fall thru */
			case 0x20000800: /* fall thru */
			default:
				printk( "SDRAM unknown(0x%08X)", MCRsdram ); 
				mem_size = 0;
				break;
		}
#endif
		printk( "SDRAM size: %dMB\n", mem_size>>20 );
	}
#else
#ifdef CONFIG_RTL865X_SDRAM_TOTAL_8MB
	mem_size = 8 << 20;
#elif defined(CONFIG_RTL865X_SDRAM_TOTAL_16MB)
	mem_size = 16 << 20;
#elif defined(CONFIG_RTL865X_SDRAM_TOTAL_32MB)
	mem_size = 32 << 20;
#elif defined(CONFIG_RTL865X_SDRAM_TOTAL_64MB)
	mem_size = 64 << 20;
#else /* default */
	mem_size = 16 << 20;
#endif
#endif/*0*/
	
	add_memory_region(0, mem_size, BOOT_MEM_RAM); 
}

void __init prom_free_prom_memory (void)
{
}
