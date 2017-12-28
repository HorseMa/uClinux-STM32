/*
 * MTD driver for the ColdFire Flash Module on MCF5282.
 *
 * $Id:$
 *
 * Author: Mathias Küster
 *
 * Copyright (c) 2004, Mathias Küster
 *
 * Some parts are based on lart.c by Abraham vd Merwe
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

/* debugging */
//#define CFM_DEBUG

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/errno.h>
#include <linux/mtd/mtd.h>
#include <linux/autoconf.h>
#ifdef CONFIG_MTD_PARTITIONS
#include <linux/mtd/partitions.h>
#endif

#ifndef CONFIG_M5282
#error This is for MCF5282 architecture only
#endif

static char module_name[] = "CFM";

/*
 * Flash memory blocksize, blockcount and offset.
 */
#define FLASH_BLOCKSIZE		(1024*2)
#define FLASH_NUMBLOCKS		256
#define FLASH_OFFSET		0x00000000

/*
 * General settings, FLASH_OFFSET based on FLASHBAR chip select
 */
#define BUSWIDTH		4
#ifdef CONFIG_MTD_CFM_OFFSET
#define CFM_OFFSET		CONFIG_MTD_CFM_OFFSET
#else
#define CFM_OFFSET		0xF0000000
#endif
#define CFM_BACKDOOR_OFFSET	(MCF_IPSBAR+0x04000000)

/*
 * CFM User Mode Commands
 */
#define CFMCMD_RDARY1	0x05	/* erase verify */
#define CFMCMD_PGM	0x20	/* longword program */
#define CFMCMD_PGERS	0x40	/* page erase */
#define CFMCMD_MASERS	0x41	/* mass erase */
#define CFMCMD_PGERSVER	0x06	/* page erase verify */

/*
 * CFM Status Bits
 */
#define CFMUSTAT_CBEIF	0x80
#define CFMUSTAT_CCIF	0x40
#define CFMUSTAT_PVIOL	0x20
#define CFMUSTAT_ACCERR	0x10
#define CFMUSTAT_BLANK	0x04

/*
 * CFM configuration fields (sector 0)
 */
#define CFMCFG_BDKEY	0x0400	/* back door comparison key (8 byte) */
#define CFMCFG_PROT	0x0408	/* flash program/erase sector protection (4 byte) */
#define CFMCFG_SACC	0x040C	/* flash supervisor/user space restrictions (4 byte) */
#define CFMCFG_DACC	0x0410	/* flash program/data space restrictions (4 byte) */
#define CFMCFG_MSEC	0x0414	/* flash security longword (4 byte) */

/***************************************************************************************************/

/*
 * Reset CFM state register error bits
 */
void reset_state (void)
{
   if ( *(volatile unsigned char*) (MCF_IPSBAR+MCF5282_CFM_USTAT) & CFMUSTAT_PVIOL )
		*(volatile unsigned char*) (MCF_IPSBAR+MCF5282_CFM_USTAT) |= CFMUSTAT_PVIOL;
   if ( *(volatile unsigned char*) (MCF_IPSBAR+MCF5282_CFM_USTAT) & CFMUSTAT_ACCERR )
		*(volatile unsigned char*) (MCF_IPSBAR+MCF5282_CFM_USTAT) |= CFMUSTAT_ACCERR;
   if ( *(volatile unsigned char*) (MCF_IPSBAR+MCF5282_CFM_USTAT) & CFMUSTAT_BLANK )
		*(volatile unsigned char*) (MCF_IPSBAR+MCF5282_CFM_USTAT) |= CFMUSTAT_BLANK;
}

/*
 * Erase one block of flash memory at offset ``offset'' which is any
 * address within the block which should be erased.
 *
 * Returns 1 if successful, 0 otherwise.
 */
static inline int erase_block (__u32 offset)
{
   __u32 status = 1;

#ifdef CFM_DEBUG
   printk (KERN_DEBUG "%s(): 0x%.8x\n",__FUNCTION__,offset);
#endif

   /* start sector erase sequence */
   *(volatile __u32*) (CFM_BACKDOOR_OFFSET+offset) = 0;
   *(volatile __u8*) (MCF_IPSBAR+MCF5282_CFM_CMD) = CFMCMD_PGERS;
   *(volatile __u8*) (MCF_IPSBAR+MCF5282_CFM_USTAT) = CFMUSTAT_CBEIF;
   
   /* wait for ready */
   while ( ! (*(volatile __u8*) (MCF_IPSBAR+MCF5282_CFM_USTAT) & CFMUSTAT_CCIF ) );
   
   /* error handling */
   if ( *(volatile __u8*) (MCF_IPSBAR+MCF5282_CFM_USTAT) & CFMUSTAT_ACCERR )
   	{
   		printk( KERN_INFO "%s(): ACCERR\n",__FUNCTION__);
		status = 0;
   	}
   if ( *(volatile __u8*) (MCF_IPSBAR+MCF5282_CFM_USTAT) & CFMUSTAT_PVIOL )
   	{
   		printk( KERN_INFO "%s(): PVIOL\n",__FUNCTION__);
		status = 0;
   	}

   /* reset status flags */
   reset_state();

   return (status);
}

/*
 * Erase flash.
 *
 * Returns 1 if successful, 0 otherwise.
 */
static inline int erase_flash (int page)
{
   __u32 status = 1;

#ifdef CFM_DEBUG
   printk (KERN_DEBUG "%s(): page %d\n",__FUNCTION__,page);
#endif

   if ( (page < 0) || (page > 1) )
   	 return (0);

   /* start sector erase sequence */
   *(volatile __u32*) (CFM_BACKDOOR_OFFSET+(page*256*1024)) = 0x00000000;
   *(volatile __u8*) (MCF_IPSBAR+MCF5282_CFM_CMD) = CFMCMD_MASERS;
   *(volatile __u8*) (MCF_IPSBAR+MCF5282_CFM_USTAT) = CFMUSTAT_CBEIF;
   
   /* wait for ready */
   while ( ! (*(volatile __u8*) (MCF_IPSBAR+MCF5282_CFM_USTAT) & CFMUSTAT_CCIF ) );
   
   /* error handling */
   if ( *(volatile __u8*) (MCF_IPSBAR+MCF5282_CFM_USTAT) & CFMUSTAT_ACCERR )
   	{
   		printk( KERN_INFO "%s(): ACCERR\n",__FUNCTION__);
		status = 0;
   	}
   if ( *(volatile __u8*) (MCF_IPSBAR+MCF5282_CFM_USTAT) & CFMUSTAT_PVIOL )
   	{
   		printk( KERN_INFO "%s(): PVIOL\n",__FUNCTION__);
		status = 0;
   	}

   /* reset status flags */
   reset_state();

   return (status);
}

static int flash_erase (struct mtd_info *mtd,struct erase_info *instr)
{
   __u32 addr,len;
   int i,first;

#ifdef CFM_DEBUG
   printk (KERN_DEBUG "%s(addr = 0x%.8x, len = %d)\n",__FUNCTION__,instr->addr,instr->len);
#endif

   /* sanity checks */
   if (instr->addr + instr->len > mtd->size) return (-EINVAL);

   /*
	* check that both start and end of the requested erase are
	* aligned with the erasesize at the appropriate addresses.
	*
	* skip all erase regions which are ended before the start of
	* the requested erase. Actually, to save on the calculations,
	* we skip to the first erase region which starts after the
	* start of the requested erase, and then go back one.
	*/
   for (i = 0; i < mtd->numeraseregions && instr->addr >= mtd->eraseregions[i].offset; i++) ;
   i--;

   /*
	* ok, now i is pointing at the erase region in which this
	* erase request starts. Check the start of the requested
	* erase range is aligned with the erase size which is in
	* effect here.
	*/
   if (instr->addr & (mtd->eraseregions[i].erasesize - 1)) return (-EINVAL);

   /* Remember the erase region we start on */
   first = i;

   /*
	* next, check that the end of the requested erase is aligned
	* with the erase region at that address.
	*
	* as before, drop back one to point at the region in which
	* the address actually falls
	*/
   for (; i < mtd->numeraseregions && instr->addr + instr->len >= mtd->eraseregions[i].offset; i++) ;
   i--;

   /* is the end aligned on a block boundary? */
   if ((instr->addr + instr->len) & (mtd->eraseregions[i].erasesize - 1)) return (-EINVAL);

   addr = instr->addr;
   len = instr->len;

   i = first;

   /* if sector 0 erased, we erase the complete flash */
   if ( addr == FLASH_OFFSET )
	 {
	 	 if (!erase_flash(0))
		   {
			 instr->state = MTD_ERASE_FAILED;
			 return (-EIO);
		   }
	 
	 	 if (!erase_flash(1))
		   {
			 instr->state = MTD_ERASE_FAILED;
			 return (-EIO);
		   }
	 	 
		 len = 0;
	 }
   
   /* now erase those blocks */
   while (len)
	 {
		if (!erase_block (addr))
		  {
			 instr->state = MTD_ERASE_FAILED;
			 return (-EIO);
		  }

		addr += mtd->eraseregions[i].erasesize;
		len -= mtd->eraseregions[i].erasesize;

		if (addr == mtd->eraseregions[i].offset + (mtd->eraseregions[i].erasesize * mtd->eraseregions[i].numblocks)) i++;
	 }

   instr->state = MTD_ERASE_DONE;
   if (instr->callback) instr->callback (instr);

   return (0);
}

static int flash_read (struct mtd_info *mtd,loff_t from,size_t len,size_t *retlen,u_char *buf)
{
#ifdef CFM_DEBUG
   printk (KERN_DEBUG "%s(from = 0x%.8x, len = %d)\n",__FUNCTION__,(__u32) from,len);
#endif

   /* sanity checks */
   if (!len) return (0);
   if (from + len > mtd->size) return (-EINVAL);

   /* we always read len bytes */
   *retlen = len;

   memcpy(buf,((u_char*)(CFM_OFFSET))+from,len);
   
   return 0;
}

/*
 * Write one dword ``x'' to flash memory at offset ``offset''. ``offset''
 * must be 32 bits, i.e. it must be on a dword boundary.
 *
 * Returns 1 if successful, 0 otherwise.
 */
static inline int write_dword (__u32 offset,__u32 x)
{
   __u32 status = 1;

#ifdef CFM_DEBUG
   printk (KERN_DEBUG "%s(): 0x%.8x <- 0x%.8x\n",__FUNCTION__,offset,x);
#endif

   /* start program sequence */
   *(volatile __u32*) (CFM_BACKDOOR_OFFSET+offset) = x;
   *(volatile __u8*) (MCF_IPSBAR+MCF5282_CFM_CMD) = CFMCMD_PGM;
   *(volatile __u8*) (MCF_IPSBAR+MCF5282_CFM_USTAT) = CFMUSTAT_CBEIF;

   /* wait for ready */
   while ( ! (*(volatile __u8*) (MCF_IPSBAR+MCF5282_CFM_USTAT) & CFMUSTAT_CCIF ) );
   
   /* error handling */
   if ( *(volatile __u8*) (MCF_IPSBAR+MCF5282_CFM_USTAT) & CFMUSTAT_ACCERR )
   	{
   		printk( KERN_INFO "%s(): ACCERR\n",__FUNCTION__);
		status = 0;
   	}
   if ( *(volatile __u8*) (MCF_IPSBAR+MCF5282_CFM_USTAT) & CFMUSTAT_PVIOL )
   	{
   		printk( KERN_INFO "%s(): PVIOL\n",__FUNCTION__);
		status = 0;
   	}

   /* reset status flags */
   reset_state();
   
   return status;
}

static int flash_write (struct mtd_info *mtd,loff_t to,size_t len,size_t *retlen,const u_char *buf)
{
   __u8 tmp[4];
   int i,n;

#ifdef CFM_DEBUG
   printk (KERN_DEBUG "%s(to = 0x%.8x, len = %d)\n",__FUNCTION__,(__u32) to,len);
#endif

   *retlen = 0;

   /* sanity checks */
   if (!len) return (0);
   if (to + len > mtd->size) return (-EINVAL);

   /* first, we write a 0xFF.... padded byte until we reach a dword boundary */
   if (to & (BUSWIDTH - 1))
	 {
		__u32 aligned = to & ~(BUSWIDTH - 1);
		int gap = to - aligned;

		i = n = 0;

		while (gap--) tmp[i++] = 0xFF;
		while (len && i < BUSWIDTH) tmp[i++] = buf[n++], len--;
		while (i < BUSWIDTH) tmp[i++] = 0xFF;

		if (!write_dword (aligned,*((__u32 *) tmp))) return (-EIO);

		to += n;
		buf += n;
		*retlen += n;
	 }

   /* now we write dwords until we reach a non-dword boundary */
   while (len >= BUSWIDTH)
	 {
		if (!write_dword (to,*((__u32 *) buf))) return (-EIO);

		to += BUSWIDTH;
		buf += BUSWIDTH;
		*retlen += BUSWIDTH;
		len -= BUSWIDTH;
	 }

   /* top up the last unaligned bytes, padded with 0xFF.... */
   if (len & (BUSWIDTH - 1))
	 {
		i = n = 0;

		while (len--) tmp[i++] = buf[n++];
		while (i < BUSWIDTH) tmp[i++] = 0xFF;

		if (!write_dword (to,*((__u32 *) tmp))) return (-EIO);

		*retlen += n;
	 }

   return (0);
}

/***************************************************************************************************/

#define NB_OF(x) (sizeof (x) / sizeof (x[0]))

static struct mtd_info mtd;

static struct mtd_erase_region_info erase_regions[] =
{
   /* parameter blocks cfmsec */
   {
	     offset: FLASH_OFFSET,
	  erasesize: FLASH_BLOCKSIZE,
	  numblocks: 1
   },
   /* parameter blocks cfm */
   {
	     offset: FLASH_OFFSET+FLASH_BLOCKSIZE,
	  erasesize: FLASH_BLOCKSIZE,
	  numblocks: (FLASH_NUMBLOCKS-1)
   }
};

#ifdef CONFIG_MTD_PARTITIONS
static struct mtd_partition cfm_partitions[] =
{
   /* cfmsec */
   {
	       name: "cfmsec",
	     offset: FLASH_OFFSET,
	       size: FLASH_BLOCKSIZE,
	 mask_flags: 0
   },
   /* cfm */
   {
	       name: "cfm",
	     offset: FLASH_OFFSET+FLASH_BLOCKSIZE,
	       size: FLASH_BLOCKSIZE * (FLASH_NUMBLOCKS-1),
	 mask_flags: 0
   }
};
#endif

void flash_init (void)
{
   reset_state();

   *(volatile unsigned short*)(MCF_IPSBAR+MCF5282_CFM_MCR)  = 0;
   /* 200KHz @ 64MHz sysclk */
   *(volatile unsigned char*) (MCF_IPSBAR+MCF5282_CFM_CLKD) = (1<<6) | 20;
   /* disable protection */
   *(volatile unsigned long*) (MCF_IPSBAR+MCF5282_CFM_PROT) = 0;
   /* disable supervisor mode */
   *(volatile unsigned long*) (MCF_IPSBAR+MCF5282_CFM_SACC) = 0;
   /* data and program address space */
   *(volatile unsigned long*) (MCF_IPSBAR+MCF5282_CFM_DACC) = 0;
}

int __init cfm_flash_init (void)
{
   int result;
   uint32_t keyhi,keylo;
   memset (&mtd,0,sizeof (mtd));
   printk ("MTD driver for ColdFire Flash Module. Written by Mathias Küster\n");
   flash_init();

   if ( (*(volatile unsigned long*) (MCF_IPSBAR+MCF5282_CFM_SEC) & 0x80000000) )
	 {
		printk ("CFM: Warning, backdoor mode enabled. (%x)\n",*(volatile unsigned long*) (MCF_IPSBAR+MCF5282_CFM_SEC));
	
		keyhi = *(volatile unsigned long*) (CFM_OFFSET + CFMCFG_BDKEY);
		keylo = *(volatile unsigned long*) (CFM_OFFSET + CFMCFG_BDKEY+4);

		*(volatile unsigned short*)(MCF_IPSBAR+MCF5282_CFM_MCR) |= (1<<5);
		*(volatile unsigned long*) (CFM_BACKDOOR_OFFSET + CFMCFG_BDKEY) = keyhi;
		*(volatile unsigned long*) (CFM_BACKDOOR_OFFSET + CFMCFG_BDKEY+4) = keylo;
		*(volatile unsigned short*)(MCF_IPSBAR+MCF5282_CFM_MCR) &= ~(1<<5);
	 }
   
   if ( (*(volatile unsigned long*) (MCF_IPSBAR+MCF5282_CFM_SEC) & 0x40000000) )
   	printk ("CFM: Warning, security mode enabled. %x\n",*(volatile unsigned long*) (MCF_IPSBAR+MCF5282_CFM_SEC));

#ifdef CFM_DEBUG
   printk("MCR : %04X\n",*(volatile unsigned short*)(MCF_IPSBAR+MCF5282_CFM_MCR));
   printk("PROT: %08X\n",*(volatile unsigned long*) (MCF_IPSBAR+MCF5282_CFM_PROT));
   printk("SEC : %08X\n",*(volatile unsigned long*) (MCF_IPSBAR+MCF5282_CFM_SEC));
   printk("SACC: %08X\n",*(volatile unsigned long*) (MCF_IPSBAR+MCF5282_CFM_SACC));
   printk("DACC: %08X\n",*(volatile unsigned long*) (MCF_IPSBAR+MCF5282_CFM_DACC));
#endif

   mtd.name = module_name;
   mtd.type = MTD_NORFLASH;
   mtd.flags = MTD_CAP_NORFLASH;
   mtd.size = FLASH_BLOCKSIZE * FLASH_NUMBLOCKS;
   mtd.erasesize = FLASH_BLOCKSIZE;
   mtd.numeraseregions = NB_OF (erase_regions);
   mtd.eraseregions = erase_regions;
   mtd.module = THIS_MODULE;
   mtd.erase = flash_erase;
   mtd.read = flash_read;
   mtd.write = flash_write;

#ifdef CFM_DEBUG
   printk (KERN_DEBUG
		   "mtd.name = %s\n"
		   "mtd.size = 0x%.8x (%uM)\n"
		   "mtd.erasesize = 0x%.8x (%uK)\n"
		   "mtd.numeraseregions = %d\n",
		   mtd.name,
		   mtd.size,mtd.size / (1024*1024),
		   mtd.erasesize,mtd.erasesize / 1024,
		   mtd.numeraseregions);

   if (mtd.numeraseregions)
	 for (result = 0; result < mtd.numeraseregions; result++)
	   printk (KERN_DEBUG
			   "\n\n"
			   "mtd.eraseregions[%d].offset = 0x%.8x\n"
			   "mtd.eraseregions[%d].erasesize = 0x%.8x (%uK)\n"
			   "mtd.eraseregions[%d].numblocks = %d\n",
			   result,mtd.eraseregions[result].offset,
			   result,mtd.eraseregions[result].erasesize,mtd.eraseregions[result].erasesize / 1024,
			   result,mtd.eraseregions[result].numblocks);

#ifdef CONFIG_MTD_PARTITIONS
   printk ("\npartitions = %d\n",NB_OF (cfm_partitions));

   for (result = 0; result < NB_OF (cfm_partitions); result++)
	 printk (KERN_DEBUG
			 "\n\n"
			 "cfm_partitions[%d].name = %s\n"
			 "cfm_partitions[%d].offset = 0x%.8x\n"
			 "cfm_partitions[%d].size = 0x%.8x (%uK)\n",
			 result,cfm_partitions[result].name,
			 result,cfm_partitions[result].offset,
			 result,cfm_partitions[result].size,cfm_partitions[result].size / 1024);
#endif
#endif

#ifndef CONFIG_MTD_PARTITIONS
   result = add_mtd_device (&mtd);
#else
   result = add_mtd_partitions (&mtd,cfm_partitions,NB_OF (cfm_partitions));
#endif

   return (result);
}

void __exit cfm_flash_exit (void)
{
#ifndef CONFIG_MTD_PARTITIONS
   del_mtd_device (&mtd);
#else
   del_mtd_partitions (&mtd);
#endif
}

module_init (cfm_flash_init);
module_exit (cfm_flash_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mathias Küster");
MODULE_DESCRIPTION("MTD driver for ColdFire Flash Module");
